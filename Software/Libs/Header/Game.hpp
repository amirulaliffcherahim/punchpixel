/**
 ******************************************************************************
 * @file    Game.hpp
 * @brief   PunchPixel rules: four buttons in, game state and sound cues out.
 *
 * Top buttons punch (L1 left glove, R1 right glove). Bottom buttons weave
 * (L2 to the left, R2 to the right). Weave left-right-left-right and the roll
 * meter fills; at full meter the next punch unleashes the DEMPSEY ROLL.
 *
 * No LVGL, no kernel: the game keeps its own clock, advanced by tick(), so a
 * whole fight can be driven and checked on the host, and PAUSE freezes it.
 ******************************************************************************
 */

#ifndef PP_GAME_HPP
#define PP_GAME_HPP

#include <cstdint>

namespace Pp
{

constexpr int kFoeCount = 4;
constexpr int kRollNeed = 4;   ///< alternating weaves needed to arm the Dempsey Roll
constexpr int kRollMax  = 5;
constexpr int kPlayerHp = 100;

// Timings in game milliseconds.
constexpr uint32_t kJabMs          = 240;
constexpr uint32_t kJabHitMs       = 90;    ///< when the glove lands
constexpr uint32_t kWeaveMs        = 320;
constexpr uint32_t kWeaveChainMs   = 120;   ///< a weave may be followed by the next after this
constexpr uint32_t kWeaveGraceMs   = 60;    ///< late-dodge forgiveness
constexpr uint32_t kHitStunMs      = 380;
constexpr uint32_t kSwingMs        = 110;
constexpr uint32_t kCounterMs      = 700;
constexpr uint32_t kRollGapMs      = 700;   ///< max gap between weaves to keep the roll
constexpr uint32_t kReadyHoldMs    = 1800;  ///< an armed roll waits this long
constexpr uint32_t kIntroMs        = 2200;
constexpr uint32_t kFightBannerMs  = 700;   ///< "FIGHT!" shows for the last part of the intro
constexpr uint32_t kDempseyStepMs  = 170;
constexpr uint32_t kDempseyHits    = 8;
constexpr uint32_t kDempseyMs      = 150 + kDempseyHits * kDempseyStepMs + 100;
constexpr uint32_t kKoMs           = 1500;
constexpr uint32_t kPopMs          = 450;

/// Same order as CustomMessage::HapticKind, shifted by one (NONE = 0).
/// Ordered by priority: when two cues land in one tick the higher one plays.
enum class Sfx : uint8_t {
    NONE = 0, JAB, SELECT, DODGE, BLOCK, ROLL_UP, HIT, HURT, ROLL_READY, DEMPSEY, KO, WIN
};

enum class Screen : uint8_t { TITLE, INTRO, FIGHT, PAUSE, WON, LOST, CHAMP };
enum class Btn : uint8_t { L1, L2, R1, R2, CHORD };
enum class FoeState : uint8_t { IDLE, GUARD, WINDUP, SWING, RECOVER, STUN, KO };
enum class PlState : uint8_t { IDLE, JAB, WEAVE, HIT, DEMPSEY, KO };
enum class Pop : uint8_t { NONE, POW, BAM, BLOCK, DODGE, COUNTER, WHAM, OUCH };

struct FoeDef
{
    const char* name;
    uint16_t    hp;
    uint16_t    windupMs;    ///< time from the tell to the swing
    uint16_t    comboGapMs;  ///< windup of the 2nd..nth punch of a combo
    uint16_t    restMs;      ///< open for a counter after a combo
    uint8_t     combo;       ///< punches per attack, sides alternate
    uint8_t     guardPct;    ///< chance to guard instead of attacking
    uint8_t     dmg;
    uint8_t     big;         ///< extra body width
    uint32_t    skin, hair, trunks, glove, bgA, bgB;
};

const FoeDef& foeDef(int idx);

struct Game
{
    explicit Game(uint32_t seed = 1);

    /// A button went down. Returns the sound cue it caused (or NONE).
    Sfx press(Btn b);
    /// Freeze a running fight (app suspended).
    void pause();
    /// Advance by wall-clock time. Returns the sound cue, if any.
    Sfx tick(uint32_t nowMs);

    // ---- state the painter and tests read -------------------------------
    uint32_t t = 0;                    ///< game clock (stops in PAUSE)

    Screen   screen      = Screen::TITLE;
    uint32_t screenSince = 0;
    uint32_t screenUntil = 0;
    bool     exitRequested = false;

    int      foeIdx = 0;
    int      foeHp  = 0;
    FoeState fs     = FoeState::IDLE;
    int      fSide  = 1;               ///< glove that is winding up / swinging: -1 left, +1 right
    uint32_t fSince = 0;
    uint32_t fUntil = 0;
    int      comboLeft  = 0;
    int      guardHits  = 0;
    uint32_t fHurtUntil = 0;

    int      plHp  = kPlayerHp;
    PlState  ps    = PlState::IDLE;
    int      pSide = -1;               ///< jab glove or weave direction
    uint32_t pSince = 0;
    uint32_t pUntil = 0;
    bool     jabResolved = true;
    int      roll = 0;
    int      lastWeaveSide = 0;
    uint32_t lastWeaveAt   = 0;
    uint32_t counterUntil  = 0;
    uint32_t dmpNext = 0;
    uint32_t dmpHits = 0;

    Pop      pop     = Pop::NONE;
    uint32_t popAt   = 0;
    int      popSide = 0;
    uint32_t shakeUntil     = 0;
    uint32_t hurtFlashUntil = 0;
    uint32_t hitFlashUntil  = 0;

    uint32_t score  = 0;
    int      dodges = 0;
    int      jabs   = 0;

    /// 0..3 = S, A, B, C by health left at the end of the run.
    int rank() const;
    bool rollReady() const { return roll >= kRollNeed; }

private:
    uint32_t mRng;
    uint32_t mLast = 0;
    bool     mStarted = false;
    Sfx      mOut = Sfx::NONE;
    uint32_t mRetryScore = 0;
    Screen   mPausedFrom = Screen::FIGHT;

    uint32_t rand(uint32_t n);
    void emit(Sfx s);
    Sfx  take();

    void toTitle();
    void startRun();
    void startFight();
    void beginFight();
    void finishFoe();

    void fightPress(Btn b);
    void startDempsey();
    void stepFight();
    void stepFoe();
    void stepDempsey();
    void resolveJab();
    void resolveSwing();
    void beginWindup(int side, int comboLeft_, uint32_t ms);
    void foeDamage(int dmg, Pop p, int side);
    void setPop(Pop p, int side);
    void koFoe();
};

} // namespace Pp

#endif
