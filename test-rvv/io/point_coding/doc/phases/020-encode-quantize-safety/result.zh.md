# Phase 020：encode 量化语义安全结果

## 实际范围

本阶段只修改 `test-rvv/io/point_coding` 下的测试支撑和 topic-local 文档，没有修改 `io/include/pcl/compression/point_coding.h`。本阶段验证了 double-semantics vector quantize（双精度语义向量量化）能否同时满足 correctness（正确性）和 board performance（板卡性能），并在负向后把默认 candidate（候选实现）恢复为 Phase 010 的 indexed gather（离散加载）+ scalar same-chain quantize（同构标量量化）。

## 计划动作回填

| 动作 | 状态 | 证据 | 结论 |
| --- | --- | --- | --- |
| RVV f64 intrinsic 探针 | done | `include/impl/point_coding_support.hpp` 中保留 `PCL_POINT_CODING_RVV_F64_QUANTIZE_PROBE` probe path。 | GCC RVV intrinsic 可用，包含 f32->f64 widen、f64 div 和向零截断转换。 |
| f64 量化 correctness | done | `make run_test_compare`；后续 `make run_test_rvv POINT_CODING_ENABLE_F64_QUANTIZE=1`。 | `vfcvt.rtz` 后 Std/RVV 3 个测试通过，说明 f64 probe 可以复刻 production 标量截断语义。 |
| f64 量化 QEMU / asm | done | `make run_qemu_bench_smoke && make dump_bench_rvv`。 | probe build 可运行，asm 可见 `vfwcvt.f.f.v`、`vfdiv.vf`、`vfcvt.rtz.x.f.v`。 |
| f64 量化 board repeated | done | 阶段执行时覆盖 `log/board/repeated_phase000/summary.md`；结果已在本文件固化。 | fallback 版本 `encode_indexed_16` median 0.64x 且 5/5 退化，`encode_indexed_4096` 2/5 退化。 |
| 默认 candidate 恢复 | done | `Makefile` 新增 `POINT_CODING_ENABLE_F64_QUANTIZE`，默认 0；默认 `encodePointsRVV` 回到 scalar same-chain quantize。 | f64 probe 不再是默认 bench path；需要显式启用才重跑。 |
| 默认路径 evidence refresh（证据刷新） | done | `make collect_board_repeated POINT_CODING_REPEATED_RUNS=5 && make run_board_repeated_evidence_doctor`。 | 当前 default summary 为 Errors=0，Warnings=6；encode 仍是 diagnostic-positive，但不能 clean-adopt。 |

## f64 probe 结果

f64 probe 的 correctness 是正向的，关键修正是使用 `vfcvt.rtz` 复刻 C++ `static_cast<int>` 的 trunc toward zero（向零截断）语义；没有这个指令时，边界样本会系统性差 1。

性能证据是否定的。fallback 版本的 5-run board summary 显示：

| case | median | min | max | 处理 |
| --- | ---: | ---: | ---: | --- |
| `encode_indexed_16` | 0.64x | 0.64x | 0.67x | 5/5 退化，Evidence Doctor Error；拒绝作为默认候选。 |
| `encode_indexed_64` | 1.17x | 1.17x | 1.17x | 仅小幅正向，不能抵消 tiny leaf 风险。 |
| `encode_indexed_256` | 1.32x | 1.11x | 1.33x | 正向但低于 Phase 010 默认候选。 |
| `encode_indexed_1024` | 1.31x | 1.30x | 1.36x | 正向但低于 Phase 010 默认候选。 |
| `encode_indexed_4096` | 1.15x | 0.84x | 1.40x | 2/5 退化，Evidence Doctor Error。 |
| `encode_indexed_16384` | 1.09x | 0.97x | 1.25x | 弱正向且有退化 warning。 |

因此 `double-semantics vector quantize` 在当前 helper boundary（测试 helper 边界）下判为 `rejected for default path`。它可以作为显式 probe 保留，用于后续分析 exact double semantics（精确双精度语义）的成本，但不能作为 production-shaped candidate（生产形态候选）或默认 diagnostic path。

## 默认路径刷新结果

恢复默认 indexed gather + scalar same-chain quantize 后，当前 5-run board summary 是：

| case | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | --- |
| `encode_indexed_16` | 1.17x | 0.42x | 1.25x | weak-positive / unstable tiny leaf；1/5 退化。 |
| `encode_indexed_64` | 1.78x | 1.78x | 1.85x | positive。 |
| `encode_indexed_256` | 2.14x | 2.12x | 2.15x | positive。 |
| `encode_indexed_1024` | 2.07x | 1.97x | 2.08x | positive。 |
| `encode_indexed_4096` | 1.63x | 1.54x | 1.76x | positive。 |
| `encode_indexed_16384` | 1.31x | 1.25x | 1.32x | positive。 |
| `decode_contiguous_16/64/256/1024/4096/16384` | 1.29x / 1.29x / 1.27x / 1.27x / 1.18x / 1.18x | 1.14x / 1.23x / 1.25x / 1.27x / 1.15x / 1.15x | 1.29x / 1.29x / 1.27x / 1.27x / 1.21x / 1.20x | weak-positive；本阶段不升级为 production 主线。 |

`log/board/repeated_phase000/evidence_doctor.md` 报告 `Errors=0，Warnings=6，Suggestions=0`。主要 warning 是 `encode_indexed_16` 的 1/5 退化和 long-tail，以及 encode 组内不同 leaf size 的 outlier。处理方式是：默认 candidate 保留为 `diagnostic-positive`，但 tiny leaf 和生产接入继续降级，不能写成 production evidence（生产证据）。

## 诊断到生产错配审计回填

| question | answer |
| --- | --- |
| evidence role | diagnostic / implementation-shape。 |
| A/B boundary | test helper，不是 public overload（公开重载）或 production detail helper（生产细节 helper）。 |
| 当前决策问题 | f64 exact quantize 是否值得成为默认实现族；答案是否。 |
| diagnostic 是否可外推到 production | no。当前结果只证明 helper-level f64 exact path 过重，不证明真实 production context 下所有边界 fallback 都不可行。 |
| comparison-boundary / baseline mismatch 风险 | 当前 f64 probe 与默认 helper 同在 test-rvv boundary；但它们仍不是 production boundary。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | f64 exact path 不允许；默认 gather + scalar quantize 仍可作为后续 production-shaped diagnostic 的输入，但必须先做 input-domain / fallback 审计。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes。当前没有 production direct（真实生产路径证据）。 |

## EvidenceDecision

`EvidenceDecision`：`diagnostic-positive / no-production for f64 quantize`。

本阶段证明：

- f64 RVV quantize 可以正确复刻 double 语义，但在目标板卡上对 encode tiny leaf 和中等 leaf 不稳，默认拒绝。
- 当前默认 candidate 回到 indexed gather + scalar same-chain quantize，仍有组件级收益线索。
- `encode_indexed_16` 的离群退化说明 tiny leaf 不能 clean-adopt；必须在 production-shaped diagnostic 中有规模 gate、输入域审计或 fallback 设计。

本阶段不能证明：

- `OctreePointCloudCompression` public entry 已经变快。
- full RVV quantize 已经找到 production-safe（生产安全）实现。
- 当前 test helper 的正向可外推到其它点类型、真实 leaf 分布、entropy context（熵编码上下文）或完整 traversal（遍历）。

## 阶段反思和下一步

f64 exact path 已被拒绝为默认路线。后续仍有两个可恢复方向：

| next candidate | 状态 | 恢复条件 |
| --- | --- | --- |
| `030-encode-input-domain-and-boundary-fallback` | phase_deferred + partially blocked by semantics | 先审计 production 输入域、reference / resolution 范围和可证明 error bound（误差界）；没有这个合同，不应实现 f32 fast path + boundary lane fallback。 |
| `040-decode-stability-profile` | phase_deferred + unblocked | 若继续 decode，建议用 20-run 或 trace 解释小规模与大规模稳定性，再决定是否做 production-shaped probe。 |

`continue_stop_decision`：本阶段关闭 f64 exact quantize 路线，并刷新默认 diagnostic 证据。继续到 production integration loop 需要扩大到 production 文件和用户确认；当前不进入。下一 worker 默认先做 `030-encode-input-domain-and-boundary-fallback` 的只读语义审计，若输入域无法给出安全边界，再转向 decode stability。
