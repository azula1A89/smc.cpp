#include "segmentation.h"

#include <uni_algo/conv.h>
#include <uni_algo/ranges_conv.h>
#include <uni_algo/ranges_grapheme.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace {

std::uint32_t closing_for(std::uint32_t c) {
    switch (c) {
    case '"': return '"';
    case '\'': return '\'';
    case 0x201c: return 0x201d;
    case 0x2018: return 0x2019;
    case 0x300c: return 0x300d;
    case 0x300e: return 0x300f;
    case 0x00ab: return 0x00bb;
    default: return 0;
    }
}

bool closing(std::uint32_t c) {
    return c == 0x201d || c == 0x2019 || c == 0x300d ||
           c == 0x300f || c == 0x00bb;
}

bool delimiter(std::uint32_t c) {
    return c == '\n' || c == '\r' || c == ',' || c == ';' ||
           c == '?' || c == '!' || c == '.' || c == 0x3002 ||
           c == 0xff0c || c == 0xff1b || c == 0xff1f ||
           c == 0xff01 || c == 0x3001;
}

bool word(std::uint32_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9');
}

} // namespace

TextSegmentation::TextSegmentation(TokenCount token_count, std::size_t limit,
                                   std::size_t fixed_tokens)
    : fsm_(*this), token_count_(std::move(token_count)), limit_(limit),
      fixed_tokens_(fixed_tokens) {
    if (!token_count_) throw std::invalid_argument("token counter is required");
    if (limit_ == 0 || fixed_tokens_ >= limit_) {
        throw std::invalid_argument("fixed token overhead must be smaller than the limit");
    }
}

std::vector<std::string> TextSegmentation::segment(std::string_view text) {
    // A fresh machine also permits reuse after an exception.
    TextSegmentation worker(token_count_, limit_, fixed_tokens_);
    worker.text_ = text;
    if (!una::is_valid_utf8(text)) throw std::invalid_argument("input is not valid UTF-8");
    auto codepoints = una::views::utf8(text);
    for (auto it = codepoints.begin(); it != codepoints.end(); ++it) {
        worker.chars_.push_back({*it, static_cast<std::size_t>(it.begin() - text.begin()),
                                 static_cast<std::size_t>(it.end() - text.begin())});
    }
    worker.fsm_.enterStartState();
    for (worker.index_ = 0; worker.index_ < worker.chars_.size(); ++worker.index_) {
        worker.fsm_.Character();
    }
    worker.fsm_.Finish();
    for (auto atom : worker.atoms_) {
        worker.atom_ = atom;
        if (!worker.current_.empty() && !worker.candidate_fits()) worker.fsm_.Flush();
        worker.fsm_.Atom();
    }
    worker.fsm_.Finish();
    return std::move(worker.output_);
}

bool TextSegmentation::is_apostrophe() const {
    const auto c = chars_[index_].value;
    return (c == '\'' || c == 0x2019) && index_ > 0 && index_ + 1 < chars_.size() &&
           word(chars_[index_ - 1].value) && word(chars_[index_ + 1].value);
}

bool TextSegmentation::is_opening_quote() const {
    const auto c = chars_[index_].value;
    if (is_apostrophe()) return false;
    if ((c == '"' || c == '\'') && !quotes_.empty() && quotes_.back() == c) return false;
    return closing_for(c) != 0;
}

bool TextSegmentation::is_closing_quote() const {
    if (is_apostrophe()) return false;
    const auto c = chars_[index_].value;
    return !quotes_.empty() && quotes_.back() == c;
}

bool TextSegmentation::is_mismatched_closer() const {
    return !is_apostrophe() && closing(chars_[index_].value) && !is_closing_quote();
}

bool TextSegmentation::closes_outer_quote() const {
    return quotes_.size() == 1 && is_closing_quote();
}

bool TextSegmentation::is_delimiter() const { return delimiter(chars_[index_].value); }

void TextSegmentation::emit_atom(std::size_t begin, std::size_t end) {
    if (begin < end) atoms_.push_back(text_.substr(begin, end - begin));
}

void TextSegmentation::begin_quote() {
    quote_begin_ = chars_[index_].begin;
    emit_atom(atom_begin_, quote_begin_);
    push_quote();
}

void TextSegmentation::push_quote() { quotes_.push_back(closing_for(chars_[index_].value)); }
void TextSegmentation::pop_quote() { quotes_.pop_back(); }

void TextSegmentation::end_quote() {
    pop_quote();
    atom_begin_ = chars_[index_].end;
    emit_atom(quote_begin_, atom_begin_);
}

void TextSegmentation::fallback_range(std::size_t begin, std::size_t end) {
    std::size_t start = begin;
    auto it = std::lower_bound(chars_.begin(), chars_.end(), begin,
                               [](const Character& ch, std::size_t pos) { return ch.begin < pos; });
    for (; it != chars_.end() && it->end <= end; ++it) {
        if (delimiter(it->value)) {
            emit_atom(start, it->end);
            start = it->end;
        }
    }
    atom_begin_ = start;
}

void TextSegmentation::fallback_quote() {
    const auto end = index_ == chars_.size() ? text_.size() : chars_[index_].end;
    fallback_range(quote_begin_, end);
    quotes_.clear();
}

void TextSegmentation::end_atom() {
    emit_atom(atom_begin_, chars_[index_].end);
    atom_begin_ = chars_[index_].end;
}

void TextSegmentation::finish_text() { emit_atom(atom_begin_, text_.size()); }

bool TextSegmentation::fits(std::string_view text) const {
    return token_count_(text) < limit_ - fixed_tokens_;
}

bool TextSegmentation::candidate_fits() const {
    std::string candidate = current_;
    candidate.append(atom_);
    return fits(candidate);
}

void TextSegmentation::append_atom() { current_.append(atom_); }

void TextSegmentation::split_atom() {
    std::vector<std::size_t> boundaries{0};
    std::vector<bool> weak{false};
    auto graphemes = una::views::grapheme::utf8(atom_);
    for (auto it = graphemes.begin(); it != graphemes.end(); ++it) {
        const auto cluster = *it;
        boundaries.push_back(static_cast<std::size_t>(cluster.data() - atom_.data()) + cluster.size());
        auto codepoints = una::views::utf8(cluster);
        auto first = codepoints.begin();
        const auto c = *first;
        weak.push_back(delimiter(c) || c == ' ' || c == '\t');
    }

    const auto n = boundaries.size() - 1;
    std::vector<std::size_t> cuts;
    std::size_t begin = 0;
    while (begin < n) {
        std::size_t best = n + 1;
        std::size_t best_weak = n + 1;
        for (std::size_t j = begin + 1; j <= n; ++j) {
            if (fits(atom_.substr(boundaries[begin], boundaries[j] - boundaries[begin]))) {
                best = j;
                if (weak[j]) best_weak = j;
            } else if (best != n + 1) {
                break;
            }
        }
        if (best == n + 1) break;
        begin = best == n ? best : (best_weak != n + 1 ? best_weak : best);
        cuts.push_back(begin);
    }

    if (begin == n) {
        begin = 0;
        for (auto cut : cuts) {
            const auto part = atom_.substr(boundaries[begin], boundaries[cut] - boundaries[begin]);
            if (cut == n) current_.assign(part);
            else output_.emplace_back(part);
            begin = cut;
        }
        return;
    }

    // Greedy cuts can strand a suffix when token counts are non-monotonic.
    std::vector<std::size_t> next(n + 1, n + 1);
    next[n] = n;
    for (std::size_t i = n; i-- > 0;) {
        std::size_t best = n + 1;
        std::size_t best_weak = n + 1;
        for (std::size_t j = i + 1; j <= n; ++j) {
            if (next[j] == n + 1 ||
                !fits(atom_.substr(boundaries[i], boundaries[j] - boundaries[i]))) continue;
            best = j;
            if (weak[j]) best_weak = j;
        }
        next[i] = best == n ? best : (best_weak != n + 1 ? best_weak : best);
    }
    if (next[0] == n + 1) {
        throw std::runtime_error("cannot split atom into graphemes within the token limit");
    }
    for (std::size_t i = 0; i < n; i = next[i]) {
        const auto part = atom_.substr(boundaries[i], boundaries[next[i]] - boundaries[i]);
        if (next[i] == n) current_.assign(part);
        else output_.emplace_back(part);
    }
}

void TextSegmentation::flush() {
    if (!fits(current_)) throw std::runtime_error("output exceeds token limit");
    output_.push_back(std::move(current_));
    current_.clear();
}
