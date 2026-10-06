#include "Translation/TranslationContext.hpp"
#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include <array>
#include <stdexcept>

using namespace ShaderRecompiler;

static void Require(bool value) {
    if (!value) {
        throw std::runtime_error("ds condxchg32 rtn b64 regression");
    }
}

static std::array<std::uint32_t, 2> Encode(bool gds, std::uint32_t offset, std::uint32_t vdst, std::uint32_t addr, std::uint32_t data0, std::uint32_t data1) {
    return {
        (0x36u << 26u) | (0x7eu << 18u) | (gds ? (1u << 17u) : 0u) | (offset & 0xffffu),
        ((vdst & 0xffu) << 24u) | ((data1 & 0xffu) << 16u) | ((data0 & 0xffu) << 8u) | (addr & 0xffu)
    };
}

static void CheckValid() {
    const auto code = Encode(false, 0x0120u, 4u, 2u, 6u, 8u);
    const RdnaInstruction instruction = DecodeRdnaDs(0u, code, 0u);
    Require(instruction.op == RdnaOpcode::DsCondxchg32RtnB64);
    Require(instruction.family == RdnaInstructionFamily::DS);
    Require(!instruction.gds);
    Require(instruction.memoryOffset == 0x0120u);
    Require(instruction.destination.kind == RdnaOperandKind::VectorRegister);
    Require(instruction.destination.reg == 4u);
    Require(instruction.source0.kind == RdnaOperandKind::VectorRegister);
    Require(instruction.source0.reg == 2u);
    Require(instruction.source1.kind == RdnaOperandKind::VectorRegister);
    Require(instruction.source1.reg == 6u);
    Require(instruction.source2.kind == RdnaOperandKind::VectorRegister);
    Require(instruction.source2.reg == 8u);
    Require(instruction.sourceCount == 3u);
    Require(instruction.dataDwordCount == 2u);
    Require(instruction.dataBits == 32u);

    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    bool threw = false;
    try {
        context.TranslateInstruction(instruction);
    } catch (const std::runtime_error& error) {
        threw = std::string(error.what()) == "DS conditional exchange translation is not supported";
    }
    Require(threw);
}

static void CheckGds() {
    const auto code = Encode(true, 0x0040u, 10u, 12u, 14u, 16u);
    const RdnaInstruction instruction = DecodeRdnaDs(0u, code, 0u);
    Require(instruction.op == RdnaOpcode::DsCondxchg32RtnB64);
    Require(instruction.gds);
    Require(instruction.memoryOffset == 0x0040u);
    Require(instruction.destination.reg == 10u);
    Require(instruction.source0.reg == 12u);
    Require(instruction.source1.reg == 14u);
    Require(instruction.source2.reg == 16u);
    Require(instruction.sourceCount == 3u);
    Require(instruction.dataDwordCount == 2u);

    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    bool threw = false;
    try {
        context.TranslateInstruction(instruction);
    } catch (const std::runtime_error& error) {
        threw = std::string(error.what()) == "DS conditional exchange translation is not supported";
    }
    Require(threw);
}

int main() {
    CheckValid();
    CheckGds();
    return 0;
}
