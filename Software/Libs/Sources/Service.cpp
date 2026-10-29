#include "SDK/Messages/SensorLayerMessages.hpp"

#include "Service.hpp"
#include "Commands.hpp"
#include "SDK/Messages/CommandMessages.hpp"

#define LOG_MODULE_PRX      "Service"
#define LOG_MODULE_LEVEL    LOG_LEVEL_INFO
#include "SDK/UnaLogger/Logger.h"

Service::Service(SDK::Kernel& kernel)
    : mKernel(SDK::KernelProviderService::GetInstance().getKernel())
    , mGUIStarted(false)
{}

void Service::run()
{
    LOG_INFO("thread started\n");

    while (true) {
        SDK::MessageBase *msg;
        if (mKernel.comm.getMessage(msg, 1000)) {
            // Command handling
            switch (msg->getType()) {
                // Kernel messages
                case SDK::MessageType::COMMAND_APP_STOP:
                    LOG_INFO("Force exit from the application\n");
                    // We must release message because this is the last event.
                    mKernel.comm.releaseMessage(msg);
                    return;

                case SDK::MessageType::COMMAND_APP_NOTIF_GUI_RUN:
                    LOG_INFO("GUI is now running\n");
                    onStartGUI();
                    break;

                case SDK::MessageType::COMMAND_APP_NOTIF_GUI_STOP:
                    LOG_INFO("GUI has stopped\n");
                    onStopGUI();
                    break;

                case CustomMessage::SFX:
                    playSfx(static_cast<CustomMessage::Sfx*>(msg)->kind);
                    break;

                default:
                    break;
            }

            // Release message after processing
            mKernel.comm.releaseMessage(msg);
        }
    }

    LOG_INFO("thread stopped\n");
}

void Service::onStartGUI()
{
    LOG_INFO("GUI started\n");
    mGUIStarted = true;
}

void Service::onStopGUI()
{
    LOG_INFO("GUI stopped\n");
    mGUIStarted = false;
}

uint32_t Service::ParseVersion(const char* str)
{
    if (str == nullptr) {
        return 0;
    }

    typedef union {
        struct {
            uint8_t patch;
            uint8_t minor;
            uint8_t major;
        }ver;
        uint32_t u32;
    } FirmwareVersion_t;

    FirmwareVersion_t v{};

    int major, minor, patch;

    if (sscanf(str, "%d.%d.%d", &major, &minor, &patch) == 3) {
        v.ver.major = static_cast<uint8_t>(major);
        v.ver.minor = static_cast<uint8_t>(minor);
        v.ver.patch = static_cast<uint8_t>(patch);
        return v.u32;
    }

    return 0;
}

namespace
{
struct Tone { uint16_t hz; uint16_t ms; };
struct SfxDef { uint8_t vibro; uint8_t volume; Tone tones[4]; };

// Indexed by Pp::Sfx (0 = NONE). Buzzer volume steps are 33 / 66 / 100.
const SfxDef kSfx[] = {
    { 0,  0,   { {0,0} } },                                                          // NONE
    { 24, 33,  { {1100, 25}, {0,0} } },                                              // JAB
    { 4,  33,  { {1500, 30}, {0,0} } },                                              // SELECT
    { 7,  33,  { {900, 25}, {1400, 35}, {0,0} } },                                   // DODGE
    { 1,  33,  { {300, 40}, {0,0} } },                                               // BLOCK
    { 24, 33,  { {1300, 25}, {0,0} } },                                              // ROLL_UP
    { 1,  66,  { {500, 35}, {250, 45}, {0,0} } },                                    // HIT
    { 14, 100, { {200, 70}, {130, 90}, {0,0} } },                                    // HURT
    { 27, 66,  { {1000, 50}, {1500, 50}, {2000, 80}, {0,0} } },                      // ROLL_READY
    { 52, 66,  { {1800, 40}, {1400, 40}, {1800, 40}, {2200, 90} } },                 // DEMPSEY
    { 15, 100, { {700, 100}, {500, 110}, {250, 250}, {0,0} } },                      // KO
    { 37, 66,  { {1000, 80}, {1300, 80}, {1600, 80}, {2200, 220} } },                // WIN
};
} // namespace

void Service::playSfx(uint32_t kind)
{
    using Vibro  = SDK::Message::RequestVibroPlay;
    using Buzzer = SDK::Message::RequestBuzzerPlay;

    if (kind == 0 || kind >= sizeof(kSfx) / sizeof(kSfx[0])) {
        return;
    }
    const SfxDef& d = kSfx[kind];

    if (auto* v = mKernel.comm.allocateMessage<Vibro>()) {
        v->notes[0].effect = d.vibro;
        v->notesCount = 1;
        mKernel.comm.sendMessage(v);
        mKernel.comm.releaseMessage(v);
    }
    if (auto* b = mKernel.comm.allocateMessage<Buzzer>()) {
        uint32_t n = 0;
        for (const Tone& t : d.tones) {
            if (t.ms == 0) {
                break;
            }
            b->notes[n].volume    = d.volume;
            b->notes[n].time      = t.ms;
            b->notes[n].frequency = t.hz;
            ++n;
        }
        b->notesCount = n;
        if (n) {
            mKernel.comm.sendMessage(b);
        }
        mKernel.comm.releaseMessage(b);
    }
}
