#include "segmentation.h"
#include "sentencepiece_lite.h"

#include <charconv>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
std::size_t number(std::string_view text) {
    std::size_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        throw std::invalid_argument("limit and overhead must be non-negative integers");
    }
    return value;
}
}

int main(int argc, char** argv) {
    try {
        if (argc != 3 && argc != 4) {
            std::cerr << "Usage: text_segmentation MODEL.spm.fb LIMIT [FIXED_TOKENS] < input.txt\n";
            return 2;
        }
        std::ifstream model_file(argv[1], std::ios::binary);
        if (!model_file) throw std::runtime_error("cannot open model file");
        std::string model((std::istreambuf_iterator<char>(model_file)), {});
        if (model_file.bad()) throw std::runtime_error("cannot read model file");
        sentencepiece::lite::SentencePieceLiteProcessor processor{std::string_view(model)};
        if (processor.status() != sentencepiece::lite::StatusCode::kOk) {
            throw std::runtime_error("cannot load SentencePiece Lite model");
        }
        auto count = [&](std::string_view text) {
            std::vector<int> ids;
            if (processor.Encode(text, &ids) != sentencepiece::lite::StatusCode::kOk) {
                throw std::runtime_error("SentencePiece Lite encoding failed");
            }
            return ids.size();
        };
        TextSegmentation segmentation(count, number(argv[2]), argc == 4 ? number(argv[3]) : 0);
        std::string text((std::istreambuf_iterator<char>(std::cin)), {});
        if (std::cin.bad()) throw std::runtime_error("cannot read standard input");
        const auto segments = segmentation.segment(text);
        for (std::size_t i = 0; i < segments.size(); ++i) {
            std::cout << "--- segment " << i + 1 << " (tokens: " << count(segments[i])
                      << ") ---\n" << segments[i] << "\n";
        }
        if (!std::cout) throw std::runtime_error("cannot write standard output");
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
