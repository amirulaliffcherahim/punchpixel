#include "Game.hpp"

namespace Pp
{

namespace
{
// Only the SDK's 64-colour palette: every channel is 00, 55, AA or FF.
const FoeDef kFoes[kFoeCount] = {
    //  name            hp   wind gap  rest cmb grd dmg big  skin      hair      trunks    glove     bgA       bgB
    { "ROOKIE RICK",    60,  610, 500, 980, 1,  10, 10, 0, 0xFFAA55, 0x555500, 0x00AAFF, 0xFF5555, 0xAA55FF, 0x5500AA },
    { "KID KOBRA",      90,  510, 455, 825, 2,  25, 12, 0, 0xAA5500, 0x000000, 0x00AA55, 0xFFFF00, 0xFF5555, 0xAA0000 },
    { "IRON IVAN",     120,  470, 445, 720, 3,  30, 13, 1, 0xFFAA55, 0xFFAA00, 0xAA0000, 0x55AAFF, 0x55AAFF, 0x0055AA },
    { "KING KRUSH",    160,  425, 435, 650, 4,  30, 15, 2, 0x55AA55, 0xFF55FF, 0xFFFF00, 0xAA0000, 0xFFAA00, 0xAA5500 },
};
} // namespace

const FoeDef& foeDef(int idx)
{
    if (idx < 0) idx = 0;
    if (idx >= kFoeCount) idx = kFoeCount - 1;
    return kFoes[idx];
}

Game::Game(uint32_t seed)
    : mRng(seed ? seed : 1)
{
    toTitle();
}

uint32_t Game::rand(uint32_t n)
{
    mRng = mRng * 1664525u + 1013904223u;
    return (mRng >> 8) % n;
}

void Game::emit(Sfx s)
{
    if (static_cast<uint8_t>(s) > static_cast<uint8_t>(mOut)) {
        mOut = s;
    }
}

Sfx Game::take()
{
    const Sfx s = mOut;
    mOut = Sfx::NONE;
    return s;
}

int Game::rank() const
{
    if (plHp >= 85) return 0;
    if (plHp >= 60) return 1;
    if (plHp >= 35) return 2;
    return 3;
}

void Game::setPop(Pop p, int side)
{
    pop = p;
    popAt = t;
    popSide = side;
}

// ---- screens --------------------------------------------------------------

void Game::toTitle()
{
    screen = Screen::TITLE;
    screenSince = t;
    ps = PlState::IDLE;
    fs = FoeState::IDLE;
    pop = Pop::NONE;
    foeIdx = 0;
    roll = 0;
}

void Game::startRun()
{
    score = 0;
    plHp = kPlayerHp;
    foeIdx = 0;
    dodges = 0;
    jabs = 0;
    startFight();
}

void Game::startFight()
{
    mRetryScore = score;
    foeHp = foeDef(foeIdx).hp;
    fs = FoeState::IDLE;
    fUntil = t + kIntroMs + 500;
    ps = PlState::IDLE;
    roll = 0;
    lastWeaveSide = 0;
    counterUntil = 0;
    guardHits = 0;
    pop = Pop::NONE;
    shakeUntil = hurtFlashUntil = hitFlashUntil = fHurtUntil = 0;
    screen = Screen::INTRO;
    screenSince = t;
    screenUntil = t + kIntroMs;
}

void Game::beginFight()
{
    screen = Screen::FIGHT;
    screenSince = t;
}

void Game::finishFoe()
{
    screenSince = t;
    if (foeIdx + 1 >= kFoeCount) {
        screen = Screen::CHAMP;
        score += static_cast<uint32_t>(plHp) * 5;
        emit(Sfx::WIN);
    } else {
        screen = Screen::WON;
        emit(Sfx::WIN);
    }
}

// ---- input ---------------------------------------------------------------

Sfx Game::press(Btn b)
{
    mOut = Sfx::NONE;
    const bool top = (b == Btn::L1 || b == Btn::R1);
    const bool lock = (t - screenSince) < 700;

    switch (screen) {
        case Screen::TITLE:
            if (b == Btn::CHORD || b == Btn::R2) {
                exitRequested = true;
            } else if (top) {
                startRun();
                emit(Sfx::SELECT);
            }
            break;
        case Screen::INTRO:
            break;
        case Screen::FIGHT:
            if (b == Btn::CHORD) {
                mPausedFrom = Screen::FIGHT;
                screen = Screen::PAUSE;
            } else {
                fightPress(b);
            }
            break;
        case Screen::PAUSE:
            if (b == Btn::CHORD) {
                exitRequested = true;
            } else if (top) {
                screen = mPausedFrom;
                emit(Sfx::SELECT);
            } else if (b == Btn::L2) {
                toTitle();
                emit(Sfx::SELECT);
            }
            break;
        case Screen::WON:
            if (lock) break;
            if (b == Btn::CHORD) { exitRequested = true; }
            else if (top) {
                ++foeIdx;
                plHp = plHp + 30 > kPlayerHp ? kPlayerHp : plHp + 30;
                startFight();
                emit(Sfx::SELECT);
            } else { toTitle(); emit(Sfx::SELECT); }
            break;
        case Screen::LOST:
            if (lock) break;
            if (b == Btn::CHORD) { exitRequested = true; }
            else if (top) {
                plHp = kPlayerHp;
                score = mRetryScore;
                startFight();
                emit(Sfx::SELECT);
            } else { toTitle(); emit(Sfx::SELECT); }
            break;
        case Screen::CHAMP:
            if (lock) break;
            if (b == Btn::CHORD) { exitRequested = true; }
            else { toTitle(); emit(Sfx::SELECT); }
            break;
    }
    return take();
}

void Game::fightPress(Btn b)
{
    if (ps == PlState::DEMPSEY || ps == PlState::KO || ps == PlState::HIT) {
        return;
    }

    if (b == Btn::L1 || b == Btn::R1) {
        if (rollReady()) {
            startDempsey();
            return;
        }
        if (ps == PlState::WEAVE && t - pSince < kWeaveChainMs) return;
        if (ps == PlState::JAB && t - pSince < 165) return;
        ps = PlState::JAB;
        pSide = (b == Btn::L1) ? -1 : 1;
        pSince = t;
        pUntil = t + kJabMs;
        jabResolved = false;
        ++jabs;
        emit(Sfx::JAB);
        return;
    }

    // Weave: L2 to the left, R2 to the right.
    if (ps == PlState::WEAVE && t - pSince < kWeaveChainMs) return;
    if (ps == PlState::JAB && t - pSince < 165) return;
    const int side = (b == Btn::L2) ? -1 : 1;
    if (lastWeaveSide == -side && t - lastWeaveAt <= kRollGapMs) {
        if (roll < kRollMax) ++roll;
    } else {
        roll = 1;
    }
    lastWeaveSide = side;
    lastWeaveAt = t;
    ps = PlState::WEAVE;
    pSide = side;
    pSince = t;
    pUntil = t + kWeaveMs;
    jabResolved = true;
    emit(roll == kRollNeed ? Sfx::ROLL_READY : Sfx::ROLL_UP);
}

// ---- Dempsey Roll ------------------------------------------------------------

void Game::startDempsey()
{
    ps = PlState::DEMPSEY;
    pSince = t;
    pUntil = t + kDempseyMs;
    roll = 0;
    lastWeaveSide = 0;
    dmpHits = 0;
    dmpNext = t + 165;
    jabResolved = true;
    if (fs != FoeState::KO) {
        fs = FoeState::STUN;
        fSince = t;
        fUntil = pUntil + 250;
    }
    score += 100;
    emit(Sfx::DEMPSEY);
}

void Game::stepDempsey()
{
    while (dmpHits < kDempseyHits && t >= dmpNext && fs != FoeState::KO) {
        const bool last = (dmpHits + 1 == kDempseyHits);
        const int  side = (dmpHits % 2) ? 1 : -1;
        foeDamage(last ? 16 : 7, last ? Pop::WHAM : ((dmpHits % 2) ? Pop::POW : Pop::BAM), side);
        pSide = side;
        dmpNext += kDempseyStepMs;
        ++dmpHits;
    }
    if (fs == FoeState::KO || t >= pUntil) {
        ps = PlState::IDLE;
    }
}

// ---- damage ---------------------------------------------------------------

void Game::foeDamage(int dmg, Pop p, int side)
{
    foeHp -= dmg;
    hitFlashUntil = t + 130;
    shakeUntil = t + (p == Pop::WHAM ? 300 : 110);
    setPop(p, side);
    // Only a rolled crit (Pop::COUNTER) makes a sound; every other landed hit is Sfx::HIT.
    emit(p == Pop::COUNTER ? Sfx::CRIT : Sfx::HIT);
    if (foeHp <= 0) {
        koFoe();
    }
}

void Game::koFoe()
{
    foeHp = 0;
    fs = FoeState::KO;
    fSince = t;
    fUntil = t + kKoMs;
    score += 200 + static_cast<uint32_t>(plHp) * 2;
    shakeUntil = t + 450;
    emit(Sfx::KO);
}

void Game::resolveJab()
{
    jabResolved = true;
    if (fs == FoeState::KO) return;

    if (fs == FoeState::GUARD) {
        ++guardHits;
        shakeUntil = t + 80;
        if (guardHits >= 3) {
            fs = FoeState::STUN;
            fSince = t;
            fUntil = t + 900;
            counterUntil = fUntil;
            setPop(Pop::WHAM, pSide);
            emit(Sfx::HIT);
        } else {
            setPop(Pop::BLOCK, pSide);
            emit(Sfx::BLOCK);
        }
        return;
    }

    const bool counter = t < counterUntil;
    const bool crit    = rand(kCritDenom) == 0;    // 1 in 10 jabs
    int dmg = (counter ? 10 : 5) + (crit ? 5 : 0);
    score += (counter ? 20 : 5) + (crit ? 25 : 0);
    const Pop p = crit ? Pop::COUNTER : (counter ? Pop::WHAM : ((jabs % 2) ? Pop::POW : Pop::BAM));
    foeDamage(dmg, p, pSide);
    fHurtUntil = t + 160;
}

void Game::resolveSwing()
{
    const FoeDef& d = foeDef(foeIdx);
    const bool dodged = ps == PlState::WEAVE && pSide == -fSide;
    if (ps == PlState::DEMPSEY) {
        return;
    }
    if (dodged) {
        ++dodges;
        score += 25;
        counterUntil = t + kCounterMs;
        setPop(Pop::DODGE, pSide);
        emit(Sfx::DODGE);
        return;
    }
    plHp -= d.dmg;
    ps = PlState::HIT;
    pSide = fSide;
    pSince = t;
    pUntil = t + kHitStunMs;
    jabResolved = true;
    roll = 0;
    lastWeaveSide = 0;
    shakeUntil = t + 260;
    hurtFlashUntil = t + 220;
    setPop(Pop::OUCH, fSide);
    emit(Sfx::HURT);
    if (plHp <= 0) {
        plHp = 0;
        ps = PlState::KO;
        screen = Screen::LOST;
        screenSince = t;
        emit(Sfx::KO);
    }
}

void Game::pause()
{
    if (screen == Screen::FIGHT || screen == Screen::INTRO) {
        mPausedFrom = screen;
        screen = Screen::PAUSE;
    }
}

// ---- time ----------------------------------------------------------------

Sfx Game::tick(uint32_t now)
{
    if (!mStarted) {
        mStarted = true;
        mLast = now;
    }
    uint32_t dt = now - mLast;
    mLast = now;
    if (dt > 250) dt = 250;   // a stall (suspend, debugger) must not fast-forward the fight
    mOut = Sfx::NONE;

    if (screen == Screen::PAUSE) {
        return take();
    }
    t += dt;

    if (screen == Screen::INTRO) {
        if (t >= screenUntil) beginFight();
    } else if (screen == Screen::FIGHT) {
        stepFight();
    }
    return take();
}

void Game::stepFight()
{
    // Player.
    if (ps == PlState::JAB && !jabResolved && t >= pSince + kJabHitMs) resolveJab();
    if (ps == PlState::DEMPSEY) stepDempsey();

    stepFoe();
    if (screen != Screen::FIGHT) return;

    if (ps == PlState::JAB && t >= pUntil) ps = PlState::IDLE;
    else if (ps == PlState::WEAVE && t >= pUntil + kWeaveGraceMs) ps = PlState::IDLE;
    else if (ps == PlState::HIT && t >= pUntil) ps = PlState::IDLE;

    if (roll > 0 && ps != PlState::DEMPSEY) {
        const uint32_t limit = rollReady() ? kReadyHoldMs : kRollGapMs;
        if (t - lastWeaveAt > limit) {
            roll = 0;
            lastWeaveSide = 0;
        }
    }
}

void Game::beginWindup(int side, int left, uint32_t ms)
{
    fs = FoeState::WINDUP;
    fSide = side;
    fSince = t;
    fUntil = t + ms;
    comboLeft = left;
}

void Game::stepFoe()
{
    const FoeDef& d = foeDef(foeIdx);
    switch (fs) {
        case FoeState::IDLE:
            if (t >= fUntil) {
                if (static_cast<int>(rand(100)) < d.guardPct) {
                    fs = FoeState::GUARD;
                    fSince = t;
                    fUntil = t + 900;
                    guardHits = 0;
                } else {
                    beginWindup(rand(2) ? 1 : -1, d.combo, d.windupMs);
                }
            }
            break;
        case FoeState::GUARD:
            if (t >= fUntil) {
                fs = FoeState::IDLE;
                fSince = t;
                fUntil = t + 250;
            }
            break;
        case FoeState::WINDUP:
            if (t >= fUntil) {
                fs = FoeState::SWING;
                fSince = t;
                fUntil = t + kSwingMs;
            }
            break;
        case FoeState::SWING:
            if (t >= fUntil) {
                resolveSwing();
                if (screen != Screen::FIGHT) break;
                if (--comboLeft > 0) {
                    beginWindup(-fSide, comboLeft, d.comboGapMs);
                } else {
                    fs = FoeState::RECOVER;
                    fSince = t;
                    fUntil = t + d.restMs;
                }
            }
            break;
        case FoeState::RECOVER:
            if (t >= fUntil) {
                fs = FoeState::IDLE;
                fSince = t;
                fUntil = t + 250 + rand(300);
            }
            break;
        case FoeState::STUN:
            if (t >= fUntil) {
                fs = FoeState::IDLE;
                fSince = t;
                fUntil = t + 350;
            }
            break;
        case FoeState::KO:
            if (t >= fUntil) finishFoe();
            break;
    }
}

} // namespace Pp
