#include "smc/generator.hpp"

#include "cpp_generator_internal.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string_view>
#include <vector>

namespace smc {
namespace {

bool identifier_start(char ch) {
    return std::isalpha(static_cast<unsigned char>(ch)) != 0 || ch == '_';
}

bool identifier_part(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_';
}

struct Token {
    std::string_view text;
    bool identifier = false;
};

// Only tokenize enough to recognize direct ctxt.member references, not C++ semantics.
std::vector<Token> tokens(std::string_view text) {
    std::vector<Token> result;
    std::size_t pos = 0;
    while (pos < text.size()) {
        if (std::isspace(static_cast<unsigned char>(text[pos]))) {
            ++pos;
        } else if (text.substr(pos, 2) == "//") {
            const auto end = text.find_first_of("\r\n", pos + 2);
            pos = end == std::string_view::npos ? text.size() : end;
        } else if (text.substr(pos, 2) == "/*") {
            const auto end = text.find("*/", pos + 2);
            pos = end == std::string_view::npos ? text.size() : end + 2;
        } else if (text.substr(pos, 2) == "R\"") {
            const auto open = text.find('(', pos + 2);
            if (open == std::string_view::npos || open - pos > 18) {
                pos = text.size();
            } else {
                const std::string terminator = ")" + std::string(text.substr(pos + 2, open - pos - 2)) + "\"";
                const auto end = text.find(terminator, open + 1);
                pos = end == std::string_view::npos ? text.size() : end + terminator.size();
            }
            result.push_back({{}, false});
        } else if (text[pos] == '"' || text[pos] == '\'') {
            const char quote = text[pos++];
            while (pos < text.size()) {
                if (text[pos] == '\\' && pos + 1 < text.size()) pos += 2;
                else if (text[pos++] == quote) break;
            }
            result.push_back({{}, false});
        } else if (identifier_start(text[pos])) {
            const auto begin = pos++;
            while (pos < text.size() && identifier_part(text[pos])) ++pos;
            // Handle encoding prefixes on raw literals.
            if ((text.substr(begin, pos - begin) == "u8R" ||
                 text.substr(begin, pos - begin) == "uR" ||
                 text.substr(begin, pos - begin) == "UR" ||
                 text.substr(begin, pos - begin) == "LR") &&
                pos < text.size() && text[pos] == '"') {
                pos = begin + (text[begin] == 'u' && text[begin + 1] == '8' ? 2 : 1);
                continue;
            }
            result.push_back({text.substr(begin, pos - begin), true});
        } else {
            const auto begin = pos++;
            if (pos < text.size() && (text.substr(begin, 2) == "::" ||
                                      text.substr(begin, 2) == "->")) ++pos;
            result.push_back({text.substr(begin, pos - begin), false});
        }
    }
    return result;
}

struct Reference {
    std::string name;
    bool action = false;
    bool guard_call = false;
    bool guard_value = false;
    bool guard_unknown = false;
    bool other = false;
    bool action_zero_only = true;
};

class Checklist {
public:
    explicit Checklist(std::string_view owner) : owner_(owner) {}

    void expression(std::string_view text, std::string_view kind, bool guard) {
        if (text.empty()) return;
        add_usage(kind, text);
        scan(text, guard);
    }

    void scan(std::string_view text, bool guard) {
        const auto parts = tokens(text);
        for (std::size_t i = 0; i + 2 < parts.size(); ++i) {
            if (parts[i].text != "ctxt" || parts[i + 1].text != "." ||
                !parts[i + 2].identifier ||
                (i > 0 && (parts[i - 1].text == "." || parts[i - 1].text == "->" ||
                           parts[i - 1].text == "::"))) continue;
            auto& ref = reference(parts[i + 2].text);
            if (!guard) ref.other = true;
            else if (i + 3 < parts.size() && parts[i + 3].text == "(") ref.guard_call = true;
            else if (i + 3 < parts.size() && parts[i + 3].text == "::") ref.guard_unknown = true;
            else ref.guard_value = true;
        }
    }

    void actions(std::string_view text) {
        for (const auto& action : cpp_detail::actions(text)) {
            const std::string usage = action.name + action.suffix;
            add_usage("action", usage);
            scan(action.suffix, false);
            if (action.name == "emptyStateStack" || action.name.find("::") != std::string::npos) {
                if (action.name != "emptyStateStack") scan(action.name, false);
                continue;
            }
            const auto dot = action.name.find('.');
            const auto name = std::string_view(action.name).substr(0, dot);
            if (!cpp_detail::identifier(name)) continue;
            auto& ref = reference(name);
            ref.action = true;
            const auto parts = tokens(action.suffix);
            ref.action_zero_only = ref.action_zero_only && dot == std::string::npos &&
                parts.size() == 3 && parts[0].text == "(" && parts[1].text == ")" &&
                parts[2].text == ";";
        }
    }

    std::string render() const {
        std::ostringstream out;
        out << "// Host class TODO checklist (review all usages).\n";
        if (references_.empty()) out << "// No direct host references recognized.\n";
        for (const auto& ref : references_) {
            if (ref.action && ref.action_zero_only && ref.name != owner_ &&
                !ref.guard_call && !ref.guard_value && !ref.guard_unknown && !ref.other) {
                out << "void " << ref.name << "(); // action; suggested signature\n";
            } else {
                out << "// TODO: " << ref.name << " (";
                bool separator = false;
                const auto label = [&](std::string_view value) {
                    if (separator) out << ", ";
                    out << value;
                    separator = true;
                };
                if (ref.action) label("action");
                if (ref.guard_call) label("guard function reference");
                if (ref.guard_value) label("guard member reference");
                if (ref.guard_unknown) label("guard reference");
                if (ref.other) label("action reference");
                out << "): determine declaration and type.\n";
            }
        }
        for (const auto& usage : usages_) out << "// " << usage << " .\n";
        return out.str();
    }

private:
    Reference& reference(std::string_view name) {
        const auto found = std::find_if(references_.begin(), references_.end(),
            [&](const Reference& ref) { return ref.name == name; });
        if (found != references_.end()) return *found;
        references_.push_back({std::string(name)});
        return references_.back();
    }

    void add_usage(std::string_view kind, std::string_view text) {
        std::string line(kind);
        line += ": ";
        for (unsigned char ch : text) {
            if (ch == '\n') line += "\\n";
            else if (ch == '\r') line += "\\r";
            else if (ch < 32 || ch == 127) line += ' ';
            else line += static_cast<char>(ch);
        }
        if (std::find(usages_.begin(), usages_.end(), line) == usages_.end()) {
            usages_.push_back(std::move(line));
        }
    }

    std::vector<Reference> references_;
    std::vector<std::string> usages_;
    std::string_view owner_;
};

} // namespace

std::string generate_cpp_todo(const Fsm& fsm) {
    Checklist checklist(fsm.context_class);
    for (const auto& map : fsm.maps) {
        for (const auto& state : map.states) {
            if (state.entry_actions) checklist.actions(*state.entry_actions);
            if (state.exit_actions) checklist.actions(*state.exit_actions);
            for (const auto& transition : state.transitions) {
                for (const auto& guard : transition.guards) {
                    checklist.expression(guard.condition, "guard expression", true);
                    checklist.actions(guard.actions);
                    checklist.expression(guard.pop_arguments, "pop arguments", false);
                }
            }
        }
    }
    return checklist.render();
}

} // namespace smc
