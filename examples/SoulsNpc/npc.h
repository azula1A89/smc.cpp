#pragma once

#include "SoulsNpc.hpp"

#include <iosfwd>

class SoulsNpc {
public:
    explicit SoulsNpc(std::ostream& output);

    SoulsNpcContext fsm;
    bool boss = false;
    bool token = false;
    bool low_hp = false;
    bool target_too_far = false;

    bool isBoss() const { return boss; }
    bool hasToken() const { return token; }
    bool lowHp() const { return low_hp; }
    bool targetTooFar() const { return target_too_far; }

    void startPatrol();
    void setInvestigatePoint();
    void startInvestigateTimer();
    void moveToTarget();
    void resetAggroAndHeal();
    void walkToSpawn();
    void playStaggerAnim();
    void openRiposteWindow();
    void closeRiposteWindow();
    void restorePoise();
    void setInvulnerable(bool value);
    void playPhaseCutscene();
    void swapMoveSet();
    void dropSouls();
    void playDeathAnim();
    void startThinkTimer();
    void pickMove();
    void playTelegraph();
    void enableHitbox();
    void disableHitbox();
    void startPunishWindow();
    void releaseToken();
    void raiseGuard();
    void lowerGuard();
    void backOffAndDodge();
    void useGapCloser();

private:
    void action(const char* message);
    std::ostream& output_;
};
