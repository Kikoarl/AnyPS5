#include "ControlFlow/GraphBuilder.hpp"
#include "RdnaDecoder/RdnaScalarOpDecoder.hpp"
#include "Translation/TranslationContext.hpp"
#include <array>
#include <stdexcept>

using namespace ShaderRecompiler;

static void Require(bool value) {
    if (!value) {
        throw std::runtime_error("scalar control flow SOP1 regression");
    }
}

static void CheckSwappc() {
    const std::uint32_t word = 0x80000000u | (0x7du << 23u) | (4u << 16u) | (0x21u << 8u) | 6u;
    const std::array<std::uint32_t, 1> code{word};
    const RdnaInstruction instruction = DecodeRdnaScalarOp(code, 0u);
    Require(instruction.op == RdnaOpcode::SSwappcB64);
    Require(instruction.family == RdnaInstructionFamily::SOP1);
    Require(instruction.dataDwordCount == 2u);
    Require(instruction.destination.kind == RdnaOperandKind::ScalarRegister);
    Require(instruction.destination.reg == 4u);
    Require(instruction.sourceCount == 1);
    Require(instruction.source0.kind == RdnaOperandKind::ScalarRegister);
    Require(instruction.source0.reg == 6u);

    const std::uint32_t nullDestWord = 0x80000000u | (0x7du << 23u) | (125u << 16u) | (0x21u << 8u) | 6u;
    const std::array<std::uint32_t, 1> nullCode{nullDestWord};
    const RdnaInstruction setpcInst = DecodeRdnaScalarOp(nullCode, 0u);
    Require(setpcInst.op == RdnaOpcode::SSetpcB64);
    Require(setpcInst.destination.kind == RdnaOperandKind::Null);

    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    bool threw = false;
    try {
        context.TranslateInstruction(instruction);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}

static void CheckRfe() {
    const std::uint32_t word = 0x80000000u | (0x7du << 23u) | (0u << 16u) | (0x22u << 8u) | 8u;
    const std::array<std::uint32_t, 1> code{word};
    const RdnaInstruction instruction = DecodeRdnaScalarOp(code, 0u);
    Require(instruction.op == RdnaOpcode::SRfeB64);
    Require(instruction.family == RdnaInstructionFamily::SOP1);
    Require(instruction.destination.kind == RdnaOperandKind::Null);
    Require(instruction.sourceCount == 1);
    Require(instruction.source0.kind == RdnaOperandKind::ScalarRegister);
    Require(instruction.source0.reg == 8u);

    IrProgram program;
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    TranslationContext context(program, block, 256);
    bool threw = false;
    try {
        context.TranslateInstruction(instruction);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}

static void CheckGraphBuilder() {
    const std::uint32_t swappcWord = 0x80000000u | (0x7du << 23u) | (4u << 16u) | (0x21u << 8u) | 6u;
    const std::uint32_t endpgmWord = 0xbf810000u;
    const std::array<std::uint32_t, 2> swappcCode{swappcWord, endpgmWord};
    RdnaProgram swappcProgram;
    swappcProgram.instructions.push_back(DecodeRdnaScalarOp(swappcCode, 0u));
    swappcProgram.instructions.push_back(DecodeRdnaScalarOp(swappcCode, 1u));

    GraphBuilder builder;
    bool swappcThrew = false;
    try {
        static_cast<void>(builder.Build(swappcProgram));
    } catch (const std::invalid_argument&) {
        swappcThrew = true;
    }
    Require(swappcThrew);

    const std::uint32_t rfeWord = 0x80000000u | (0x7du << 23u) | (0u << 16u) | (0x22u << 8u) | 8u;
    const std::array<std::uint32_t, 1> rfeCode{rfeWord};
    RdnaProgram rfeProgram;
    rfeProgram.instructions.push_back(DecodeRdnaScalarOp(rfeCode, 0u));
    bool rfeThrew = false;
    try {
        static_cast<void>(builder.Build(rfeProgram));
    } catch (const std::invalid_argument&) {
        rfeThrew = true;
    }
    Require(rfeThrew);
}

int main() {
    CheckSwappc();
    CheckRfe();
    CheckGraphBuilder();
    return 0;
}
