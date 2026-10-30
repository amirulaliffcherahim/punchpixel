/**
 ******************************************************************************
 * @file    Audio.hpp
 * @brief   Decides what the piezo and vibration motor actually play.
 *
 * The game is fast; the hardware must stay gentle:
 *
 *  - The buzzer sounds for a critical hit (1 in 10 jabs) only, as a single
 *    short tone followed by a silent note so the piezo is always left off.
 *  - Button presses and weaves do not vibrate; only landed punches, dodges
 *    and round moments do, each as one short single-shot click/tick, at least
 *    kMinVibroGapMs apart.
 *  - stop() silences both drivers on suspend and exit.
 *
 * Pure logic, no kernel: the service turns a Plan into kernel messages.
 ******************************************************************************
 */

#ifndef PP_AUDIO_HPP
#define PP_AUDIO_HPP

#include <cstdint>

#include "Game.hpp"

namespace Pp
{

constexpr uint16_t kMinHz          = 500;
constexpr uint16_t kMaxHz          = 4000;
constexpr uint8_t  kMaxVolume      = 33;     ///< softest audible step
constexpr uint16_t kTailRestMs     = 30;     ///< silent note closing every pattern
constexpr uint32_t kMinBuzzerGapMs = 600;    ///< between crit beeps
constexpr uint32_t kMinVibroGapMs  = 140;    ///< between motor ticks (one per punch)
constexpr uint32_t kMaxNotes       = 4;

struct Note
{
    uint16_t hz  = 0;
    uint16_t ms  = 0;
    uint8_t  vol = 0;      ///< 0 = rest
};

struct Plan
{
    uint8_t  vibro     = 0;     ///< haptic effect id, 0 = leave the motor alone
    uint32_t noteCount = 0;     ///< 0 = leave the buzzer alone
    Note     notes[kMaxNotes];
};

class Audio
{
public:
    Plan play(Sfx s, uint32_t nowMs);
    void stop() { mBuzzerFreeAt = mBuzzerLastStart = mVibroFreeAt = 0; mVibroPrio = 0; mStarted = false; }
    static uint32_t patternMs(const Plan& p);

private:
    uint32_t mBuzzerFreeAt    = 0;
    uint32_t mBuzzerLastStart = 0;
    uint32_t mVibroFreeAt     = 0;
    uint8_t  mVibroPrio       = 0;
    bool     mStarted         = false;
};

} // namespace Pp

#endif