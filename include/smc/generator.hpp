#pragma once

#include "smc/ast.hpp"

#include <iosfwd>
#include <string>

namespace smc {

class Generator {
public:
    virtual ~Generator() = default;

    virtual void visit(const Fsm& fsm) = 0;
    virtual void visit(const StateMap& map) = 0;
    virtual void visit(const State& state) = 0;
    virtual void visit(const Transition& transition) = 0;
    virtual void visit(const Guard& guard) = 0;
};

// Generate one complete translation unit by visiting an Fsm.
class CppHeaderGenerator final : public Generator {
public:
    CppHeaderGenerator(std::ostream& output, std::string header_name);
    void visit(const Fsm& fsm) override;
    void visit(const StateMap& map) override;
    void visit(const State& state) override;
    void visit(const Transition& transition) override;
    void visit(const Guard& guard) override;

private:
    std::ostream& output_;
    std::string header_name_;
};

class CppSourceGenerator final : public Generator {
public:
    CppSourceGenerator(std::ostream& output, std::string header_name);
    void visit(const Fsm& fsm) override;
    void visit(const StateMap& map) override;
    void visit(const State& state) override;
    void visit(const Transition& transition) override;
    void visit(const Guard& guard) override;

private:
    std::ostream& output_;
    std::string header_name_;
};

} // namespace smc
