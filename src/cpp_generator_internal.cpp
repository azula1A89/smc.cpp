#include "cpp_generator_internal.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>

namespace smc::cpp_detail {
namespace {

bool identifier(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_';
}

void skip_trivia(std::string_view text, std::size_t& pos) {
    while (pos < text.size()) {
        if (std::isspace(static_cast<unsigned char>(text[pos]))) {
            ++pos;
        } else if (text.substr(pos, 2) == "//") {
            pos = text.find_first_of("\r\n", pos + 2);
            if (pos == std::string_view::npos) pos = text.size();
        } else if (text.substr(pos, 2) == "/*") {
            pos += 2;
            int depth = 1;
            while (pos < text.size() && depth) {
                if (text.substr(pos, 2) == "/*") {
                    ++depth;
                    pos += 2;
                } else if (text.substr(pos, 2) == "*/") {
                    --depth;
                    pos += 2;
                } else {
                    ++pos;
                }
            }
            if (depth) throw std::invalid_argument("Unterminated action comment");
        } else {
            break;
        }
    }
}

} // namespace

std::string trim(std::string_view text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    return std::string(text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1));
}

std::vector<Parameter> parameters(std::string_view text) {
    std::vector<Parameter> result;
    std::size_t begin = 0;
    int angles = 0, parens = 0, brackets = 0;
    char quote = 0;
    bool escape = false;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        const char ch = i == text.size() ? ',' : text[i];
        if (quote) {
            if (escape) escape = false;
            else if (ch == '\\') escape = true;
            else if (ch == quote) quote = 0;
            continue;
        }
        if (ch == '"' || ch == '\'') quote = ch;
        else if (ch == '<') ++angles;
        else if (ch == '>' && angles) --angles;
        else if (ch == '(') ++parens;
        else if (ch == ')' && parens) --parens;
        else if (ch == '[') ++brackets;
        else if (ch == ']' && brackets) --brackets;
        else if (ch == ',' && !angles && !parens && !brackets) {
            auto part = text.substr(begin, i - begin);
            auto colon = part.find(':');
            if (trim(part).empty() && result.empty() && i == text.size()) return result;
            if (colon == std::string_view::npos) throw std::invalid_argument("Invalid transition parameter");
            auto name = trim(part.substr(0, colon));
            auto type = trim(part.substr(colon + 1));
            if (name.empty() || !(std::isalpha(static_cast<unsigned char>(name.front())) || name.front() == '_') ||
                !std::all_of(name.begin() + 1, name.end(), identifier) || type.empty() ||
                (colon + 1 < part.size() && part[colon + 1] == ':')) {
                throw std::invalid_argument("Invalid transition parameter");
            }
            result.push_back({std::move(name), std::move(type)});
            begin = i + 1;
        }
    }
    if (angles || parens || brackets || quote) throw std::invalid_argument("Unbalanced transition parameter");
    return result;
}

std::vector<Action> actions(std::string_view text) {
    std::vector<Action> result;
    std::size_t pos = 0;
    while (true) {
        skip_trivia(text, pos);
        if (pos == text.size()) break;
        const auto start = pos;
        while (pos < text.size() && (identifier(text[pos]) || text[pos] == '.' || text[pos] == ':')) ++pos;
        if (pos == start) throw std::invalid_argument("Invalid action name");
        std::string name(text.substr(start, pos - start));
        const auto suffix_start = pos;
        int parens = 0, brackets = 0, braces = 0;
        char quote = 0;
        bool escape = false;
        for (; pos < text.size(); ++pos) {
            const char ch = text[pos];
            if (quote) {
                if (escape) escape = false;
                else if (ch == '\\') escape = true;
                else if (ch == quote) quote = 0;
            } else if (ch == '"' || ch == '\'') {
                quote = ch;
            } else if (text.substr(pos, 2) == "//" || text.substr(pos, 2) == "/*") {
                skip_trivia(text, pos);
                if (pos == text.size()) break;
                --pos;
            } else if (ch == '(') ++parens;
            else if (ch == ')') --parens;
            else if (ch == '[') ++brackets;
            else if (ch == ']') --brackets;
            else if (ch == '{') ++braces;
            else if (ch == '}') --braces;
            else if (ch == ';' && !parens && !brackets && !braces) break;
        }
        if (pos == text.size()) throw std::invalid_argument("Unterminated action");
        result.push_back({std::move(name), std::string(text.substr(suffix_start, pos - suffix_start + 1))});
        ++pos;
    }
    return result;
}

std::string fsm_name(const Fsm& fsm) {
    if (!fsm.fsm_class.empty()) return fsm.fsm_class;
    const auto stem = std::filesystem::path(fsm.source_name).stem().string();
    if (stem.empty()) throw std::invalid_argument("FSM source name is required when %fsmclass is absent");
    return stem + "Context";
}

std::string state_name(std::string_view name, std::string_view map) {
    if (name.find("::") != std::string_view::npos) return std::string(name);
    return std::string(map) + "::" + std::string(name);
}

} // namespace smc::cpp_detail
