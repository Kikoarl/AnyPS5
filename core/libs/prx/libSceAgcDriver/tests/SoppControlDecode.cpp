#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <array>
#include <cstdio>
#include <exception>
#include <vector>

using namespace ShaderRecompiler;

int main() {
    struct TestCase {
        std::uint32_t word;
        RdnaOpcode expectedOpcode;
        std::uint32_t expectedValue;
        const char* name;
    };

    constexpr std::array<TestCase, 5> testCases{{
        {0xbf8d0001u, RdnaOpcode::SSethalt, 1u, "s_sethalt 1"},
        {0xbf8d0000u, RdnaOpcode::SSethalt, 0u, "s_sethalt 0"},
        {0xbf910000u, RdnaOpcode::SSendmsghalt, 0u, "s_sendmsghalt 0"},
        {0xbf910009u, RdnaOpcode::SSendmsghalt, 9u, "s_sendmsghalt 9"},
        {0xbf9f0000u, RdnaOpcode::SCodeEnd, 0u, "s_code_end"},
    }};

    int failures = 0;
    for (const auto& tc : testCases) {
        try {
            const std::array<std::uint32_t, 1> word{tc.word};
            const auto instruction = DecodeRdnaInstruction(0u, word, 0u);

            if (instruction.op != tc.expectedOpcode) {
                std::fprintf(stderr, "%s: decoded opcode %u, expected %u\n", tc.name, static_cast<unsigned>(instruction.op), static_cast<unsigned>(tc.expectedOpcode));
                ++failures;
            }
            if (instruction.family != RdnaInstructionFamily::SOPP) {
                std::fprintf(stderr, "%s: instruction family is not SOPP\n", tc.name);
                ++failures;
            }
            if (instruction.wordCount != 1u) {
                std::fprintf(stderr, "%s: wordCount is %u, expected 1\n", tc.name, instruction.wordCount);
                ++failures;
            }
            if (instruction.sourceCount != 1u) {
                std::fprintf(stderr, "%s: sourceCount is %u, expected 1\n", tc.name, instruction.sourceCount);
                ++failures;
            }
            if (instruction.source0.kind != RdnaOperandKind::LiteralConstant || instruction.source0.value != tc.expectedValue) {
                std::fprintf(stderr, "%s: source0 value is %u, expected %u\n", tc.name, instruction.source0.value, tc.expectedValue);
                ++failures;
            }
        } catch (const std::exception& error) {
            std::fprintf(stderr, "%s failed with exception: %s\n", tc.name, error.what());
            ++failures;
        }
    }

    const std::vector<std::uint32_t> programCode{
        0xbf8d0001u, // s_sethalt 1
        0xbf910000u, // s_sendmsghalt 0
        0xbf9f0000u, // s_code_end
        0xbf810000u, // s_endpgm
    };

    try {
        const auto decoded = RdnaInstructionDecoder{}.Decode(programCode);
        if (decoded.instructions.size() != 4u) {
            std::fprintf(stderr, "program decoded %zu instructions, expected 4\n", decoded.instructions.size());
            return 1;
        }
        if (decoded.instructions[0].op != RdnaOpcode::SSethalt ||
            decoded.instructions[1].op != RdnaOpcode::SSendmsghalt ||
            decoded.instructions[2].op != RdnaOpcode::SCodeEnd ||
            decoded.instructions[3].op != RdnaOpcode::SEndpgm) {
            std::fprintf(stderr, "program instructions decoded with wrong opcodes\n");
            return 1;
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "program decode exception: %s\n", error.what());
        return 1;
    }

    return failures == 0 ? 0 : 1;
}
