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

constexpr int kFoeCount     = 4;
constexpr int kPatternSteps = 5;   ///< 4 preset moves + 1 random slot per boss cycle
constexpr int kRollNeed     = 4;   ///< alternating weaves needed to arm the Dempsey Roll
constexpr int kRollMax  = 5;
constexpr int kPlayerHp = 100;

// Timings in game milliseconds.
// Fight pace is ~0.92x of the initial prototype (timings scaled by 1/0.92).
constexpr uint32_t kJabMs          = 260;
constexpr uint32_t kJabHitMs       = 100;   ///< when the glove lands
constexpr uint32_t kWeaveMs        = 350;
constexpr uint32_t kWeaveChainMs   = 130;   ///< a weave may be followed by the next after this
constexpr uint32_t kWeaveGraceMs   = 65;    ///< late-dodge forgiveness
constexpr uint32_t kHitStunMs      = 415;
constexpr uint32_t kSwingMs        = 120;
constexpr uint32_t kCounterMs      = 760;
constexpr uint32_t kRollGapMs      = 760;   ///< max gap between weaves to keep the roll
constexpr uint32_t kReadyHoldMs    = 1960;  ///< an armed roll waits this long
constexpr uint32_t kIntroMs        = 2200;
constexpr uint32_t kFightBannerMs  = 700;   ///< "FIGHT!" shows for the last part of the intro
constexpr uint32_t kDempseyStepMs  = 185;
constexpr uint32_t kDempseyHits    = 8;
constexpr uint32_t kDempseyMs      = 165 + kDempseyHits * kDempseyStepMs + 100;
constexpr uint32_t kKoMs           = 1630;
constexpr uint32_t kPopMs          = 490;
constexpr uint32_t kCritDenom      = 10;    ///< 1 in kCritDenom jabs is a crit

/// Same order as CustomMessage::HapticKind, shifted by one (NONE = 0).
/// Ordered by priority: when two cues land in one tick the higher one plays.
enum class Sfx : uint8_t {
    NONE = 0, JAB, SELECT, DODGE, BLOCK, ROLL_UP, HIT, CRIT, HURT, ROLL_READY, DEMPSEY, KO, WIN
};

enum class Screen : uint8_t { TITLE, INTRO, FIGHT, PAUSE, WON, LOST, CHAMP };
enum class Btn : uint8_t { L1, L2, R1, R2, CHORD };
enum class FoeState : uint8_t { IDLE, GUARD, WINDUP, SWING, RECOVER, STUN, KO };
enum class PlState : uint8_t { IDLE, JAB, WEAVE, HIT, DEMPSEY, KO };
enum class Pop : uint8_t { NONE, POW, BAM, BLOCK, DODGE, COUNTER, WHAM, OUCH };

/// Preset boss move: ATK_L / ATK_R start a combo on that side, GUARD raises
/// the guard, BREATHE takes an extra rest window, RANDOM rolls at runtime.
enum class FoeMove : int8_t { ATK_L = -1, GUARD = 0, ATK_R = 1, BREATHE = 2, RANDOM = 3 };

struct FoeDef
{
    const char* name;
    uint16_t    hp;
    uint16_t    windupMs;    ///< time from the tell to the swing
    uint16_t    comboGapMs;  ///< windup of the 2nd..nth punch of a combo
    uint16_t    restMs;      ///< open for a counter after a combo
    uint8_t     combo;       ///< punches per attack, sides alternate
    uint8_t     guardPct;    ///< chance to guard on the random slot
    uint8_t     dmg;
    uint8_t     big;         ///< extra body width
    uint32_t    skin, hair, trunks, glove, bgA, bgB;
    FoeMove     pattern[kPatternSteps];   ///< 4/5 preset rotation, 1/5 random
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
    int      patternStep = 0;          ///< 0..kPatternSteps-1 in the rival's rotation
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
    /// True once the rival drops to 50 % HP or below (faster windup, +1 combo, +2 dmg).
    bool enraged() const { return foeHp > 0 && foeHp * 2 <= foeDef(foeIdx).hp; }

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
