#pragma once

#include <stdexcept>
#include <utility>
#include <vector>

#include "cpp_fixture_generated.hpp"

namespace example::nested {
struct Helper {
    static inline int calls = 0;
    static void recordStatic() { ++calls; }
};
class Owner {
public:
    Machine fsm{*this};
    std::vector<int> events;
    int number = 0;

    void record(int value) { events.push_back(value); }
    void failAction() { throw std::runtime_error("action failed"); }
    void invoke(void (*callback)(int)) { callback(15); }
};
}
