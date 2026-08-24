# Phase 051 计划：more generic xyz AoS point types

## 阶段意图和边界

本阶段继续优化矩阵中 `generic-point-type-expansion`（泛型点型扩展）的证据边界：在不修改 production 源码的前提下，补更多 PCL 常见 xyz AoS 点型的 public ordered path correctness（公开顺序入口正确性）和 QEMU smoke（QEMU 冒烟验证，只证明构建、日志形状和路径，不证明性能）。

冻结范围：

- entry：ordered-cloud-pair public overload。
- row source：ordered only；不覆盖 source-indexed、dual-indexed 或 correspondence。
- point type：在既有 `PointXYZI` / `PointXYZRGB` 代表点型外，新增更多 `RVVXYZAoSFloatLayout` 命中的 PCL 常见 xyz AoS 点型组合。
- `Scalar`：只覆盖 `float`；`Scalar=double` 仍需要独立数值预算。
- production：不改 dispatch / fallback / public API；只验证已采纳 traits-gated xyz AoS gate 的更多实例。

## 当前状态清单

| 项目 | 当前状态 | 路径 |
| --- | --- | --- |
| production gate | 已采纳 ordered / row-source direct-fused RVV path，gate 为 dense、`float`、traits-gated xyz AoS。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| generic public evidence | Phase 040 correctness scout + Phase 041 board / ASM 已覆盖 `PointXYZI`、`PointXYZRGB` 和 mixed target 代表组合。 | `doc/phases/040-generic-point-type-expansion/result.zh.md`、`doc/phases/041-generic-point-type-public-board-asm/result.zh.md` |
| 当前 correctness | Phase 050 后 Std/RVV 各 15 tests passed；本阶段完成后预期更新为 16 tests。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log`、`log/evidence_registry.json` |
| roadmap 状态 | 更多 xyz AoS 点型需要另开 phase；`Scalar=double` 为 scope guard。 | `doc/optimization-roadmap.zh.md` |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `more-generic-xyz-aos-point-types` | 已采纳 production gate 只读取 x/y/z，更多 PCL 常见 xyz AoS 点型应与同构标量 reference 对齐。 | 某些点型 traits / POD layout 不满足 RVV gate；补 correctness 不能直接证明全部自定义点型的板卡性能。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `more-generic-xyz-aos-point-types` | ordered-cloud-pair | additional PCL xyz AoS / `float` | public ordered overload | `run_test_compare` 新增 correctness case | 新增独立 `more-generic-xyz-aos-point-types-public` QEMU smoke label | not required in this phase；不写性能结论 | QEMU manifest 继续引用 bench RVV asm | QEMU smoke Doctor | pending |

## 实现和测试动作

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| A1 point type scout | `src/test_tesvd_scale.cpp` | 新增点型 static_assert 和 public path reference 对拍。 |
| A2 bench smoke labels | `src/bench_tesvd_scale.cpp` | `more-generic-xyz-aos-point-types-public` case-filter 输出新增点型 label 和 checksum。 |
| A3 manifest / registry | `script/generate_tesvd_scale_qemu_evidence_manifest.py`、`Makefile` | manifest 能解析新增 label；registry doc-ref 指向 Phase 051 result。 |
| A4 证据运行 | QEMU correctness、QEMU smoke、Evidence Doctor、registry | Std/RVV correctness 通过；QEMU smoke Doctor 无 Error；registry fresh。 |
| A5 文档同步 | result、matrix、roadmap、topic-local docs | 记录新 evidence role 和不外推边界。 |

## Evidence Doctor 和 Registry

QEMU smoke 只用于构建、日志形状、checksum 和 manifest metadata。若 Evidence Doctor 出现 Error，先修复 manifest 或 label；若只有 QEMU timing 方向变化，不写性能结论。

## 完成条件

- `adopted-evidence-boundary`：新增点型 correctness 通过、QEMU smoke / Doctor / registry fresh，并写清它只是扩大 public generic correctness / smoke 证据边界。
- `attempted`：某些点型 traits gate 不满足或 correctness 不通过，记录点型和原因，不回推否定已采纳代表点型。
- `turn_stop_deferred`：若继续需要 board repeated、更多自定义点型或 `Scalar=double`，停止在用户选择点。

## 继续 / 停止条件

本阶段完成后，若没有新的当前授权内 candidate family，默认进入提交 / review 判断。更多自定义点型、row-source 泛型点型的更广覆盖和 `Scalar=double` 都作为独立 scope。

## 文档更新清单

更新 `result.zh.md`、`optimization-matrix.zh.md`、`optimization-roadmap.zh.md`、`README.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md`、`doc/testing-overview.zh.md`、`doc/test-support-code-map.zh.md` 和适用的长期 `doc-rvv` 边界说明。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production-public correctness/smoke`，真实 public ordered overload，但 QEMU smoke 不代表性能。 |
| A/B boundary | Std public ordered scale vs RVV public ordered scale；新增点型只用于 correctness / smoke。 |
| 当前决策问题 | 已采纳 traits-gated xyz AoS gate 是否有更宽的常见 PCL 点型正确性支撑。 |
| diagnostic 是否可外推到 production | correctness 可支撑这些具体点型的 public 语义；QEMU timing 不可外推性能。 |
| comparison-boundary / baseline mismatch 风险 | low；仍是 public overload，同一 case-filter。 |
| weak / negative / unstable 时是否允许 bounded production probe | 不适用；本阶段不做性能采纳判断。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；没有新 RVV family 选择问题。 |
