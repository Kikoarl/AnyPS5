#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include <array>
#include <stdexcept>

using namespace ShaderRecompiler;

static void Require(bool value) {
    if (!value) {
        throw std::runtime_error("ds ordered count regression");
    }
}

static std::array<std::uint32_t, 2> Encode(bool gds, std::uint32_t offset, std::uint32_t vdst, std::uint32_t addr, std::uint32_t data0 = 0u, std::uint32_t data1 = 0u) {
    return {
        (0x36u << 26u) | (0x3fu << 18u) | (gds ? (1u << 17u) : 0u) | (offset & 0xffffu),
        ((vdst & 0xffu) << 24u) | ((data1 & 0xffu) << 16u) | ((data0 & 0xffu) << 8u) | (addr & 0xffu)
    };
}

static void CheckValid() {
    const auto code = Encode(true, 0x0120u, 7u, 14u);
    const RdnaInstruction instruction = DecodeRdnaDs(0u, code, 0u);
    Require(instruction.op == RdnaOpcode::DsOrderedCount);
    Require(instruction.family == RdnaInstructionFamily::DS);
    Require(instruction.gds);
    Require(instruction.memoryOffset == 0x0120u);
    Require(instruction.destination.kind == RdnaOperandKind::VectorRegister);
    Require(instruction.destination.reg == 7u);
    Require(instruction.source0.kind == RdnaOperandKind::VectorRegister);
    Require(instruction.source0.reg == 14u);
    Require(instruction.sourceCount == 1u);
    Require(instruction.dataDwordCount == 1u);

    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    bool threw = false;
    try {
        context.TranslateInstruction(instruction);
    } catch (const std::runtime_error& error) {
        threw = std::string(error.what()) == "DS ordered count translation is not supported";
    }
    Require(threw);
}

static void CheckRejections() {
    bool threwNoGds = false;
    try {
        const auto code = Encode(false, 0u, 1u, 2u);
        (void)DecodeRdnaDs(0u, code, 0u);
    } catch (const std::runtime_error& error) {
        threwNoGds = std::string(error.what()) == "DS ordered count is available only for GDS";
    }
    Require(threwNoGds);

    bool threwData0 = false;
    try {
        const auto code = Encode(true, 0u, 1u, 2u, 1u, 0u);
        (void)DecodeRdnaDs(0u, code, 0u);
    } catch (const std::runtime_error& error) {
        threwData0 = std::string(error.what()) == "DS ordered count data operands are not supported";
    }
    Require(threwData0);

    bool threwData1 = false;
    try {
        const auto code = Encode(true, 0u, 1u, 2u, 0u, 1u);
        (void)DecodeRdnaDs(0u, code, 0u);
    } catch (const std::runtime_error& error) {
        threwData1 = std::string(error.what()) == "DS ordered count data operands are not supported";
    }
    Require(threwData1);
}

int main() {
    CheckValid();
    CheckRejections();
    return 0;
}
