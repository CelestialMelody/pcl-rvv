# Phase 010: vcompress select 消融计划

## 阶段意图和边界

本阶段只新增测试专用 `selectWithinDistanceVCompressCandidate`。目标是比较 Phase 000 的 scalar lane writeback（把 RVV distance 存到临时 buffer 后逐 lane 标量追加）和 `vcompress`（RVV 保序压缩）写回是否能进一步降低 `selectWithinDistance` 成本。阶段仍不修改 production（生产源码），不创建 `doc-rvv` 长期主题文档，不把消融结果写成 production direct（真实生产路径证据）。

## 当前状态清单

| area | current state | path |
| --- | --- | --- |
| Phase 000 result | count/select 测试专用候选在 `PointXYZ + Normal` 和 `PointXYZI + Normal` 单次板卡 smoke 为 positive diagnostic。 | `doc/phases/000-normal-sphere-count-select-diagnostic/result.zh.md` |
| 当前 select 候选 | RVV 计算每 lane distance，然后 `vse32` 到临时 float buffer，再由标量循环 append inliers 和 double error。 | `include/impl/sac_model_normal_sphere_access.hpp` |
| benchmark 输出 | 当前只有 public 三入口和 diagnostic candidate 三入口；缺少 vcompress select label。 | `include/bench_sac_model_normal_sphere.h` |
| Evidence wrapper | 当前 case 字典只解析 Phase 000 labels；需要补 Phase 010 label 和输出路径参数。 | `script/generate_normal_sphere_evidence_manifest.py` |

## validated_scope / unvalidated_scope

| scope type | 内容 |
| --- | --- |
| validated_scope | `PointXYZ + Normal`、`PointXYZI + Normal`，direct indexed source/normal，`selectWithinDistance` 测试专用 helper。 |
| unvalidated_scope | production dispatch、`countWithinDistance`、`getDistancesToModel`、其它 source 点型、其它 normal 点型、`Scalar=double`、非 indexed 或自定义 layout。 |
| point_type_expansion_queue | Phase 010 完成后，若 `vcompress` 仍 positive，可考虑 `PointXYZRGB/RGBA + Normal` 或 production PI1；每项需要独立 correctness、fallback、asm、board 和 Evidence Doctor。 |
| phase_closeout_boundary | 只能关闭 `vcompress` select 消融条目；不能关闭 production adoption（生产采纳）或泛型模板结论。 |

## Diagnostic 到 Production 错配审计

| question | answer |
| --- | --- |
| evidence role | component ablation（组件消融）+ production-shaped diagnostic。 |
| A/B boundary | 同一 test-only access wrapper 内，当前 scalar writeback RVV candidate 对 `vcompress` RVV candidate。 |
| 当前决策问题 | implementation-shape（实现形态选择），不是生产接入决策。 |
| diagnostic 是否可外推到 production | 不能直接外推；只用于判断 select 写回实现族是否值得进入 PI1 审计。 |
| comparison-boundary / baseline mismatch 风险 | 若只看 Std/RVV speedup 会有风险；本阶段必须优先看同一 RVV build 内的 candidate A/B，或在 manifest 中明确降级。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 可以保留 Phase 000 scalar writeback 候选作为后续 PI1 备选；不能直接拒绝整个 normal-sphere production probe。 |
| clean adoption 是否需要 production boundary 内 RVV-vs-RVV detail A/B | 需要。若 Phase 010 正向，production clean adoption 仍必须在 PI1-PI5 内重做真实公开入口证据。 |

## 实现和测试动作

| action | files | command / evidence | completion |
| --- | --- | --- | --- |
| TDD RED：新增 vcompress correctness case | `src/test_sac_model_normal_sphere.cpp` / `include/test_sac_model_normal_sphere.h` | `make -C test-rvv/sample_consensus/sac_model_normal_sphere run_test_rvv` 预期因 helper 缺失而编译失败。 | 失败原因指向 `selectWithinDistanceVCompressCandidate` 缺失。 |
| 实现测试专用 helper | `include/impl/sac_model_normal_sphere_access.hpp` | `run_test_compare` | vcompress candidate 与 scalar reference 保持 inlier 顺序和 error double 输出。 |
| 增加 bench label | `include/bench_sac_model_normal_sphere.h` | board / QEMU smoke 输出 `diagnostic candidate vcompress selectWithinDistance` | label 可被 manifest parser 解析。 |
| 刷新 manifest wrapper | `script/generate_normal_sphere_evidence_manifest.py` | `generate_evidence_manifest` 或 Phase 010 专用参数 | Evidence Doctor 能区分 Phase 000 scalar writeback 和 Phase 010 vcompress label。 |
| 反汇编归因 | `dump_bench_rvv` + `rg "vcompress\\.vm"` | RVV asm 中可见 `vcompress.vm`，并能归属到测试专用候选边界。 | 缺失则降级为 attempted。 |
| 板卡验证 | `board_smoke`，PointXYZ 与 PointXYZI 分开输出到 Phase 010 目录 | board summary、manifest、doctor、registry | 性能结论只来自板卡或目标硬件。 |

## Evidence Doctor 和 registry 规则

Phase 010 应生成独立 summary / manifest / doctor，避免覆盖 Phase 000 当前 truth。若临时复用 `generate_evidence_manifest`，必须通过参数指定 Phase 010 输出路径、run label 和输入目录。Evidence Doctor 若出现 Error，不能进入性能结论；若只有 `low_run_count` Warning，且 B/A 远离阈值，可作为消融初筛。

## 板卡复跑预算和决策桶

先执行 1-run board smoke。`vcompress` 对当前 scalar writeback 候选的 B/A 若在 `0.95x-1.05x`，或 PointXYZ 与 PointXYZI 方向相反，最多扩大到 5-run。decision bucket：`positive >= 1.20x`、`weak-positive 1.05x-1.20x`、`neutral 0.95x-1.05x`、`negative < 0.95x`、跨批次方向变化为 `unstable`。

## 继续 / 停止条件

板卡当前可用，Phase 010 不因“需要板卡”而停止。合法停止条件是编译工具链失败且不能本轮修复、板卡实际不可达、Evidence Doctor Error 无法降级、或继续需要修改 production 源码而尚未进入 PI1。若 `vcompress` positive，下一步是 PI1 production integration plan；若 neutral/negative，下一步是保留 Phase 000 scalar writeback 候选并审计是否仍值得 PI1。

## 文档更新清单

阶段结束时更新本 `result.zh.md`、`optimization-matrix.zh.md`、`optimization-roadmap.zh.md`、evaluation 和当前 Handoff。`doc-rvv/sample_consensus/sac_model_normal_sphere-RVV.zh.md` 仍为 `not_applicable`，直到出现用户确认采纳的 production behavior。
