# Phase 041 计划：generic point type public board / ASM

## 阶段意图和边界

本阶段承接 Phase 040 的代表点型 correctness scout，继续把 `generic-point-type-expansion` 推向可审查的性能闭环。目标是先把已采纳的 `traits-gated xyz AoS` public scale path 在代表点型上补齐 QEMU smoke、ASM attribution 和 board repeated evidence，再决定是否需要扩大到更多泛型点型或新的 row source。

本阶段不改 production 算法形状，不触碰 row-source 扩展，不接 source-indexed / dual-indexed / correspondence。验证范围冻结为 ordered-cloud-pair、`PointXYZI / PointXYZRGB` 代表点型、`Scalar=float`、dense xyz AoS、4K/64K/256K repeated board 规模，以及对应的 public generic smoke。

## S0 偏好冻结

| 字段 | 冻结值 |
| --- | --- |
| `preferences_loaded` | defaults loaded；local override absent；prompt override 延续当前 topic。 |
| `work_preferences` | 测试资产、diagnostic 和 prototype 详细中文注释；production 注释克制；文档 current-state-first；evidence policy summary-only。 |
| `commit_preferences` | 不自动 commit；若后续提交，topic、evidence logs 和 agent assets 分开。 |
| `artifact_publication_decision` | topic-local test/doc 为 review-required；QEMU/board summary 只在被文档引用后进入提交候选；production 长期文档已适用。 |
| `dirty_isolation` | 仅处理 `test-rvv/registration/transformation_estimation_svd_scale/**` 和必要的 topic-local phase 文档；不碰其它 topic 的 dirty 工作。 |

## 当前状态清单

| 对象 | 当前状态 | 证据 |
| --- | --- | --- |
| correctness scout | Phase 040 已证明 `PointXYZI` / `PointXYZRGB` 代表点型 public correctness。 | `doc/phases/040-generic-point-type-expansion/result.zh.md` |
| bench / board | 尚未补 generic public bench / board 入口。 | `src/bench_tesvd_scale.cpp`、`Makefile` 当前只含 PointXYZ bench。 |
| evidence registry | 当前 registry 已记录 Phase 040 之后的 qemu_correctness。 | `log/evidence_registry.json` |
| production source | 已采纳 production behavior 保持不变。 | `registration/include/pcl/registration/transformation_estimation_svd_scale.h`、`registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `generic-point-type-public` | 已采纳 traits-gated public path 在代表点型上仍保持同构标量 reference 一致，并能在 board 上给出可审查性能证据。 | 代表点型 board 结果不能外推到全部 xyz AoS 点型，更不能外推到其它 row source。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `generic-point-type-public` | ordered-cloud-pair | `PointXYZI` / `PointXYZRGB` / mixed xyz AoS / `float` / dense | current QEMU correctness 8 tests；后续补 generic public smoke | `run_bench_compare --case-filter generic-xyz-point-types-public` | board repeated target pending | production public ordered overload | pending | phase_deferred + unblocked |
| `row-source-expansion` | source-indexed / dual-indexed / correspondence | not in this phase | not in this phase | not in this phase | not in this phase | not in this phase | not in this phase | phase_deferred |

## 实现和测试动作

| 动作 | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| 补 generic public bench 入口 | `src/bench_tesvd_scale.cpp`、`Makefile` | 能在独立 case-filter 下运行 representative generic public cases。 |
| 补 qemu / board manifest 钩子 | `script/generate_tesvd_scale_qemu_evidence_manifest.py`、`script/generate_tesvd_scale_board_repeated_summary.py`、`Makefile` | 能生成 generic public smoke / repeated summary、manifest 和 Doctor。 |
| QEMU generic public smoke | `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_generic_public_state` | label、checksum、manifest 可解析；不写性能结论。 |
| 反汇编归属 | `make -C test-rvv/registration/transformation_estimation_svd_scale dump_bench_rvv` | generic public bench 符号可见 production public path 相关指令归属。 |
| board repeated generic public | `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_generic_xyz_point_types_public_repeated` | 5 runs、20 iterations、5 warmup；summary、manifest、Doctor 和 registry 刷新。 |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production public generic evidence。 |
| A/B boundary | public overload；不是 row-source / candidate A/B。 |
| 当前决策问题 | 已采纳 public path 在代表点型上的性能与维护边界是否仍可接受。 |
| 是否可外推到 production | yes, 仅限已测代表点型和 current public path；不外推到全部点型。 |
| comparison-boundary / baseline mismatch 风险 | low for public correctness；performance 仍以 board repeated 为准。 |
| weak / negative / unstable 时是否允许继续扩展 | 若 board 不稳定，可先停在代表点型并写清恢复条件；不进入 row-source 扩展。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | no；这是已采纳 family 的覆盖扩展，不做 family selection。 |

## 继续 / 停止条件

若 generic public bench、QEMU smoke、ASM attribution 和 board repeated 都闭合，则可把本阶段写成 generic point type 的 board pending/positive 结果，并继续考虑是否扩到更多 xyz AoS 点型。若 board 不可达，则保留 QEMU / ASM 证据并把 board evidence 标记为 deferred，不扩大到 row-source。
