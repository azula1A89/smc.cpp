#include "npc.h"

#include <exception>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>

namespace {
using Event = void (SoulsNpcContext::*)();
const std::map<std::string, Event> events{
    {"HpZero", &SoulsNpcContext::HpZero},
    {"Stagger", &SoulsNpcContext::Stagger},
    {"Damaged", &SoulsNpcContext::Damaged},
    {"TargetSpotted", &SoulsNpcContext::TargetSpotted},
    {"AlertTimeout", &SoulsNpcContext::AlertTimeout},
    {"InAttackRange", &SoulsNpcContext::InAttackRange},
    {"TargetLost", &SoulsNpcContext::TargetLost},
    {"BeyondLeash", &SoulsNpcContext::BeyondLeash},
    {"PhaseStart", &SoulsNpcContext::PhaseStart},
    {"AtSpawn", &SoulsNpcContext::AtSpawn},
    {"Riposted", &SoulsNpcContext::Riposted},
    {"StaggerEnd", &SoulsNpcContext::StaggerEnd},
    {"PhaseDone", &SoulsNpcContext::PhaseDone},
    {"ThinkTimer", &SoulsNpcContext::ThinkTimer},
    {"TargetOutOfRange", &SoulsNpcContext::TargetOutOfRange},
    {"HpBelowPhaseThreshold", &SoulsNpcContext::HpBelowPhaseThreshold},
    {"PlayerAttacking", &SoulsNpcContext::PlayerAttacking},
    {"MovePicked", &SoulsNpcContext::MovePicked},
    {"WindupDone", &SoulsNpcContext::WindupDone},
    {"StrikeDone", &SoulsNpcContext::StrikeDone},
    {"RecoveryDone", &SoulsNpcContext::RecoveryDone},
    {"ThreatGone", &SoulsNpcContext::ThreatGone},
    {"DistanceRegained", &SoulsNpcContext::DistanceRegained},
    {"InRange", &SoulsNpcContext::InRange},
};

bool parse_bool(std::string_view text, bool& value) {
    if (text == "true") { value = true; return true; }
    if (text == "false") { value = false; return true; }
    return false;
}

void status(SoulsNpc& npc) {
    std::cout << "state: " << npc.fsm.getState().getName()
              << ", stack: " << npc.fsm.getStateStackDepth() << '\n'
              << std::boolalpha << "guards: boss=" << npc.boss << " token=" << npc.token
              << " lowhp=" << npc.low_hp << " far=" << npc.target_too_far << '\n';
}

void help() {
    std::cout << "Commands:\n"
              << "  event <name>       send an event\n"
              << "  event Sensed <true|false>\n"
              << "  set <boss|token|lowhp|far> <true|false>\n"
              << "  status | help | quit\nEvents:";
    for (const auto& [name, event] : events) {
        std::cout << ' ' << name;
    }
    std::cout << " Sensed\n";
}

int run() {
    SoulsNpc npc(std::cout);
    npc.fsm.enterStartState();
    help();
    status(npc);
    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line)) {
        std::istringstream input(line);
        std::string command, name, argument, extra;
        if (!(input >> command)) continue;
        if (command == "quit" || command == "help" || command == "status") {
            if (input >> extra) {
                std::cout << "error: unexpected argument\n";
            } else if (command == "quit") {
                return 0;
            } else if (command == "help") {
                help();
            } else {
                status(npc);
            }
        } else if (command == "set") {
            bool value;
            if (!(input >> name >> argument) || (input >> extra) || !parse_bool(argument, value)) {
                std::cout << "error: usage: set <boss|token|lowhp|far> <true|false>\n";
                continue;
            }
            if (name == "boss") npc.boss = value;
            else if (name == "token") npc.token = value;
            else if (name == "lowhp") npc.low_hp = value;
            else if (name == "far") npc.target_too_far = value;
            else {
                std::cout << "error: unknown guard " << name << '\n';
                continue;
            }
            status(npc);
        } else if (command == "event") {
            if (!(input >> name)) {
                std::cout << "error: expected an event name\n";
                continue;
            }
            bool noise = false;
            const auto event = events.find(name);
            if (name == "Sensed") {
                if (!(input >> argument) || (input >> extra) || !parse_bool(argument, noise)) {
                    std::cout << "error: usage: event Sensed <true|false>\n";
                    continue;
                }
            } else if (event == events.end()) {
                std::cout << "error: unknown event " << name << '\n';
                continue;
            } else if (input >> extra) {
                std::cout << "error: unexpected event argument\n";
                continue;
            }
            try {
                if (name == "Sensed") npc.fsm.Sensed(noise);
                else (npc.fsm.*event->second)();
                status(npc);
            } catch (const statemap::TransitionUndefinedException& error) {
                std::cout << "error: event " << name << " is undefined in "
                          << npc.fsm.getState().getName() << ": " << error.what() << '\n';
            }
        } else {
            std::cout << "error: unknown command " << command << '\n';
        }
    }
    if (std::cin.bad()) {
        std::cerr << "error: failed to read input\n";
        return 1;
    }
    return 0;
}
}

int main() {
    try {
        return run();
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
