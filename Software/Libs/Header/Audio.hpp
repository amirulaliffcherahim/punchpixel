/**
 ******************************************************************************
 * @file    Audio.hpp
 * @brief   Decides what the piezo and vibration motor actually play.
 *
 * The game asks for a cue on almost every hit; the hardware must not get one
 * request per hit. This scheduler keeps the drivers gentle:
 *
 *  - The buzzer is never re-triggered while a pattern is still sounding (a
 *    pattern started on top of another one could leave the piezo driven).
 *    A cue that arrives while it is busy plays haptics only.
 *  - Every pattern ends on an explicit silent note, so the piezo is always
 *    left off, and starts are at least kMinBuzzerGapMs apart.
 *  - Pitches stay inside the SDK's 500-20000 Hz range (clamped to 500-4000),
 *    and volume is capped at 66 %.
 *  - A long vibration effect is only cut short by a more important cue.
 *  - stop() silences both, for exit and suspend.
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

constexpr uint16_t kMinHz           = 500;
constexpr uint16_t kMaxHz           = 4000;
constexpr uint8_t  kMaxVolume       = 66;
constexpr uint16_t kTailRestMs      = 30;    ///< silent note closing every pattern
constexpr uint32_t kMinBuzzerGapMs  = 90;    ///< between buzzer pattern starts
constexpr uint32_t kMaxNotes        = 6;     ///< kernel allows 10

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
    /// What to play for cue @p s at @p nowMs.
    Plan play(Sfx s, uint32_t nowMs);

    /// Forget what is playing (after the caller silenced both drivers).
    void stop() { mBuzzerFreeAt = mVibroFreeAt = 0; mVibroPrio = 0; mStarted = false; }

    /// Total length of a plan's buzzer pattern, rests included.
    static uint32_t patternMs(const Plan& p);

private:
    uint32_t mBuzzerFreeAt = 0;
    uint32_t mBuzzerLastStart = 0;
    uint32_t mVibroFreeAt  = 0;
    uint8_t  mVibroPrio    = 0;
    bool     mStarted      = false;
};

} // namespace Pp

#endif
