#include "npc.h"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Fixture {
    std::ostringstream log;
    SoulsNpc npc{log};

    Fixture() { npc.fsm.enterStartState(); }
    void state(std::string_view expected, int depth) {
        require(npc.fsm.getState().getName() == expected, "unexpected state");
        require(npc.fsm.getStateStackDepth() == depth, "unexpected stack depth");
    }
    void combat() {
        npc.fsm.Sensed(true);
        npc.fsm.TargetSpotted();
        npc.fsm.InAttackRange();
        state("CombatMap::Strafe", 1);
    }
};

void test_patrol_and_return() {
    Fixture f;
    f.state("NpcMap::Idle", 0);
    require(f.log.str().find("start patrol") != std::string::npos, "missing initial Entry");
    f.npc.fsm.Sensed(false);
    f.state("NpcMap::Alert", 0);
    f.npc.fsm.AlertTimeout();
    f.state("NpcMap::Idle", 0);
    f.npc.fsm.Damaged();
    f.npc.fsm.Damaged();
    f.npc.fsm.InAttackRange();
    f.state("CombatMap::Strafe", 1);
    f.npc.fsm.TargetOutOfRange();
    f.state("NpcMap::Returning", 0);
    f.npc.fsm.Damaged();
    f.state("NpcMap::Returning", 0);
    f.npc.fsm.AtSpawn();
    f.state("NpcMap::Idle", 0);
}

void test_attack_cycle() {
    Fixture f;
    f.npc.token = true;
    f.combat();
    f.npc.fsm.ThinkTimer();
    f.npc.fsm.MovePicked();
    f.state("CombatMap::Windup", 1);
    f.npc.fsm.WindupDone();
    f.state("CombatMap::Strike", 1);
    f.npc.fsm.Stagger();
    f.state("CombatMap::Strike", 1);
    f.npc.fsm.StrikeDone();
    f.state("CombatMap::Recovery", 1);
    f.npc.fsm.RecoveryDone();
    f.state("CombatMap::Strafe", 1);
    const auto log = f.log.str();
    const auto hitbox_on = log.find("enable hitbox");
    const auto hitbox_off = log.find("disable hitbox");
    require(hitbox_on != std::string::npos && hitbox_off != std::string::npos &&
            hitbox_on < hitbox_off, "missing hitbox Entry/Exit");
    require(log.find("release token") != std::string::npos, "missing recovery Exit");
    f.npc.fsm.PlayerAttacking();
    f.state("CombatMap::Block", 1);
    f.npc.fsm.ThreatGone();
    f.state("CombatMap::Strafe", 1);
    require(f.log.str().find("lower guard") != std::string::npos, "missing block Exit");
    f.npc.fsm.BeyondLeash();
    f.state("NpcMap::Returning", 0);
}

void test_guards() {
    for (int branch = 0; branch != 4; ++branch) {
        Fixture f;
        f.npc.token = branch == 0 || branch == 1;
        f.npc.low_hp = branch == 0;
        f.npc.target_too_far = branch != 3;
        f.combat();
        f.npc.fsm.ThinkTimer();
        f.npc.fsm.MovePicked();
        if (branch == 0) {
            f.state("CombatMap::Retreat", 1);
            f.npc.fsm.DistanceRegained();
        } else if (branch == 1) {
            f.state("CombatMap::Windup", 1);
            f.npc.fsm.Stagger();
            f.state("NpcMap::Staggered", 0);
            f.npc.fsm.StaggerEnd();
            f.state("NpcMap::Chase", 0);
            continue;
        } else if (branch == 2) {
            f.state("CombatMap::GapCloser", 1);
            f.npc.fsm.InRange();
        } else {
            f.state("CombatMap::Strafe", 1);
        }
        f.state("CombatMap::Strafe", 1);
    }
}

void test_phase_and_death() {
    Fixture f;
    f.combat();
    try {
        f.npc.fsm.HpBelowPhaseThreshold();
        throw std::runtime_error("non-boss phase event should be undefined");
    } catch (const statemap::TransitionUndefinedException&) {}
    f.state("CombatMap::Strafe", 1);
    f.npc.boss = true;
    f.npc.fsm.HpBelowPhaseThreshold();
    f.state("NpcMap::PhaseTransition", 0);
    f.npc.fsm.PhaseDone();
    f.state("NpcMap::Chase", 0);
    const auto log = f.log.str();
    const auto invulnerability_on = log.find("enable invulnerability");
    const auto invulnerability_off = log.find("disable invulnerability");
    require(invulnerability_on != std::string::npos && invulnerability_off != std::string::npos &&
            invulnerability_on < invulnerability_off,
            "missing phase Entry/Exit");
    require(log.find("swap move set") != std::string::npos, "missing phase Exit action");
    f.npc.fsm.InAttackRange();
    f.npc.fsm.HpZero();
    f.state("NpcMap::Dead", 0);
    f.npc.fsm.Stagger();
    f.npc.fsm.TargetSpotted();
    f.state("NpcMap::Dead", 0);
    require(f.log.str().find("drop souls") != std::string::npos, "missing death Entry");

    Fixture riposte;
    riposte.npc.fsm.Stagger();
    riposte.npc.fsm.Riposted();
    riposte.state("NpcMap::Dead", 0);
    require(riposte.log.str().find("close riposte window") != std::string::npos,
            "missing stagger Exit");
}
}

int main() {
    try {
        test_patrol_and_return();
        test_attack_cycle();
        test_guards();
        test_phase_and_death();
    } catch (const std::exception& error) {
        std::cerr << "test failed: " << error.what() << '\n';
        return 1;
    }
}
