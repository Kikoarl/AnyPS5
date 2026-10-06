#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include <array>
#include <stdexcept>

using namespace ShaderRecompiler;

static void Require(bool value) {
    if (!value) {
        throw std::runtime_error("ds gws barrier regression");
    }
}

static std::array<std::uint32_t, 2> Encode(bool gds, std::uint32_t offset, std::uint32_t vdst = 0u, std::uint32_t addr = 0u, std::uint32_t data0 = 0u, std::uint32_t data1 = 0u) {
    return {
        (0x36u << 26u) | (0x9du << 18u) | (gds ? (1u << 17u) : 0u) | (offset & 0xffffu),
        ((vdst & 0xffu) << 24u) | ((data1 & 0xffu) << 16u) | ((data0 & 0xffu) << 8u) | (addr & 0xffu)
    };
}

static void CheckValid() {
    const auto code = Encode(true, 0x0014u, 0u, 0u, 7u, 0u);
    const RdnaInstruction instruction = DecodeRdnaDs(0u, code, 0u);
    Require(instruction.op == RdnaOpcode::DsGwsBarrier);
    Require(instruction.family == RdnaInstructionFamily::DS);
    Require(instruction.gds);
    Require(instruction.memoryOffset == 0x0014u);
    Require(instruction.sourceCount == 1u);
    Require(instruction.source1.kind == RdnaOperandKind::VectorRegister && instruction.source1.reg == 7u);

    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    bool threw = false;
    try {
        context.TranslateInstruction(instruction);
    } catch (const std::runtime_error& error) {
        threw = std::string(error.what()) == "DS GWS barrier translation is not supported";
    }
    Require(threw);
}

static void CheckRejections() {
    bool threwNoGds = false;
    try {
        const auto code = Encode(false, 0x0014u, 0u, 0u, 7u, 0u);
        (void)DecodeRdnaDs(0u, code, 0u);
    } catch (const std::runtime_error& error) {
        threwNoGds = std::string(error.what()) == "DS GWS barrier is available only for GDS";
    }
    Require(threwNoGds);

    bool threwAddr = false;
    try {
        const auto code = Encode(true, 0x0014u, 0u, 1u, 7u, 0u);
        (void)DecodeRdnaDs(0u, code, 0u);
    } catch (const std::runtime_error& error) {
        threwAddr = std::string(error.what()) == "DS GWS barrier register operands are not supported";
    }
    Require(threwAddr);

    bool threwData1 = false;
    try {
        const auto code = Encode(true, 0x0014u, 0u, 0u, 7u, 1u);
        (void)DecodeRdnaDs(0u, code, 0u);
    } catch (const std::runtime_error& error) {
        threwData1 = std::string(error.what()) == "DS GWS barrier register operands are not supported";
    }
    Require(threwData1);

    bool threwVdst = false;
    try {
        const auto code = Encode(true, 0x0014u, 1u, 0u, 7u, 0u);
        (void)DecodeRdnaDs(0u, code, 0u);
    } catch (const std::runtime_error& error) {
        threwVdst = std::string(error.what()) == "DS GWS barrier register operands are not supported";
    }
    Require(threwVdst);
}

int main() {
    CheckValid();
    CheckRejections();
    return 0;
}
