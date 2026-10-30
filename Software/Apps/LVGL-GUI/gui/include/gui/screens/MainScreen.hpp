#ifndef MAIN_SCREEN_HPP
#define MAIN_SCREEN_HPP

#include <cstdint>

#include "lvgl.h"

#include "Game.hpp"
#include "gui/model/Model.hpp"
#include "gui/model/ModelListener.hpp"

/// One full-screen object whose DRAW_MAIN paints the whole game, pixel by
/// pixel, straight into LVGL's render stripe.
class MainScreen : public ModelListener
{
public:
    explicit MainScreen(Model& model);
    ~MainScreen();

    MainScreen(const MainScreen&)            = delete;
    MainScreen& operator=(const MainScreen&) = delete;

    lv_obj_t* root() const { return mRoot; }

    void appSuspended() override;

private:
    void onKey(uint8_t code);
    void onTick();
    void play(Pp::Sfx s);
    void onDraw(lv_event_t* e);

    static void keyEventCb(lv_event_t* e);
    static void drawCb(lv_event_t* e);
    static void tickCb(lv_timer_t* t);

    Model&      mModel;
    lv_obj_t*   mRoot   = nullptr;
    lv_obj_t*   mCanvas = nullptr;
    lv_timer_t* mTimer  = nullptr;

    Pp::Game mGame;
    bool     mR2Down    = false;
    bool     mHoldFired = false;
    uint32_t mR2At      = 0;
    bool     mPaintedStill = false;   ///< the static pause screen is already on display
};

#endif
