# Phase 050 Chunk-local Staging Result

## 当前状态

Phase 050 已把 `accumulateVFHSPFHRVV()` 与 `accumulateVFHViewpointRVV()` 从 O(N) whole-cloud staging
（整云暂存）改为 O(VLmax) chunk-local staging（按 RVV 最大向量长度复用的小缓冲）。RVV helper 在每个
VL chunk（可变向量长度分块）内先计算 f1/f2/f3 或 viewpoint alpha，再按 lane（向量通道）顺序执行标量
histogram scatter（直方图离散累加）。这保持了直方图累加顺序，避免同 bin 冲突和浮点非结合风险。

覆盖范围没有扩大，仍为 Phase 040 的 production-public 默认 VFH 边界。

## 计划动作回填

| action | 状态 | 证据 / 结果 |
| --- | --- | --- |
| IMPL | done | 生产 helper 改为 chunk-local buffers，不改变 public API、dispatch 或 fallback gate。 |
| CORRECTNESS | done | `make -C test-rvv/features/vfh run_test_compare` 通过；Std/RVV 各 7 个测试。 |
| ASM | done | `make -B -C test-rvv/features/vfh dump_bench_rvv` 通过；production helper 符号可见，RVV 指令存在。 |
| BOARD | done | `make -C test-rvv/features/vfh board_repeated REPEATED_BOARD_OUTPUT_DIR=log/board/repeated-production-phase050 BENCH_ARGS='--side 80 --iterations 8 --warmup 2'` 完成 5-run。 |
| DOCTOR | done | `make -C test-rvv/features/vfh evidence_doctor_repeated REPEATED_BOARD_OUTPUT_DIR=log/board/repeated-production-phase050` 输出 `0E/0W/11S`。 |
| DOC | superseded_by_phase060 | Phase 050 结果已记录，但最终采用形态由 Phase 060 替代。 |

## 板卡结果

`production_vfh_compute_default` 的 checksum 在 5 次运行中均一致：Std/RVV 均为 `448016`。

| run | Std ms | RVV ms | speedup |
| --- | ---: | ---: | ---: |
| run_01 | 8.79718 | 6.07652 | 1.44773x |
| run_02 | 8.78846 | 6.09961 | 1.44082x |
| run_03 | 8.82524 | 6.11764 | 1.44259x |
| run_04 | 8.79826 | 6.14111 | 1.43268x |
| run_05 | 8.81191 | 6.11128 | 1.44191x |
| mean | 8.80421 | 6.10923 | 1.44115x |

Phase 050 相比 Phase 040 的 `1.36195x` 有稳定提升，因此本阶段 implementation-shape（实现形态）
判断为 positive。但 Phase 050 后仍有每点标量 `floor` / clamp 分箱成本，且该分箱不改变直方图累加顺序，
可继续用 RVV 预计算 bin index。

## Evidence Doctor

`log/board/repeated-production-phase050/evidence_doctor.md` 结果为 `Errors=0，Warnings=0，Suggestions=11`。
Suggestions 只涉及环境 metadata、binary identity，以及历史 `component_vfh_reference` near-threshold baseline。
这些建议不阻塞 Phase 050 作为生产实现形态改进的判断；若后续出现方向反转，应先补 board 环境字段和二进制身份。

## 继续 / 停止决策

Phase 050 为 `attempted / positive production-public`，但不是最终采用形态。由于 Phase 060 的 bin index
precompute（分箱索引预计算）仍在当前生产边界内，且不改变 histogram scatter 顺序，本阶段未命中停止条件，继续推进 Phase 060。
