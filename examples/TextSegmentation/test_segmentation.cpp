#include "segmentation.h"
#include "sentencepiece_lite.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void atoms(std::string_view input, std::vector<std::string> expected) {
    auto count = [&](std::string_view text) -> std::size_t {
        for (const auto& atom : expected) if (text == atom) return 1;
        return 2;
    };
    TextSegmentation segmenter(count, 2);
    require(segmenter.segment(input) == expected, "unexpected atomic boundaries");
}

void verify(TextSegmentation& segmenter, std::string_view input,
            const TextSegmentation::TokenCount& count, std::size_t limit,
            std::size_t overhead = 0) {
    auto parts = segmenter.segment(input);
    std::string joined;
    for (const auto& part : parts) {
        require(!part.empty(), "empty segment");
        require(count(part) + overhead < limit, "segment exceeds token budget");
        joined += part;
    }
    require(joined == input, "segments do not reconstruct input");
}
}

int main(int argc, char** argv) {
    try {
        require(argc == 2, "model path required");
        atoms("A,“B,‘C!D’ E”F!", {"A,", "“B,‘C!D’ E”", "F!"});
        atoms("A,“B,C!D", {"A,", "“B,", "C!", "D"});
        atoms("A,“B,‘C”D!E", {"A,", "“B,", "‘C”D!", "E"});
        atoms("A,”B,C", {"A,", "”B,", "C"});
        atoms("don't, 'hello, world'!", {"don't,", " ", "'hello, world'", "!"});

        auto bytes = [](std::string_view s) { return s.size(); };
        TextSegmentation strict(bytes, 4);
        verify(strict, "abcdef,gh", bytes, 4);
        require(strict.segment("abc") == std::vector<std::string>{"abc"}, "below limit");
        require(strict.segment("abcd") == (std::vector<std::string>{"abc", "d"}), "equal limit");
        require(strict.segment("abcde,f") ==
                (std::vector<std::string>{"abc", "de,", "f"}), "oversized atom packing");
        require(strict.segment("abcd,x") ==
                (std::vector<std::string>{"abc", "d,x"}), "merge oversized tail");
        require(strict.segment("").empty(), "empty input");
        TextSegmentation overhead(bytes, 5, 1);
        verify(overhead, "abcdef", bytes, 5, 1);

        auto nonmonotonic = [](std::string_view s) -> std::size_t {
            if (s == "a") return 3;
            if (s == "ab") return 1;
            return 3;
        };
        TextSegmentation irregular(nonmonotonic, 2);
        require(irregular.segment("ab") == std::vector<std::string>{"ab"}, "nonmonotonic fit");
        auto stranded = [](std::string_view s) -> std::size_t {
            return s == "a" || s == "ab" || s == "bc" ? 1 : 2;
        };
        TextSegmentation retry(stranded, 2);
        require(retry.segment("abc") == (std::vector<std::string>{"a", "bc"}),
                "nonmonotonic suffix recovery");
        TextSegmentation grapheme(bytes, 5);
        require(grapheme.segment("e\u0301xx") ==
                (std::vector<std::string>{"e\u0301x", "x"}), "grapheme boundary");
        bool failed = false;
        TextSegmentation too_small(bytes, 3);
        try { too_small.segment("e\u0301"); } catch (const std::runtime_error&) { failed = true; }
        require(failed, "oversized grapheme must fail");
        failed = false;
        try { strict.segment("😀"); } catch (const std::runtime_error&) { failed = true; }
        require(failed, "unrepresentable character must fail");
        failed = false;
        try { strict.segment(std::string("\xc0\x80", 2)); } catch (const std::invalid_argument&) { failed = true; }
        require(failed, "invalid UTF-8 must fail");
        require(strict.segment("ok") == std::vector<std::string>{"ok"}, "reuse after failure");

        std::ifstream file(argv[1], std::ios::binary);
        require(static_cast<bool>(file), "cannot open test model");
        std::string model((std::istreambuf_iterator<char>(file)), {});
        require(!file.bad(), "cannot read test model");
        sentencepiece::lite::SentencePieceLiteProcessor processor{std::string_view(model)};
        require(processor.status() == sentencepiece::lite::StatusCode::kOk, "cannot load test model");
        auto real_count = [&](std::string_view s) {
            std::vector<int> ids;
            require(processor.Encode(s, &ids) == sentencepiece::lite::StatusCode::kOk,
                    "tokenizer failed");
            return ids.size();
        };
        TextSegmentation real(real_count, 8, 1);
        verify(real, "  Hello, “世界！‘嵌套，句子’ test。”  😀\n"
                     "An exceptionallylongwordandmoretext.  ", real_count, 8, 1);
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
