#include <gtest/gtest.h>

#include "Audio.hpp"

using namespace Pp;

namespace
{
const Sfx kAll[] = { Sfx::JAB, Sfx::SELECT, Sfx::DODGE, Sfx::BLOCK, Sfx::ROLL_UP, Sfx::HIT,
                     Sfx::HURT, Sfx::ROLL_READY, Sfx::DEMPSEY, Sfx::KO, Sfx::WIN };
}

TEST(Audio, EveryPatternEndsSilentAndStaysInTheSafeRange)
{
    for (Sfx s : kAll) {
        Audio a;
        const Plan p = a.play(s, 1000);
        ASSERT_GE(p.noteCount, 2u) << static_cast<int>(s);
        ASSERT_LE(p.noteCount, kMaxNotes);
        const Note& last = p.notes[p.noteCount - 1];
        EXPECT_EQ(last.vol, 0) << "pattern must leave the piezo off";
        EXPECT_GT(last.ms, 0);
        for (uint32_t i = 0; i + 1 < p.noteCount; ++i) {
            const Note& n = p.notes[i];
            EXPECT_GE(n.hz, 500) << "below the SDK's 500 Hz floor";
            EXPECT_LE(n.hz, kMaxHz);
            EXPECT_GT(n.ms, 0);
            EXPECT_LE(n.vol, kMaxVolume);
            EXPECT_GT(n.vol, 0);
        }
        EXPECT_LE(Audio::patternMs(p), 700u);
        EXPECT_GT(p.vibro, 0);
    }
}

TEST(Audio, BuzzerIsNeverRetriggeredWhilePlaying)
{
    Audio a;
    const Plan first = a.play(Sfx::KO, 1000);
    ASSERT_GT(first.noteCount, 0u);
    const uint32_t len = Audio::patternMs(first);
    // Hammering cues during the pattern: haptics may go, the buzzer must not.
    for (uint32_t t = 1010; t < 1000 + len; t += 10) {
        EXPECT_EQ(a.play(Sfx::WIN, t).noteCount, 0u) << "at +" << (t - 1000);
    }
    EXPECT_GT(a.play(Sfx::JAB, 1000 + len).noteCount, 0u);   // free again afterwards
}

TEST(Audio, BuzzerStartsAreRateLimited)
{
    Audio a;
    ASSERT_GT(a.play(Sfx::JAB, 1000).noteCount, 0u);   // 25 + 30 ms
    EXPECT_EQ(a.play(Sfx::JAB, 1060).noteCount, 0u);   // pattern over, gap not
    EXPECT_GT(a.play(Sfx::JAB, 1000 + kMinBuzzerGapMs).noteCount, 0u);
}

TEST(Audio, ASustainedCueStormStillSoundsButNeverOverlaps)
{
    // 10 cues a second for 10 s: every one can sound (they are short), but
    // never two patterns at once.
    Audio a;
    int buzz = 0;
    uint32_t busyUntil = 0;
    for (uint32_t t = 0; t < 10000; t += 100) {
        const uint32_t now = 5000 + t;
        const Plan p = a.play(Sfx::JAB, now);
        if (p.noteCount) {
            EXPECT_GE(now, busyUntil);
            busyUntil = now + Audio::patternMs(p);
            ++buzz;
        }
    }
    EXPECT_GE(buzz, 50);                               // still sounds like a fight
}

TEST(Audio, LongVibrationIsNotCutByLesserCues)
{
    Audio a;
    ASSERT_EQ(a.play(Sfx::KO, 1000).vibro, 15);
    EXPECT_EQ(a.play(Sfx::JAB, 1100).vibro, 0);        // the KO buzz keeps going
    EXPECT_EQ(a.play(Sfx::WIN, 1200).vibro, 37);       // a bigger moment may take over
    EXPECT_GT(a.play(Sfx::JAB, 1200 + 400).vibro, 0);  // free again later
}

TEST(Audio, StopForgetsTheSchedule)
{
    Audio a;
    a.play(Sfx::KO, 1000);
    a.stop();
    EXPECT_GT(a.play(Sfx::JAB, 1010).noteCount, 0u);
}

TEST(Audio, UnknownCueDoesNothing)
{
    Audio a;
    const Plan p = a.play(static_cast<Sfx>(200), 1000);
    EXPECT_EQ(p.noteCount, 0u);
    EXPECT_EQ(p.vibro, 0);
    EXPECT_EQ(a.play(Sfx::NONE, 1000).noteCount, 0u);
}

TEST(Audio, ClockWrapDoesNotLockTheBuzzer)
{
    Audio a;
    ASSERT_GT(a.play(Sfx::KO, 0xFFFFFF00u).noteCount, 0u);
    EXPECT_GT(a.play(Sfx::JAB, 0x00000400u).noteCount, 0u);   // after the wrap, long done
}
