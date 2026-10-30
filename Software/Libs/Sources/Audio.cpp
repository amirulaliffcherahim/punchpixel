#include "Audio.hpp"

namespace Pp
{

namespace
{
struct Tone { uint16_t hz; uint16_t ms; };

struct Def
{
    uint8_t  vibro;      ///< RequestVibroPlay::Effect (0 = silent)
    uint16_t holdMs;     ///< how long before a lesser cue may tick again
    uint8_t  volume;     ///< 33 = softest step
    Tone     tones[2];
};

// Indexed by Sfx. Swinging (JAB) and weaving (ROLL_UP) do not vibrate: the
// tick happens when the punch lands (HIT / BLOCK / CRIT / HURT), so each punch
// is felt once, as a short single-shot click. Effects: 24 SHARP_TICK,
// 21 MEDIUM_CLICK, 17 STRONG_CLICK, 4 SHARP_CLICK.
const Def kDefs[] = {
    { 0,  0,   0,  { {0, 0} } },                        // NONE
    { 0,  0,   0,  { {0, 0} } },                        // JAB (silent until it lands)
    { 24, 80,  0,  { {0, 0} } },                        // SELECT
    { 24, 140, 0,  { {0, 0} } },                        // DODGE
    { 24, 140, 0,  { {0, 0} } },                        // BLOCK
    { 0,  0,   0,  { {0, 0} } },                        // ROLL_UP (weave is visual)
    { 24, 160, 0,  { {0, 0} } },                        // HIT: one crisp tick per landed punch
    { 21, 180, 66, { {0, 80}, {0, 0} } },               // CRIT: 1 note at kernel default pitch (0 Hz)
    { 21, 180, 0,  { {0, 0} } },                        // HURT
    { 4,  160, 0,  { {0, 0} } },                        // ROLL_READY
    { 21, 160, 0,  { {0, 0} } },                        // DEMPSEY
    { 17, 220, 0,  { {0, 0} } },                        // KO
    { 17, 220, 0,  { {0, 0} } },                        // WIN
};
constexpr uint32_t kDefCount = sizeof(kDefs) / sizeof(kDefs[0]);

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
    const uint8_t prio = static_cast<uint8_t>(idx);

    if (d.vibro && (!mStarted || atOrAfter(now, mVibroFreeAt) || prio > mVibroPrio)) {
        p.vibro      = d.vibro;
        const uint32_t gap = d.holdMs > kMinVibroGapMs ? d.holdMs : kMinVibroGapMs;
        mVibroFreeAt = now + gap;
        mVibroPrio   = prio;
    }

    const bool buzzerFree = !mStarted ||
                            (atOrAfter(now, mBuzzerFreeAt) &&
                             atOrAfter(now, mBuzzerLastStart + kMinBuzzerGapMs));
    if (buzzerFree && d.tones[0].ms) {
        const uint8_t vol = d.volume > kMaxVolume ? kMaxVolume : d.volume;
        Note& n = p.notes[p.noteCount++];
        n.hz  = d.tones[0].hz;                          // 0 = kernel default pitch
        n.ms  = d.tones[0].ms;
        n.vol = vol;
        mBuzzerLastStart = now;
        mBuzzerFreeAt    = now + patternMs(p);
    }

    if (p.vibro || p.noteCount) {
        mStarted = true;
    }
    return p;
}

} // namespace Pp