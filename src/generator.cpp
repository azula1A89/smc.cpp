#include "smc/generator.hpp"

namespace smc {

void Guard::accept(Generator& generator) const {
    generator.visit(*this);
}

void Transition::accept(Generator& generator) const {
    generator.visit(*this);
}

void State::accept(Generator& generator) const {
    generator.visit(*this);
}

void StateMap::accept(Generator& generator) const {
    generator.visit(*this);
}

void Fsm::accept(Generator& generator) const {
    generator.visit(*this);
}

} // namespace smc
