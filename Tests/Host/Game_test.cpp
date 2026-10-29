#include <gtest/gtest.h>

#include <vector>

#include "Game.hpp"

using namespace Pp;

namespace
{
/// Drives a Game with a fake wall clock.
struct Rig
{
    Game     g{12345};
    uint32_t now = 1000;
    std::vector<Sfx> heard;

    Rig() { g.tick(now); }

    void run(uint32_t ms)
    {
        for (uint32_t i = 0; i < ms; i += 10) {
            now += 10;
            const Sfx s = g.tick(now);
            if (s != Sfx::NONE) heard.push_back(s);
        }
    }
    Sfx press(Btn b)
    {
        const Sfx s = g.press(b);
        if (s != Sfx::NONE) heard.push_back(s);
        return s;
    }
    bool heardAny(Sfx s) const
    {
        for (Sfx h : heard) if (h == s) return true;
        return false;
    }
    /// Title -> fight, past the ROUND / FIGHT! intro.
    void toFight()
    {
        press(Btn::R1);
        run(kIntroMs + 20);
    }
    /// Hold the foe idle (no attack) for the next @p ms of game time.
    void calmFoe(uint32_t ms)
    {
        g.fs = FoeState::IDLE;
        g.fUntil = g.t + ms;
    }
    /// Make the foe's next swing land after @p ms from @p side.
    void foeSwingsIn(int side, uint32_t ms)
    {
        g.fs = FoeState::WINDUP;
        g.fSide = side;
        g.fSince = g.t;
        g.fUntil = g.t + ms;
        g.comboLeft = 1;
    }
};
} // namespace

TEST(Game, StartsOnTitleAndTopButtonStartsRound1)
{
    Rig r;
    EXPECT_EQ(r.g.screen, Screen::TITLE);
    EXPECT_EQ(r.press(Btn::L2), Sfx::NONE);   // bottom-left does nothing on the title
    EXPECT_EQ(r.g.screen, Screen::TITLE);
    EXPECT_EQ(r.press(Btn::L1), Sfx::SELECT);
    EXPECT_EQ(r.g.screen, Screen::INTRO);
    EXPECT_EQ(r.g.foeIdx, 0);
    EXPECT_EQ(r.g.foeHp, foeDef(0).hp);
    r.run(kIntroMs + 20);
    EXPECT_EQ(r.g.screen, Screen::FIGHT);
}

TEST(Game, R2OnTitleQuits)
{
    Rig r;
    r.press(Btn::R2);
    EXPECT_TRUE(r.g.exitRequested);
}

TEST(Game, ButtonsDoNothingDuringIntro)
{
    Rig r;
    r.press(Btn::R1);
    r.run(200);
    EXPECT_EQ(r.press(Btn::L1), Sfx::NONE);
    EXPECT_EQ(r.g.ps, PlState::IDLE);
}

TEST(Game, FoeDoesNotAttackDuringIntro)
{
    Rig r;
    r.press(Btn::R1);
    r.run(kIntroMs - 50);
    EXPECT_EQ(r.g.plHp, kPlayerHp);
    EXPECT_NE(r.g.fs, FoeState::WINDUP);
    EXPECT_NE(r.g.fs, FoeState::SWING);
}

TEST(Game, TopButtonsJabWithTheMatchingGlove)
{
    Rig r;
    r.toFight();
    r.calmFoe(5000);
    EXPECT_EQ(r.press(Btn::L1), Sfx::JAB);
    EXPECT_EQ(r.g.ps, PlState::JAB);
    EXPECT_EQ(r.g.pSide, -1);
    const int hp = r.g.foeHp;
    r.run(kJabHitMs + 10);
    EXPECT_EQ(r.g.foeHp, hp - 5);
    EXPECT_TRUE(r.heardAny(Sfx::HIT));
    r.run(kJabMs);
    EXPECT_EQ(r.g.ps, PlState::IDLE);
    r.press(Btn::R1);
    EXPECT_EQ(r.g.pSide, 1);
}

TEST(Game, BottomButtonsWeaveToTheirSide)
{
    Rig r;
    r.toFight();
    r.calmFoe(5000);
    EXPECT_EQ(r.press(Btn::L2), Sfx::ROLL_UP);
    EXPECT_EQ(r.g.ps, PlState::WEAVE);
    EXPECT_EQ(r.g.pSide, -1);
    r.run(kWeaveMs + kWeaveGraceMs + 10);
    EXPECT_EQ(r.g.ps, PlState::IDLE);
    r.press(Btn::R2);
    EXPECT_EQ(r.g.pSide, 1);
}

TEST(Game, WeavingAwayFromTheSwingDodgesIt)
{
    Rig r;
    r.toFight();
    r.foeSwingsIn(1, 100);          // right hook (from the foe's right)
    r.press(Btn::L2);               // weave left: away from it
    r.run(100 + kSwingMs + 20);
    EXPECT_EQ(r.g.plHp, kPlayerHp);
    EXPECT_EQ(r.g.dodges, 1);
    EXPECT_TRUE(r.heardAny(Sfx::DODGE));
    EXPECT_GT(r.g.counterUntil, r.g.t);
}

TEST(Game, WeavingIntoTheSwingGetsHit)
{
    Rig r;
    r.toFight();
    r.foeSwingsIn(1, 100);
    r.press(Btn::R2);               // weave right: into the right hook
    r.run(100 + kSwingMs + 20);
    EXPECT_EQ(r.g.plHp, kPlayerHp - foeDef(0).dmg);
    EXPECT_TRUE(r.heardAny(Sfx::HURT));
}

TEST(Game, StandingStillGetsHitAndStunned)
{
    Rig r;
    r.toFight();
    r.foeSwingsIn(-1, 100);
    r.run(100 + kSwingMs + 20);
    EXPECT_EQ(r.g.plHp, kPlayerHp - foeDef(0).dmg);
    EXPECT_EQ(r.g.ps, PlState::HIT);
    EXPECT_EQ(r.press(Btn::L1), Sfx::NONE);   // stunned: cannot punch
}

TEST(Game, CounterAfterDodgeDoesDoubleDamage)
{
    Rig r;
    r.toFight();
    r.foeSwingsIn(1, 100);
    r.press(Btn::L2);
    r.run(100 + kSwingMs + 20);
    ASSERT_EQ(r.g.dodges, 1);
    r.run(kWeaveMs);                // let the weave finish
    const int hp = r.g.foeHp;
    r.press(Btn::R1);
    r.run(kJabHitMs + 10);
    EXPECT_EQ(r.g.foeHp, hp - 10);
    EXPECT_EQ(r.g.pop, Pop::COUNTER);
}

TEST(Game, GuardBlocksJabsAndBreaksOnTheThird)
{
    Rig r;
    r.toFight();
    r.g.fs = FoeState::GUARD;
    r.g.fUntil = r.g.t + 5000;
    r.g.guardHits = 0;
    const int hp = r.g.foeHp;
    for (int i = 0; i < 2; ++i) {
        r.press(i ? Btn::R1 : Btn::L1);
        r.run(kJabMs + 20);
    }
    EXPECT_EQ(r.g.foeHp, hp);
    EXPECT_TRUE(r.heardAny(Sfx::BLOCK));
    r.press(Btn::L1);
    r.run(kJabHitMs + 10);
    EXPECT_EQ(r.g.fs, FoeState::STUN);
}

TEST(Game, AlternatingWeavesArmTheDempseyRoll)
{
    Rig r;
    r.toFight();
    r.calmFoe(10000);
    const Btn seq[] = { Btn::L2, Btn::R2, Btn::L2, Btn::R2 };
    for (int i = 0; i < 4; ++i) {
        const Sfx s = r.press(seq[i]);
        EXPECT_EQ(r.g.roll, i + 1);
        EXPECT_EQ(s, i == 3 ? Sfx::ROLL_READY : Sfx::ROLL_UP);
        r.run(kWeaveChainMs + 10);
    }
    EXPECT_TRUE(r.g.rollReady());
}

TEST(Game, SameSideWeavesDoNotBuildTheRoll)
{
    Rig r;
    r.toFight();
    r.calmFoe(10000);
    for (int i = 0; i < 4; ++i) {
        r.press(Btn::L2);
        r.run(kWeaveMs + kWeaveGraceMs + 10);
    }
    EXPECT_EQ(r.g.roll, 1);
}

TEST(Game, SlowWeavesLoseTheRoll)
{
    Rig r;
    r.toFight();
    r.calmFoe(10000);
    r.press(Btn::L2);
    r.run(kRollGapMs + 50);
    EXPECT_EQ(r.g.roll, 0);
    r.press(Btn::R2);
    EXPECT_EQ(r.g.roll, 1);
}

TEST(Game, ArmedRollWaitsThenFades)
{
    Rig r;
    r.toFight();
    r.calmFoe(20000);
    for (Btn b : { Btn::L2, Btn::R2, Btn::L2, Btn::R2 }) {
        r.press(b);
        r.run(kWeaveChainMs + 10);
    }
    ASSERT_TRUE(r.g.rollReady());
    r.run(kRollGapMs + 100);            // longer than a normal gap: still armed
    EXPECT_TRUE(r.g.rollReady());
    r.run(kReadyHoldMs);
    EXPECT_FALSE(r.g.rollReady());
}

TEST(Game, PunchWhileArmedUnleashesTheDempseyRoll)
{
    Rig r;
    r.toFight();
    r.calmFoe(20000);
    for (Btn b : { Btn::L2, Btn::R2, Btn::L2, Btn::R2 }) {
        r.press(b);
        r.run(kWeaveChainMs + 10);
    }
    const int hp = r.g.foeHp;
    EXPECT_EQ(r.press(Btn::R1), Sfx::DEMPSEY);
    EXPECT_EQ(r.g.ps, PlState::DEMPSEY);
    EXPECT_EQ(r.g.fs, FoeState::STUN);
    EXPECT_EQ(r.g.roll, 0);
    r.run(kDempseyMs + 20);
    // Rookie has 60 hp: 7 * 7 + 16 = 65 knocks him out.
    EXPECT_LT(r.g.foeHp, hp);
    EXPECT_EQ(r.g.foeHp, 0);
    EXPECT_EQ(r.g.fs, FoeState::KO);
    EXPECT_TRUE(r.heardAny(Sfx::KO));
}

TEST(Game, DempseyRollIgnoresTheFoesSwing)
{
    Rig r;
    r.toFight();
    r.g.foeHp = 1000;
    r.calmFoe(20000);
    for (Btn b : { Btn::L2, Btn::R2, Btn::L2, Btn::R2 }) {
        r.press(b);
        r.run(kWeaveChainMs + 10);
    }
    r.press(Btn::L1);
    r.run(kDempseyMs + 20);
    EXPECT_EQ(r.g.plHp, kPlayerHp);
    EXPECT_EQ(r.g.foeHp, 1000 - 7 * 7 - 16);
}

TEST(Game, KoAdvancesToWinScreenThenNextFoe)
{
    Rig r;
    r.toFight();
    r.calmFoe(20000);
    r.g.foeHp = 3;
    r.press(Btn::L1);
    r.run(kJabHitMs + 10);
    EXPECT_EQ(r.g.fs, FoeState::KO);
    r.run(kKoMs + 20);
    EXPECT_EQ(r.g.screen, Screen::WON);
    EXPECT_TRUE(r.heardAny(Sfx::WIN));
    EXPECT_EQ(r.press(Btn::L1), Sfx::NONE);   // input lock right after the win
    r.run(800);
    r.g.plHp = 50;
    r.press(Btn::L1);
    EXPECT_EQ(r.g.screen, Screen::INTRO);
    EXPECT_EQ(r.g.foeIdx, 1);
    EXPECT_EQ(r.g.plHp, 80);                  // +30 between rounds
    EXPECT_EQ(r.g.foeHp, foeDef(1).hp);
}

TEST(Game, LosingAllHealthShowsKnockedOutAndRetryRestoresHealth)
{
    Rig r;
    r.toFight();
    r.g.plHp = 5;
    r.foeSwingsIn(1, 50);
    r.run(50 + kSwingMs + 20);
    EXPECT_EQ(r.g.screen, Screen::LOST);
    EXPECT_EQ(r.g.plHp, 0);
    r.run(800);
    r.press(Btn::R1);
    EXPECT_EQ(r.g.screen, Screen::INTRO);
    EXPECT_EQ(r.g.plHp, kPlayerHp);
    EXPECT_EQ(r.g.foeIdx, 0);
}

TEST(Game, BeatingTheLastFoeCrownsTheChampion)
{
    Rig r;
    r.toFight();
    r.g.foeIdx = kFoeCount - 1;
    r.g.foeHp = 2;
    r.calmFoe(20000);
    r.press(Btn::R1);
    r.run(kJabHitMs + kKoMs + 40);
    EXPECT_EQ(r.g.screen, Screen::CHAMP);
    EXPECT_EQ(r.g.rank(), 0);
    r.run(800);
    r.press(Btn::L2);
    EXPECT_EQ(r.g.screen, Screen::TITLE);
}

TEST(Game, PauseFreezesTheClockAndResumes)
{
    Rig r;
    r.toFight();
    r.foeSwingsIn(1, 100);
    r.g.press(Btn::CHORD);
    EXPECT_EQ(r.g.screen, Screen::PAUSE);
    const uint32_t t = r.g.t;
    r.run(2000);
    EXPECT_EQ(r.g.t, t);
    EXPECT_EQ(r.g.plHp, kPlayerHp);
    r.press(Btn::L1);
    EXPECT_EQ(r.g.screen, Screen::FIGHT);
    r.run(100 + kSwingMs + 20);
    EXPECT_LT(r.g.plHp, kPlayerHp);           // the swing resumes after the pause
}

TEST(Game, SuspendPausesAndHoldOnPauseQuits)
{
    Rig r;
    r.toFight();
    r.g.pause();
    EXPECT_EQ(r.g.screen, Screen::PAUSE);
    r.press(Btn::R2);                         // plain R2 on the pause menu does nothing
    EXPECT_EQ(r.g.screen, Screen::PAUSE);
    r.g.press(Btn::CHORD);
    EXPECT_TRUE(r.g.exitRequested);
}

TEST(Game, LongStallDoesNotFastForward)
{
    Rig r;
    r.toFight();
    const uint32_t t = r.g.t;
    r.now += 60000;
    r.g.tick(r.now);
    EXPECT_LE(r.g.t - t, 250u);
}

TEST(Game, AFullAutoplayedRunCanBeWon)
{
    // A bot that dodges every tell, counters, and rolls when armed must be
    // able to beat all four foes: the game is winnable with the four buttons.
    Rig r;
    r.press(Btn::R1);
    int guard = 0;
    while (r.g.screen != Screen::CHAMP && guard++ < 400000) {
        r.run(10);
        Game& g = r.g;
        if (g.screen == Screen::WON && g.t - g.screenSince > 800) { r.press(Btn::R1); continue; }
        if (g.screen == Screen::LOST) { FAIL() << "bot lost to foe " << g.foeIdx; }
        if (g.screen != Screen::FIGHT) continue;
        const bool busy = g.ps == PlState::DEMPSEY || g.ps == PlState::HIT;
        if (busy) continue;
        if (g.fs == FoeState::WINDUP && g.fUntil - g.t <= 150 && g.ps != PlState::WEAVE) {
            r.press(g.fSide > 0 ? Btn::L2 : Btn::R2);
        } else if (g.rollReady() && g.ps != PlState::WEAVE) {
            r.press(Btn::L1);
        } else if ((g.fs == FoeState::RECOVER || g.fs == FoeState::STUN || g.fs == FoeState::GUARD)
                   && g.ps == PlState::IDLE) {
            r.press((g.t / 300) & 1 ? Btn::L1 : Btn::R1);
        }
    }
    EXPECT_EQ(r.g.screen, Screen::CHAMP);
    EXPECT_GT(r.g.score, 0u);
}
