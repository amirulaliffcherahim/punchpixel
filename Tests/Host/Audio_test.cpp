#include <gtest/gtest.h>

#include "Audio.hpp"

using namespace Pp;

namespace
{
const Sfx kAll[] = { Sfx::JAB, Sfx::SELECT, Sfx::DODGE, Sfx::BLOCK, Sfx::ROLL_UP, Sfx::HIT, Sfx::CRIT,
                     Sfx::HURT, Sfx::ROLL_READY, Sfx::DEMPSEY, Sfx::KO, Sfx::WIN };
static_assert(sizeof(kAll) / sizeof(kAll[0]) == static_cast<size_t>(Sfx::WIN), "every cue listed");
}

TEST(Audio, OnlyACritMakesASoundAndWindupsAreSilent)
{
    for (Sfx s : kAll) {
        Audio a;
        const Plan p = a.play(s, 1000);
        if (s == Sfx::CRIT) {
            EXPECT_GT(p.noteCount, 0u);
        } else {
            EXPECT_EQ(p.noteCount, 0u) << "cue " << static_cast<int>(s);
        }
        if (s == Sfx::JAB || s == Sfx::ROLL_UP) {
            EXPECT_EQ(p.vibro, 0) << "swing/weave must not vibrate before contact";
        } else {
            EXPECT_GT(p.vibro, 0) << static_cast<int>(s);
        }
    }
}

TEST(Audio, VibrationsAreShortSingleClicksOnly)
{
    // Single-shot click/tick effects only: no buzz, alert, double or pulsing patterns.
    const uint8_t allowed[] = { 0, 4, 17, 21, 24 };
    for (Sfx s : kAll) {
        Audio a;
        const uint8_t v = a.play(s, 1000).vibro;
        bool ok = false;
        for (uint8_t e : allowed) ok |= v == e;
        EXPECT_TRUE(ok) << "cue " << static_cast<int>(s) << " uses effect " << int(v);
    }
}

TEST(Audio, CritBeepIsOneShortSoftNoteAndEndsSilent)
{
    Audio a;
    const Plan p = a.play(Sfx::CRIT, 1000);
    ASSERT_EQ(p.noteCount, 2u);
    EXPECT_GE(p.notes[0].hz, 500);
    EXPECT_LE(p.notes[0].hz, kMaxHz);
    EXPECT_LE(p.notes[0].ms, 50);
    EXPECT_LE(p.notes[0].vol, 33);
    EXPECT_EQ(p.notes[1].vol, 0) << "must leave the piezo off";
    EXPECT_GT(p.notes[1].ms, 0);
}

TEST(Audio, CritsAndTicksRespectTheirRateGaps)
{
    Audio a;
    ASSERT_GT(a.play(Sfx::CRIT, 1000).noteCount, 0u);
    for (uint32_t t = 1010; t < 1000 + kMinBuzzerGapMs; t += 40) {
        EXPECT_EQ(a.play(Sfx::CRIT, t).noteCount, 0u);
    }
    EXPECT_GT(a.play(Sfx::CRIT, 1000 + kMinBuzzerGapMs).noteCount, 0u);

    Audio b;
    ASSERT_GT(b.play(Sfx::HIT, 2000).vibro, 0);
    EXPECT_EQ(b.play(Sfx::HIT, 2050).vibro, 0);
    EXPECT_GT(b.play(Sfx::HIT, 2000 + kMinVibroGapMs).vibro, 0);
}

TEST(Audio, StopForgetsTheSchedule)
{
    Audio a;
    a.play(Sfx::CRIT, 1000);
    a.stop();
    EXPECT_GT(a.play(Sfx::CRIT, 1010).noteCount, 0u);
}

TEST(Audio, UnknownCueDoesNothing)
{
    Audio a;
    const Plan p = a.play(static_cast<Sfx>(200), 1000);
    EXPECT_EQ(p.noteCount, 0u);
    EXPECT_EQ(p.vibro, 0);
}

TEST(Audio, ClockWrapDoesNotLockTheBuzzer)
{
    Audio a;
    ASSERT_GT(a.play(Sfx::CRIT, 0xFFFFFF00u).noteCount, 0u);
    EXPECT_GT(a.play(Sfx::CRIT, 0x00000400u).noteCount, 0u);
}