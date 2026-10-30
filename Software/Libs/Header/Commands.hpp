#pragma once

#include "SDK/Messages/MessageBase.hpp"
#include "SDK/Messages/MessageTypes.hpp"

#include <cstdint>

#pragma pack(push, 4)

namespace CustomMessage {

// GUI --> Service: only the Service may drive the buzzer and vibro motor.
constexpr SDK::MessageType::Type SFX = 0x00000001;

/// `kind` is a Pp::Sfx value (see Game.hpp); 0 (Sfx::NONE) silences buzzer and motor.
struct Sfx : public SDK::MessageBase {
    uint32_t kind;

    Sfx() : SDK::MessageBase(SFX), kind(0) {}
    explicit Sfx(uint32_t k) : Sfx() { kind = k; }
};

} // namespace CustomMessage

#pragma pack(pop)
