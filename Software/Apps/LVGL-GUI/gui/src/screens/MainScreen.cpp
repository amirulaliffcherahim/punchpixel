#include "gui/screens/MainScreen.hpp"

#include "SDK/GUI/Button.hpp"
#include "SDK/GUI/LVGL/Draw.hpp"
#include "SDK/Port/GuiCommandProcessor.hpp"

#include "Painter.hpp"
#include "Scene.hpp"

#if LV_USE_OS != LV_OS_NONE || LV_DRAW_SW_DRAW_UNIT_CNT != 1
#error "MainScreen paints into the render buffer during DRAW_MAIN; that needs LVGL's single-threaded software renderer"
#endif

namespace Draw = SDK::LVGL::Draw;
namespace Key  = SDK::GUI::Button;

namespace
{
constexpr uint32_t kFrameMs = 10;    // fires once per kernel frame (10 Hz on the watch)
constexpr uint32_t kHoldMs  = 900;   // hold R2 this long: pause / quit
} // namespace

MainScreen::MainScreen(Model& model)
    : mModel(model)
    , mGame(model.nowMs() | 1u)
{
    mRoot = lv_obj_create(nullptr);
    Draw::applyScreen(mRoot);
    lv_obj_add_event_cb(mRoot, &MainScreen::keyEventCb, LV_EVENT_KEY, this);

    mCanvas = Draw::container(mRoot, 0, 0, Pp::kScreen, Pp::kScreen);
    lv_obj_remove_flag(mCanvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(mCanvas, &MainScreen::drawCb, LV_EVENT_DRAW_MAIN, this);

    bind(&mModel);
    mModel.bind(this);

    mTimer = lv_timer_create(&MainScreen::tickCb, kFrameMs, this);
}

MainScreen::~MainScreen()
{
    if (mTimer) {
        lv_timer_delete(mTimer);
    }
    mModel.bind(nullptr);
    lv_obj_delete(mRoot);
}

void MainScreen::keyEventCb(lv_event_t* e)
{
    auto* self = static_cast<MainScreen*>(lv_event_get_user_data(e));
    self->onKey(static_cast<uint8_t>(lv_event_get_key(e)));
}

void MainScreen::drawCb(lv_event_t* e)
{
    static_cast<MainScreen*>(lv_event_get_user_data(e))->onDraw(e);
}

void MainScreen::tickCb(lv_timer_t* t)
{
    static_cast<MainScreen*>(lv_timer_get_user_data(t))->onTick();
}

void MainScreen::appSuspended()
{
    mGame.pause();
    mR2Down = false;
}

void MainScreen::play(Pp::Sfx s)
{
    if (s != Pp::Sfx::NONE) {
        mModel.sfx(static_cast<uint32_t>(s));
    }
}

void MainScreen::onKey(uint8_t code)
{
    // Act on the press itself: a punch is thrown when the button goes down.
    Pp::Btn b;
    switch (code) {
        case Key::L1_PRESS: b = Pp::Btn::L1; break;
        case Key::L2_PRESS: b = Pp::Btn::L2; break;
        case Key::R1_PRESS: b = Pp::Btn::R1; break;
        case Key::R2_PRESS:
            b = Pp::Btn::R2;
            mR2Down = true;
            mHoldFired = false;
            mR2At = mModel.nowMs();
            break;
        case Key::R2_RELEASE:
            mR2Down = false;
            return;
        default:
            return;     // clicks and other releases carry nothing new
    }
    // Frames are 100 ms apart: bring the game clock up to the press, or two
    // quick weaves would look simultaneous and the second would be dropped.
    play(mGame.tick(mModel.nowMs()));
    play(mGame.press(b));
    if (mGame.exitRequested) {
        mModel.exitApp();
    }
}

void MainScreen::onTick()
{
    // The port hands the screen one key per frame; at 10 frames a second a
    // fast weave (press, release, click per tap) would lag by a third of a
    // second. Drain whatever else is queued now.
    uint8_t code = 0;
    auto& pump = SDK::GuiCommandProcessor::GetInstance();
    while (pump.getKeySample(code)) {
        onKey(code);
        if (mGame.exitRequested) {
            return;
        }
    }

    const uint32_t now = mModel.nowMs();
    if (mR2Down && !mHoldFired && now - mR2At >= kHoldMs) {
        mHoldFired = true;
        play(mGame.press(Pp::Btn::CHORD));
        if (mGame.exitRequested) {
            mModel.exitApp();
            return;
        }
    }

    play(mGame.tick(now));
    lv_obj_invalidate(mCanvas);
}

void MainScreen::onDraw(lv_event_t* e)
{
    lv_layer_t* layer = lv_event_get_layer(e);
    if (!layer || layer->color_format != LV_COLOR_FORMAT_RGB565) {
        return;
    }
    // Single-threaded renderer: every draw task added before this event has
    // already run, so writing pixels here keeps the painter's order.
    if (!lv_draw_layer_alloc_buf(layer)) {
        return;
    }
    const lv_area_t& a = layer->_clip_area;
    const lv_area_t& b = layer->buf_area;
    lv_area_t clip;
    clip.x1 = LV_MAX(a.x1, b.x1);
    clip.y1 = LV_MAX(a.y1, b.y1);
    clip.x2 = LV_MIN(a.x2, b.x2);
    clip.y2 = LV_MIN(a.y2, b.y2);
    if (clip.x1 > clip.x2 || clip.y1 > clip.y2) {
        return;
    }
    lv_draw_buf_t* db = layer->draw_buf;
    const int stride = static_cast<int>(db->header.stride / 2);
    auto* base = static_cast<uint16_t*>(lv_draw_layer_go_to_xy(layer, clip.x1 - layer->buf_area.x1,
                                                                clip.y1 - layer->buf_area.y1));
    Pp::Painter p(base, stride, clip.x1, clip.y1, lv_area_get_width(&clip), lv_area_get_height(&clip));
    Pp::drawScene(p, mGame, mModel.nowMs());
}
