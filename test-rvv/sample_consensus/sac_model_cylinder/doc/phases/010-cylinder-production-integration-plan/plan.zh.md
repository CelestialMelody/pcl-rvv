# Phase 010: cylinder production integration plan

## 阶段意图和边界

本阶段是 PI1 production integration plan（生产接入计划）。它只把 Phase 000 的
`partial-production-candidate`（局部生产候选）转成可审查的 production patch（生产补丁）计划，
冻结入口、fallback（回退路径）、dispatch（分流逻辑）、点型 / `Scalar` 边界和证据计划。

本阶段不修改 production 源码。PI2 若要改
`sample_consensus/include/pcl/sample_consensus/impl/sac_model_cylinder.hpp`，需要用户明确授权。

## 当前证据 baseline

| 来源 | 当前事实 | 对 PI1 的影响 |
| --- | --- | --- |
| Phase 000 result | test-only count/select RVV candidate correctness、asm、5-run board repeated 和 Evidence Doctor 已闭合；EvidenceDecision 为 `partial-production-candidate`。 | 允许设计有界 production probe（生产探针），不允许直接写 adopted production。 |
| Manifest / Doctor | `repeated-evidence-manifest.json`；Doctor 为 Errors=0、Warnings=1、Suggestions=0。 | production direct 必须重跑同边界 repeated board，并解释或消除 count long-tail warning。 |
| Production source | `countWithinDistance` / `selectWithinDistance` 当前为模板公开入口，读取 `PointT` xyz 和 `PointNT` normal。 | production gate 不能只按 `PointXYZ + Normal` 诊断结果外推到泛型。 |
| Generic point type strategy | xyz 和 normal 必须分别用 traits / AoS layout gate 证明；exact gate 只能作为阶段性例外。 | PI2 设计默认应优先使用 `RVVXYZAoSFloatLayout<PointT>` 和 normal field gate；不能写死 `PointXYZ` offset。 |

## 生产候选范围

| 维度 | PI1 冻结范围 | 不覆盖范围 |
| --- | --- | --- |
| public entry（公开入口） | `countWithinDistance` 和 `selectWithinDistance`。 | `getDistancesToModel`、`optimizeModelCoefficients`、`projectPoints`、`doSamplesVerifyModel`。 |
| row source（行来源） | 当前 `indices_` direct indexed path。 | 其它上游 policy 或非标准 index 语义。 |
| point type / layout（点型 / 布局） | 生产计划优先 traits-gated xyz + normal AoS float path；若 normal gate 或 wrapper 不足，PI2 必须收窄为阶段性 exact gate 并记录 fallback。 | custom layout、非 float 字段、POD / standard-layout 不满足、未证明 normal-like 泛型。 |
| `Scalar` / 系数 | 当前源码使用 `Eigen::VectorXf` model coefficients；RVV 内部按 float 计算。 | `Scalar=double` 或替代系数类型不在本阶段。 |
| 输出 | `count` mask popcount；`select` 保序写 `inliers` 和 `error_sqr_dists_`。 | dense distance vector 写回留给后续 `getDistancesToModel` phase。 |

## 候选实现形态

PI2 候选应最小拆分生产路径：

1. 抽出或保留清晰的 `countWithinDistanceStd` / `selectWithinDistanceStd` 等价标量 helper，公开入口只做已有
   `isModelValid` 检查、RVV 尝试和 fallback 调用。
2. 在 `__RVV10__` 下提供 `countWithinDistanceRVV` 和 `selectWithinDistanceRVV`，复用 Phase 000 candidate 的
   indexed xyz/normal gather、radial norm、normal angle、`vcpop.m` 和 `vcompress.vm` 形态。
3. 生产 RVV load/store 优先复用 `pcl::rvv_load::indexed_load3_f32m2`、`indexed_load3_fields_f32m2` 和
   `rvvMaxU32ByteOffsetElements`；normal field gate 可以复用现有公共 traits，如果公共 API 不足则只在当前文件新增小型本地
   normal AoS gate，并在 Handoff 写清后续公共化条件。
4. `select` 的 RVV helper 必须先 resize 预分配输出，然后按 `vcompress` 活跃 lane 数缩回，保证 chunk 内顺序与
   `indices_` 扫描顺序一致。
5. 对小规模、布局不满足、32-bit byte offset 不可表示、`pcl::index_t` 非 int32 signed 或 `__RVV10__` 关闭的构建，自然回退标量。

## Fallback 矩阵

| gate | 期望行为 | PI2 / PI3 测试要求 |
| --- | --- | --- |
| 非 RVV 构建 | 只编译和运行 Std helper。 | Std build correctness 通过。 |
| 点型缺少 xyz float AoS | 编译期不进入 RVV helper，走 Std。 | 若当前 topic 无合适自定义点型 fixture，先写 not_applicable with evidence，并保留 expansion queue。 |
| normal 点型缺少 normal_x/y/z float AoS | 编译期回退 Std。 | production fallback test 或代表性 compile-time fixture。 |
| cloud size 超出 32-bit byte offset | RVV helper 内运行期回退 Std。 | 可用小型 wrapper 单元测试或代码审查闭合；不强造超大内存 case。 |
| `pcl::index_t` 不是 signed int32 | `select` 回退 Std，避免 `vse32` 写错。 | 代码路径审查 + static condition。 |
| 小规模输入 | 可走 RVV 或 Std，但必须保持输出一致。 | 增加 tail / small-size gtest。 |
| model invalid | 保持现有 public 行为：count 返回 0，select 清空输出。 | 复用现有 public tests 或补 production direct gtest。 |

## Evidence plan

| 证据层 | PI3 / PI4 target | 完成判据 |
| --- | --- | --- |
| production direct correctness（真实生产路径正确性） | 新增或扩展 `src/test_sac_model_cylinder.cpp`，通过 public entry 命中 RVV 和 fallback。 | Std/RVV 对拍通过，select 顺序和 `error_sqr_dists_` 近似保持。 |
| QEMU | `make run_test_compare`、必要的 QEMU smoke。 | 只证明正确性、构建和日志形状。 |
| production asm | 新增 `check_production_asm` 或扩展现有 checker。 | RVV 指令归属到 production helper、public overload 或明确 inline boundary。 |
| production board repeated | 新增 production repeated manifest / doctor，或扩展 Phase 000 wrapper 生成新的 run label。 | public count/select Std/RVV 5-run repeated positive；Evidence Doctor 无 Error。 |
| registry freshness | `record_*_production*_evidence_state` 与 `repeated_evidence_status`。 | manifest / doctor / registry 被 phase result、evaluation 和 README 引用。 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 000 是 `production_shaped_diagnostic`；PI4 必须升级为 `production-public`。 |
| A/B boundary | Phase 000 是 test helper；PI4 必须是 public overload 或 production helper 同边界。 |
| 当前决策问题 | 是否能把 diagnostic candidate 转为有界 production probe，并用真实 public Std/RVV 证据决定是否进入 PI5。 |
| diagnostic 是否可外推到 production | 只能外推公式和访存组织；不能外推 dispatch、fallback、production symbol 和 public performance。 |
| comparison-boundary / baseline mismatch 风险 | 存在。PI4 的 public Std/RVV repeated 是必须证据，不可由 Phase 000 替代。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | Phase 000 为 positive；若 PI4 弱、负或不稳定，必须停在 PI5 等待用户确认回滚或继续验证。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前没有已采纳 cylinder RVV family；若 PI2 只迁移 Phase 000 family，不要求 RVV-vs-RVV family selection。若 PI2 新增替代 family，需要同边界 detail A/B。 |

## 板卡复跑预算和决策桶

PI4 默认 5-run production repeated。decision bucket 继续沿用 Phase 000 口径：median speedup >= `1.20x`
且无 Error 为 positive；`1.05x-1.20x` 为 weak_positive；`0.95x-1.05x` 为 neutral；低于 `0.95x` 为 negative；
跨桶摇摆或 Error 未解为 unstable / blocked。若 Doctor 显示长尾或方向接近阈值，最多追加一次同边界确认复跑，
除非用户明确扩大预算。

## 暂停条件

任一条件成立时不进入或停止 PI2-PI5：

- 用户尚未明确授权修改 production 源码。
- 需要扩大到 `getDistancesToModel`、其它 production 文件、public API、其它 topic、其它点型泛型采纳或 `Scalar=double`。
- traits / fallback gate 无法隔离，可能让未覆盖点型误命中 RVV。
- production direct correctness、production asm 或 board repeated 不能闭合。
- PI5 完成后，无论证据正向或负向，都必须等待用户确认采纳或回滚。

## 文档更新清单

PI1 result 或后续 PI4/PI5 result 必须同步：

- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/sac_model_cylinder-evaluation.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/test-support-code-map.zh.md`
- README

`doc-rvv/sample_consensus/sac_model_cylinder-RVV.zh.md` 仍为 not_applicable，直到 PI5 production evidence 通过并经用户确认采纳。
