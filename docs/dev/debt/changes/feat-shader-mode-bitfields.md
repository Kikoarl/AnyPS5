# shader

## change Shader MODE register access

- [Shader MODE register access](../../../core/shader/recompiler/Translation/src/RoundingMode.cpp) follows immediate writes to MODE round (bits 0-3) and denormal (bits 4-7) fields (s_round_mode, s_denorm_mode, s_setreg_imm32_b32) through the CFG; s_getreg_b32 returns tracked values and known status bitfields (DX10_CLAMP bit 8, IEEE bit 9, FP16_OVFL bit 23). s_setreg writes to bits 8+ are ignored, so write-then-read returns static values. Only instructions in RoundingUseOf modeling runtime mode act on it; other instructions throw if consulted fields differ from initial, as do reads written from SGPRs (s_setreg_b32) or differing across paths. Reads follow RDNA2 ISA §12.3 Table 39 and [LLVM SMInstructions.td](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.0/llvm/lib/Target/AMDGPU/SMInstructions.td#L150). Other register reads throw and other s_setreg_b32 fields are ignored. Labels: gpu, tested.
