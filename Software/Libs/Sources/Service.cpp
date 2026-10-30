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
                    silence();
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
    silence();                                     // nothing may keep sounding after the game
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

void Service::playSfx(uint32_t kind)
{
    if (kind == 0 || kind > static_cast<uint32_t>(Pp::Sfx::WIN)) {
        return;                                    // trust boundary: unknown cue
    }
    const Pp::Plan p = mAudio.play(static_cast<Pp::Sfx>(kind), mKernel.sys.getTimeMs());

    if (p.vibro) {
        if (auto* v = mKernel.comm.allocateMessage<SDK::Message::RequestVibroPlay>()) {
            v->notes[0].effect = p.vibro;
            v->notesCount = 1;
            mKernel.comm.sendMessage(v);
            mKernel.comm.releaseMessage(v);
        }
    }
    if (p.noteCount) {
        if (auto* b = mKernel.comm.allocateMessage<SDK::Message::RequestBuzzerPlay>()) {
            for (uint32_t i = 0; i < p.noteCount; ++i) {
                b->notes[i].volume    = p.notes[i].vol;
                b->notes[i].time      = p.notes[i].ms;
                b->notes[i].frequency = p.notes[i].hz;
            }
            b->notesCount = p.noteCount;
            mKernel.comm.sendMessage(b);
            mKernel.comm.releaseMessage(b);
        }
    }
}

void Service::silence()
{
    // Empty patterns cancel whatever is playing (as the SDK's Alarm and Timer do).
    if (auto* b = mKernel.comm.allocateMessage<SDK::Message::RequestBuzzerPlay>()) {
        mKernel.comm.sendMessage(b);
        mKernel.comm.releaseMessage(b);
    }
    if (auto* v = mKernel.comm.allocateMessage<SDK::Message::RequestVibroPlay>()) {
        mKernel.comm.sendMessage(v);
        mKernel.comm.releaseMessage(v);
    }
    mAudio.stop();
}