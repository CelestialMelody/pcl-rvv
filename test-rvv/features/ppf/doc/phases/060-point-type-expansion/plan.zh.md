# Phase 060 Plan: Point type expansion

## 阶段意图和边界

本阶段把 Phase 040/050 已采纳的 `alpha_m` RVV production path（生产路径）从 exact
`pcl::PointXYZ + pcl::Normal + pcl::PPFSignature` 扩展为 traits-gated（基于字段特征门控）
的 `PointXYZ-like + Normal-like + PPFSignature`。目标是扩大当前模板入口的安全覆盖面，同时保持
其它模板实例自然 fallback（回退）到 `computePPFFeatureStd`。

本阶段准备验证的范围：

- 入口：public `PPFEstimation::compute` / `computeFeature`。
- row source（行来源）：当前 production 的 ordered `indices_ × input_` all-pairs。
- source 点类型：满足 `pcl::rvv::RVVXYZAoSFloatLayout<PointInT>::value`，且标量路径支持
  `getVector4fMap()` / `getVector3fMap()`。
- normal 点类型：满足本阶段 PPF 本地 normal AoS gate，且标量路径支持
  `getNormalVector4fMap()` / `getNormalVector3fMap()`。
- 输出：仍严格限制为 `pcl::PPFSignature`。
- `Scalar` / layout：float / AoS，使用当前点类型的 field offset（字段偏移）读取 staging 输入。

本阶段不验证：

- `Scalar=double`。
- 非 `PPFSignature` 输出。
- source-indexed、dual-indexed、correspondence 或其它 row source。
- 改写 `f1..f4` 的 `computePairFeatures` 标量部分。
- 新 RVV family（实现族）或 direct-AoS pair-feature revisit。

## 当前状态清单

| 项 | 当前状态 | 证据 |
| --- | --- | --- |
| adopted base path | exact `PointXYZ + Normal + PPFSignature` 已采纳，public board repeated 为 positive。 | `040-production-alpha-m-rvv-integration/result.zh.md`、`050-production-closeout-doc-rvv/result.zh.md`。 |
| source traits | `common/include/pcl/rvv_point_traits.h` 已有 `RVVXYZAoSFloatLayout<PointT>`。 | generic point type strategy 和 common traits 源码。 |
| normal traits | `RVVNormalFloatLayout<PointT>` 可证明 normal 字段；本阶段需补 PPF 本地 AoS gate，类似 PFH/VFH 的本地 helper。 | `features/include/pcl/features/impl/pfh.hpp`、`vfh.hpp`。 |
| current production code | RVV helper 仍 exact-gate `PointXYZ + Normal`，并直接读 `.x/.y/.z/.normal_x/...`。 | `features/include/pcl/features/impl/ppf.hpp`。 |
| current tests | 已有 exact trace test；缺少 `PointXYZI + Normal`、`PointXYZ + PointNormal` 命中 / fallback 测试。 | `test-rvv/features/ppf/src/test_ppf.cpp`。 |
| current bench | 只有 `public_ppf_compute` exact case。 | `test-rvv/features/ppf/src/bench_ppf.cpp`。 |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | 本阶段直接修改已存在 production helper，证据角色是 production-public 和 production-direct correctness。 |
| A/B boundary | Std build 的同一 public overload vs RVV build 的 traits-gated public overload。 |
| 当前决策问题 | `RVV-vs-scalar`：代表性 traits-gated 点型命中 RVV 后是否仍有板卡收益，是否值得扩大 production dispatch。 |
| diagnostic 是否可外推到 production | 不外推。Phase 040 的 exact 证据只证明基础 family；本阶段必须补新点型 production direct 测试和板卡 repeated。 |
| comparison-boundary / baseline mismatch 风险 | 新 bench case 必须用独立 label，避免把 exact `public_ppf_compute` 的旧 speedup 混作泛型扩展收益。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段就是 bounded production probe。若新代表点型 public case 非 positive，则不采纳扩展，改回或收窄 gate。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；本阶段没有替换已采纳 family，只扩大同一 `alpha_m` RVV family 的点类型 gate。 |

## point_type_expansion_queue

| 组合 | 意义 | 本阶段动作 |
| --- | --- | --- |
| `PointXYZI + Normal -> PPFSignature` | source 侧 xyz offset / stride 不再等同 exact `PointXYZ`，检验 source traits gate。 | production-direct trace、correctness、bench、asm、board repeated。 |
| `PointXYZ + PointNormal -> PPFSignature` | normal 侧 normal field offset / stride 不再等同 exact `Normal`，检验 normal traits gate。 | production-direct trace、correctness、bench、asm、board repeated。 |
| 其它满足 traits 的自定义点型 | 证明 compile-time gate 语义，但不逐个采集板卡数据。 | 文档中写成 gate-allowed but not individually board-covered。 |
| 不满足 traits 的点型或非 `PPFSignature` 输出 | 保持标量 fallback。 | RVV-only fallback / trace 不命中测试。 |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| A1 RED 测试 | `test-rvv/features/ppf/src/test_ppf.cpp`、必要的 fixture helper | RVV-only 新点型 trace 测试先失败，证明当前 exact gate 未覆盖。 |
| A2 production gate 扩展 | `features/include/pcl/features/impl/ppf.hpp` | RVV helper 引入 `pcl/rvv_point_traits.h`，使用 source xyz AoS gate + normal AoS gate + exact `PPFSignature` 输出 gate。 |
| A3 staging 字段读取改造 | `features/include/pcl/features/impl/ppf.hpp` | `dx/dy/dz` 和 `nx/ny/nz` 使用当前模板点类型的 offset 或 traits helper，不再依赖 exact 成员布局。 |
| A4 correctness / fallback | Make targets | `run_test_compare` Std/RVV 通过；新点型 production direct trace 命中；非 `PPFSignature` 或不满足 gate 的路径不命中。 |
| A5 bench case | `test-rvv/features/ppf/src/bench_ppf.cpp`、manifest script | 新增 `public_ppf_compute_pointxyzi_normal`、`public_ppf_compute_pointxyz_pointnormal` 独立 label。 |
| A6 asm / board / Doctor | Make targets | 反汇编可归属到新实例；5-run board repeated 和 Evidence Doctor 完成。 |
| A7 文档回填 | phase result、matrix、roadmap、evaluation、`doc-rvv`、Handoff | positive 才写 adopted；否则写 attempted / rollback-to-exact。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | bench / board | asm | Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `ppf-point-type-expansion` | ordered `indices_ × input_` | traits-gated source xyz AoS + normal AoS + exact `PPFSignature` / float | planned | planned | planned | planned | planned |

## Fallback 矩阵

| 条件 | 预期行为 | 验证 |
| --- | --- | --- |
| `__RVV10__` 未启用 | 不编译 RVV helper，直接走 `computePPFFeatureStd`。 | Std build correctness。 |
| source 不满足 `RVVXYZAoSFloatLayout` | RVV helper compile-time 返回 `false`。 | representative fallback test 或 static assertion。 |
| normal 不满足 PPF normal AoS gate | RVV helper compile-time 返回 `false`。 | representative fallback test 或 static assertion。 |
| 输出不是 `pcl::PPFSignature` | RVV helper compile-time 返回 `false`。 | RVV-only trace 不命中测试。 |
| 输入规模为 0 或 identity pairs | 输出语义保持原 production NaN / non-dense 行为。 | 既有 correctness + 新点型 correctness。 |

## 板卡复跑预算和决策桶

计划使用 5-run repeated board，沿用 Phase 040 的 public case 规模：

```text
--side 28 --index-count 64 --repeat 8 --iterations 8 --warmup 2
```

为了隔离本阶段决策，`BENCH_ARGS` 使用新 case filter：

```text
--case-filter public_ppf_compute_pointxyzi_normal
--case-filter public_ppf_compute_pointxyz_pointnormal
```

决策桶：

- `positive`：两个新 public case 的 mean / median 均大于 1，且没有跨桶摇摆；可写 adopted。
- `weak_positive`：收益存在但接近 1，按用户“板卡有收益即可采纳”可考虑采纳，但必须说明实现小、fallback 简单和风险低。
- `neutral / negative`：任一核心代表点型 public case 不优于标量；不采纳 traits 扩展，改回 exact 或收窄到 positive 组合。
- `unstable`：预算内跨桶摇摆；降级为 blocked / 需要人工判断。

Evidence Doctor（证据体检）必须在 EvidenceDecision 前运行。metadata / binary identity suggestion
不阻塞稳定 positive bucket；Error 或未解释 Warning 阻塞采纳。

## 继续 / 停止条件

若 A1-A6 全部通过且板卡为 positive，本阶段将 traits-gated point type expansion 写成 adopted production
behavior，并同步 `doc-rvv/features/ppf-RVV.zh.md`。若 correctness、asm、board 或 Doctor 阻塞，停止在
Phase 060 result，保留 Phase 050 的 exact adopted boundary，说明不建议继续扩大点类型范围的原因。

下一阶段默认入口由结果决定：

- positive：进入 Phase 061 evidence hardening 或 ready for review，取决于 matrix 是否仍有高优先级未阻塞项。
- non-positive：停止并整理 rollback / 收窄理由，不再继续泛型扩展。
