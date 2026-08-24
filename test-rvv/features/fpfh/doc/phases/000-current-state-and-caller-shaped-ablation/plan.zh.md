# Phase 000 Plan: current-state and caller-shaped ablation

## 阶段意图和边界

本阶段启动 `features/fpfh` RVV topic。目标是建立 `fpfh.hpp` 的函数级评估、topic-local
test / bench scaffold（测试 / 性能测试脚手架）和 caller-shaped diagnostic（调用方形态诊断）证据入口。
本阶段不修改 production（生产源码），不创建 `doc-rvv/features/fpfh-RVV.zh.md`，不证明 OMP 入口或泛型点类型。

## 当前状态清单

| area | 当前事实 |
| --- | --- |
| production | `features/include/pcl/features/impl/fpfh.hpp` 没有 `__RVV10__` 分支；核心 loop 是 SPFH pair histogram 和 33-bin weighted SPFH。 |
| shared helper | `features/src/pfh.cpp::computePairFeatures` 提供 Darboux frame 点对特征；本身没有 batch API。 |
| upstream tests | `test/features/test_pfh_estimation.cpp` 覆盖 FPFH / PFH / VFH，包括 `computePointSPFHSignature` 和 `weightPointSPFHSignature` 数值。 |
| topic scaffold | 新建 `test-rvv/features/fpfh`，包含 topic-local reference、gtest、bench、board 和 Evidence Doctor manifest wrapper。 |
| production doc | no adopted production behavior，`doc-rvv/features/fpfh-RVV.zh.md` not_applicable。 |

## Phase scope 与扩展队列

| item | scope |
| --- | --- |
| validated_scope | `PointNormal -> FPFHSignature33`、`Scalar=float`、11+11+11 bins、synthetic dense cloud、KSearch synthetic public path、fixed wrapped neighborhood component cases。 |
| unvalidated_scope | `PointXYZ` with separate `Normal` cloud、PointXYZ-like / PointNormal-like traits、custom point layouts、search surface != input、indices subset、radius search、OMP、real registration datasets、`Scalar=double`、production dispatch / fallback。 |
| point_type_expansion_queue | 若后续 production candidate positive，必须补 PointXYZ + Normal、PointXYZRGB + Normal、PointNormal exact、custom xyz-like traits 的 correctness / fallback / bench / asm / board / Evidence Doctor。 |
| phase_closeout_boundary | 本阶段只能关闭 scaffold 和诊断基线；不能关闭 production template 泛型结论。 |

## 优化矩阵

| candidate family | row source / input | point type / Scalar / layout | correctness | bench / board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `weighted-spfh-33` | dense SPFH row indices | `PointNormal`, float, 33-bin output | planned `run_test_compare` | planned `component_weighted_spfh_33` board repeated | planned | planned | planned |
| `spfh-pair-feature-batch` | fixed neighborhood indices | `PointNormal`, finite AoS | planned `run_test_compare` | planned `component_spfh_signature` board repeated | planned | planned | planned |
| `public-fpfh-k` | KSearch public path | `PointNormal -> FPFHSignature33` | planned `run_test_compare` | planned `public_fpfh_k` board repeated | planned | planned | planned |

## 实现和测试动作

| action | artifact / command | completion |
| --- | --- | --- |
| A1 scaffold topic harness | `Makefile`, `board.mk`, `include/fpfh.h`, `include/impl/fpfh_reference.hpp`, `src/test_fpfh.cpp`, `src/bench_fpfh.cpp`, manifest script | files compile or errors are recorded in result |
| A2 QEMU correctness | `make -C test-rvv/features/fpfh run_test_compare` | Std/RVV gtest pass; QEMU only correctness/log-shape |
| A3 asm attribution | `make -C test-rvv/features/fpfh dump_bench_rvv` | RVV instructions present and attribution status recorded |
| A4 board smoke / repeated | `make -C test-rvv/features/fpfh board_smoke` then `board_repeated` | board test pass and repeated compare summary exists |
| A5 Evidence Doctor readiness | `make -C test-rvv/features/fpfh evidence_doctor_repeated` | Errors / Warnings / Suggestions recorded or metadata downgrade explained |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic` for `public_fpfh_k`; `component_ablation` for SPFH / weighted SPFH helper cases. |
| A/B boundary | Std build vs RVV build of the same topic-local wrapper; no production dispatch yet. |
| 当前决策问题 | RVV-vs-scalar feasibility and component attribution, not production adoption. |
| diagnostic 是否可外推到 production | 部分可外推。public case 真实调用 production header，但 synthetic cloud、点型、search policy 和无 production dispatch 使它不能替代 production direct。 |
| comparison-boundary / baseline mismatch 风险 | 中等。component cases 不包含 search 和 SPFH lookup；public case 包含 search 但没有新增 RVV family。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes, only if component evidence is stable positive and mismatch audit shows public dilution can be addressed by a narrow production candidate. |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes, once a production RVV family exists. Phase 000 还没有 adopted RVV family。 |

## 板卡复跑预算和决策桶

Phase 000 先跑 board smoke，再跑 5-run repeated。若 Evidence Doctor 或 summary 显示方向接近阈值、
长尾严重或 `B/A < 1` 频率异常，最多追加一次同边界确认复跑；若 budget 用尽后仍摇摆，标为
`unstable` 并降级 EvidenceDecision。

默认 decision bucket：

- `positive`: median speedup >= 1.10 且无方向反转。
- `weak_positive`: median speedup 在 1.03 到 1.10 之间且无方向反转。
- `neutral`: median speedup 接近 1.00 或不同 case 互相抵消。
- `negative`: median speedup < 0.97 且无测量污染解释。
- `unstable`: 方向跨越 1.00 或 Evidence Doctor Warning 未解释。

## 文档更新清单

- 更新 `doc/fpfh-evaluation.zh.md` 的当前状态和证据链。
- 写 `result.zh.md`，回填每个 action 的命令、证据路径和 EvidenceDecision。
- 更新 `doc/phases/optimization-matrix.zh.md` 和 `doc/optimization-roadmap.zh.md`。
- 若没有 adopted production behavior，继续不创建 `doc-rvv/features/fpfh-RVV.zh.md`。

## 继续 / 停止条件

默认下一步是执行 A2-A5。只有板卡不可达、工具链失败、证据矛盾、dirty isolation 不安全、
或继续需要 production patch 用户确认时才停止。若 Phase 000 只完成 scaffold 或 QEMU correctness，
但 board 已可用且当前矩阵需要板卡证据，不得把“等待板卡验证”写成合法终点。
