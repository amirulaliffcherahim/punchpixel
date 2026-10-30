#include "Audio.hpp"

namespace Pp
{

namespace
{
struct Tone { uint16_t hz; uint16_t ms; };

struct Def
{
    uint8_t  vibro;      ///< RequestVibroPlay::Effect
    uint16_t vibroMs;    ///< rough length of that effect, for scheduling only
    uint8_t  volume;     ///< 33 / 66 (the kernel has four levels)
    Tone     tones[4];   ///< ms = 0 ends the list
};

// Indexed by Sfx. Same chiptune voicing as before, moved into the piezo's
// documented range: the low "thuds" now sit at 500-700 Hz.
const Def kDefs[] = {
    { 0,  0,   0,  { {0, 0} } },                                              // NONE
    { 24, 30,  33, { {1100, 25}, {0, 0} } },                                  // JAB
    { 4,  30,  33, { {1500, 30}, {0, 0} } },                                  // SELECT
    { 7,  40,  33, { {900, 25}, {1400, 35}, {0, 0} } },                       // DODGE
    { 1,  40,  33, { {520, 40}, {0, 0} } },                                   // BLOCK
    { 24, 30,  33, { {1300, 25}, {0, 0} } },                                  // ROLL_UP
    { 1,  40,  66, { {700, 35}, {520, 45}, {0, 0} } },                        // HIT
    { 14, 250, 66, { {600, 70}, {500, 90}, {0, 0} } },                        // HURT
    { 27, 120, 66, { {1000, 50}, {1500, 50}, {2000, 80}, {0, 0} } },          // ROLL_READY
    { 52, 600, 66, { {1800, 40}, {1400, 40}, {1800, 40}, {2200, 90} } },      // DEMPSEY
    { 15, 750, 66, { {900, 100}, {700, 110}, {500, 250}, {0, 0} } },          // KO
    { 37, 300, 66, { {1000, 80}, {1300, 80}, {1600, 80}, {2200, 220} } },     // WIN
};
constexpr uint32_t kDefCount = sizeof(kDefs) / sizeof(kDefs[0]);

/// Wrap-safe "a is at or after b" for millisecond clocks.
bool atOrAfter(uint32_t a, uint32_t b) { return static_cast<int32_t>(a - b) >= 0; }
} // namespace

uint32_t Audio::patternMs(const Plan& p)
{
    uint32_t ms = 0;
    for (uint32_t i = 0; i < p.noteCount; ++i) {
        ms += p.notes[i].ms;
    }
    return ms;
}

Plan Audio::play(Sfx s, uint32_t now)
{
    Plan p;
    const uint32_t idx = static_cast<uint32_t>(s);
    if (idx == 0 || idx >= kDefCount) {
        return p;
    }
    const Def& d = kDefs[idx];
    const uint8_t prio = static_cast<uint8_t>(idx);   // Sfx is ordered by importance

    // Haptics: a long effect is only cut short by something more important.
    if (d.vibro && (!mStarted || atOrAfter(now, mVibroFreeAt) || prio > mVibroPrio)) {
        p.vibro      = d.vibro;
        mVibroFreeAt = now + d.vibroMs;
        mVibroPrio   = prio;
    }

    // Buzzer: never on top of a pattern that is still sounding.
    const bool buzzerFree = !mStarted ||
                            (atOrAfter(now, mBuzzerFreeAt) &&
                             atOrAfter(now, mBuzzerLastStart + kMinBuzzerGapMs));
    if (buzzerFree && d.tones[0].ms) {
        const uint8_t vol = d.volume > kMaxVolume ? kMaxVolume : d.volume;
        for (const Tone& t : d.tones) {
            if (t.ms == 0 || p.noteCount + 1 >= kMaxNotes) {
                break;
            }
            Note& n = p.notes[p.noteCount++];
            n.hz  = t.hz < kMinHz ? kMinHz : (t.hz > kMaxHz ? kMaxHz : t.hz);
            n.ms  = t.ms;
            n.vol = vol;
        }
        Note& tail = p.notes[p.noteCount++];       // always end silent
        tail.ms = kTailRestMs;
        mBuzzerLastStart = now;
        mBuzzerFreeAt    = now + patternMs(p);
    }

    if (p.vibro || p.noteCount) {
        mStarted = true;
    }
    return p;
}

} // namespace Pp
