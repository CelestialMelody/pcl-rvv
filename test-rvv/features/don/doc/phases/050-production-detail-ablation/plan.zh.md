# 050 production detail ablation plan

## 阶段意图和边界

本阶段继续 DON（Difference of Normals，法线差分）topic 的 RVV 优化搜索，但不修改 PCL production（生产源码）。phase 030 的 production-public（公开生产入口）证据为 `negative`，phase 040 已回滚 production RVV path；本阶段只用 test-only diagnostic（仅测试诊断）拆分成本来源，判断是否还有值得重新设计的 RVV family（实现族）。

本阶段要回答的问题：helper-only（仅 helper）弱正向和 public entry（公开入口）负向之间，退化更可能来自 finite mask（有限值掩码）、curvature `sqrt`、curvature store（曲率写回）还是 public wrapper / timer boundary（公开包装层 / 计时边界）。

本阶段不证明：

- 当前 RVV path 可接 production。
- 泛型 normal-like 点类型可扩展。
- QEMU timing（QEMU 计时）可作为性能结论。

## 当前状态清单

| area | 当前状态 | 本阶段动作 |
| --- | --- | --- |
| production | `rolled_back_no_production` | 不改 production 源码 |
| diagnostic helper | `computeDoNRVV()` 完整复刻 normal diff + finite mask + sqrt + strided stores | 增加 detail-ablation variants |
| bench target | `bench_don` 支持 `don_normal_pair` | 增加 `don_ablate_*` case，并保持 Std/RVV 同 label 对比 |
| board target | `run_board_don_repeated` 可用，通过 `BENCH_ARGS` 切 case | 新增 phase 050 专用 repeated target / summary / doctor / registry |
| evidence | phase 010 helper-only weak_positive；phase 030 production-public negative | 生成 phase 050 detail-ablation repeated board summary |

## 候选族和假设

| candidate family | hypothesis | evidence needed |
| --- | --- | --- |
| `finite-only-no-mask` | 如果去掉 finite mask 后明显加速，mask 是当前 full RVV path 的主要成本之一 | QEMU smoke、asm 有 `vlse32/vfsub/vfmul/vfsqrt/vsse32` 且无关键 merge mask，board repeated |
| `no-sqrt-store-zero-curvature` | 如果去掉 `vfsqrt` 后明显加速，curvature sqrt 是主要成本 | QEMU smoke、asm 缺少 `vfsqrt`，board repeated |
| `normal-only-no-curvature-store` | 如果只写 normal 三分量才明显正向，curvature store / checksum-facing writeback 成本明显 | QEMU smoke、asm 缺少 curvature store，board repeated |
| `public-wrapper-cost` | 如果 helper detail positive 但 production direct negative，wrapper / output prepare / virtual entry 仍可能抵消收益 | 本阶段只解释，不重新接 production |

## Diagnostic-to-Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `diagnostic` |
| A/B boundary | `test helper detail-ablation` |
| 当前决策问题 | `implementation-shape`，判断还有没有值得进入新 production probe 的实现族 |
| diagnostic 是否可外推到 production | 不能直接外推；只能定位成本来源和恢复条件 |
| comparison-boundary / baseline mismatch 风险 | 有。phase 030 已证明 helper-only 与 public entry 方向可能不一致 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不允许；只有 detail ablation 出现稳定 positive 且能映射到 production 语义时，才另写 PI plan |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 是；本阶段不做 clean adoption |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| full RVV helper | ordered normal cloud | `pcl::Normal` / float / AoS | existing gtest | `don_normal_pair` | phase 010 weak_positive | existing asm | phase 010 doctor clean | historical baseline |
| finite-only-no-mask | ordered normal cloud, finite inputs only | `pcl::Normal` / float / AoS | QEMU smoke + checksum equality | `don_ablate_finite_only_no_mask` | planned | planned | planned | planned |
| no-sqrt-store-zero-curvature | ordered normal cloud | `pcl::Normal` / float / AoS | QEMU smoke + checksum equality | `don_ablate_no_sqrt_store_zero_curvature` | planned | planned | planned | planned |
| normal-only-no-curvature-store | ordered normal cloud | `pcl::Normal` / float / AoS | QEMU smoke + checksum equality | `don_ablate_normal_only` | planned | planned | planned | planned |

## 实现和测试动作

1. 在 `include/impl/don_core.hpp` 增加 test-only ablation helpers。Std build 走对应 scalar variant；RVV build 走对应 RVV variant。
2. 在 `src/bench_don.cpp` 增加 case-filter 支持，输出保持 `case_label,points=N` 格式，便于 shared compare script 解析。
3. 在 `Makefile` 增加 phase 050 repeated board target、summary、Evidence Doctor 和 registry 记录。
4. 运行 `make -C test-rvv/features/don run_test_compare`，确保既有 correctness 不退化。
5. 运行 QEMU smoke：`run_bench_std` / `run_bench_rvv` + `analyze_bench_compare`，只检查日志形状和 checksum，不写性能结论。
6. 运行 `dump_bench_rvv`，检查 ablation binary 中 RVV 指令存在；必要时用 filtered asm 人工确认 `vfsqrt` 是否按 case 被移除。
7. 板卡可用时运行 phase 050 repeated board target，生成 summary / Evidence Doctor / registry。

## 板卡复跑预算和决策桶

- runs: 5
- warmup: 沿用 bench 默认或 `BENCH_ARGS`
- positive: median speedup >= 1.20 且 `B/A < 1 = 0`
- weak_positive: median speedup >= 1.05 且退化不超过 1/5
- neutral: median 在 `[0.95, 1.05)`
- negative: median < 0.95
- unstable: 方向摇摆且不满足上面桶

若所有 detail ablation 都是 `neutral` / `negative` / `unstable`，当前 DON topic 命中“不建议继续优化推进”的停止条件，进入 no-production closeout。若某个 detail ablation 稳定 `positive`，下一 phase 只允许设计新的 production-shaped helper，不直接改 production。

## 文档更新清单

- `050-production-detail-ablation/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/don-evaluation.zh.md`
- `doc-rvv/library-screening/features/features-function-evaluation-queue.zh.md`
