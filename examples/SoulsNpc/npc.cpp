#include "npc.h"

#include <ostream>

SoulsNpc::SoulsNpc(std::ostream& output) : fsm(*this), output_(output) {}

void SoulsNpc::action(const char* message) { output_ << "  action: " << message << '\n'; }
void SoulsNpc::startPatrol() { action("start patrol"); }
void SoulsNpc::setInvestigatePoint() { action("set investigation point"); }
void SoulsNpc::startInvestigateTimer() { action("start investigation timer"); }
void SoulsNpc::moveToTarget() { action("move to target"); }
void SoulsNpc::resetAggroAndHeal() { action("reset aggro and heal"); }
void SoulsNpc::walkToSpawn() { action("walk to spawn"); }
void SoulsNpc::playStaggerAnim() { action("play stagger animation"); }
void SoulsNpc::openRiposteWindow() { action("open riposte window"); }
void SoulsNpc::closeRiposteWindow() { action("close riposte window"); }
void SoulsNpc::restorePoise() { action("restore poise"); }
void SoulsNpc::setInvulnerable(bool value) {
    action(value ? "enable invulnerability" : "disable invulnerability");
}
void SoulsNpc::playPhaseCutscene() { action("play phase cutscene"); }
void SoulsNpc::swapMoveSet() { action("swap move set"); }
void SoulsNpc::dropSouls() { action("drop souls"); }
void SoulsNpc::playDeathAnim() { action("play death animation"); }
void SoulsNpc::startThinkTimer() { action("start think timer"); }
void SoulsNpc::pickMove() { action("pick move"); }
void SoulsNpc::playTelegraph() { action("play attack telegraph"); }
void SoulsNpc::enableHitbox() { action("enable hitbox"); }
void SoulsNpc::disableHitbox() { action("disable hitbox"); }
void SoulsNpc::startPunishWindow() { action("start punish window"); }
void SoulsNpc::releaseToken() { action("release token (simulated)"); }
void SoulsNpc::raiseGuard() { action("raise guard"); }
void SoulsNpc::lowerGuard() { action("lower guard"); }
void SoulsNpc::backOffAndDodge() { action("back off and dodge"); }
void SoulsNpc::useGapCloser() { action("use gap closer"); }
