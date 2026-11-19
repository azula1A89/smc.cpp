#include "cpp_fixture_owner.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {
int callback_value = 0;
void callback(int value) { callback_value = value; }
}

int main() {
    using example::nested::Owner;
    Owner owner;
    owner.fsm.enterStartState();
    owner.fsm.go(0);
    owner.fsm.repeat(-1);
    owner.fsm.repeat(-2);
    owner.fsm.assignment();
    assert(owner.number == 42);
    assert((owner.events == std::vector<int>{1, 0, 9, 8}));

    owner.fsm.pushit();
    assert(std::string(owner.fsm.getState().getName()) == "Worker::Busy");
    owner.fsm.finish();
    assert(std::string(owner.fsm.getState().getName()) == "Main::Resume");
    assert((owner.events == std::vector<int>{1, 0, 9, 8, 2, 3, 5, 12, 13, 4, 7}));

    Owner failing;
    failing.fsm.enterStartState();
    failing.fsm.go(8);
    try {
        failing.fsm.fail();
        assert(false);
    } catch (const std::runtime_error&) {
        assert(std::string(failing.fsm.getState().getName()) == "Main::Idle");
        assert((failing.events == std::vector<int>{1, 2, 8, 10, 11}));
    }
    failing.fsm.fallback();
    try {
        failing.fsm.missing();
        assert(false);
    } catch (const statemap::TransitionUndefinedException&) {}

    Owner jumping;
    jumping.fsm.pushit();
    jumping.fsm.jumpBack();
    assert(std::string(jumping.fsm.getState().getName()) == "Main::Idle");
    assert(jumping.fsm.getStateStackDepth() == 0);

    Owner complex;
    complex.fsm.complex({20, 21}, callback);
    assert((complex.events == std::vector<int>{20, 14}));
    assert(callback_value == 15);
    assert(example::nested::Helper::calls == 1);

    Owner stack;
    stack.fsm.enterStartState();
    stack.fsm.pushAgain();
    stack.fsm.leave();
    assert((stack.events == std::vector<int>{1, 12, 13}));
    assert(std::string(stack.fsm.getState().getName()) == "Main::Idle");

    try {
        stack.fsm.throwPush();
        assert(false);
    } catch (const std::runtime_error&) {
        assert(std::string(stack.fsm.getState().getName()) == "Worker::Busy");
        assert(stack.fsm.getStateStackDepth() == 1);
        assert((stack.events == std::vector<int>{1, 12, 13}));
    }
}
