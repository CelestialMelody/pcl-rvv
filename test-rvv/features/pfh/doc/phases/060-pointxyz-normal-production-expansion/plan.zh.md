# Phase 060 Plan: PointXYZ + Normal production expansion

## 阶段意图和边界

本阶段把 Phase 040/050 已采纳的 direct AoS RVV production helper 扩展到常见组合
`PFHEstimation<pcl::PointXYZ, pcl::Normal, pcl::PFHSignature125>`。这是一个新的 point-type
expansion（点类型扩展）phase；它不能复用 exact `PointNormal -> PointNormal` 的板卡数字来关闭结论。

本阶段准备验证的范围：

- 入口：`computePointPFHSignature` 和 public `PFHEstimation::compute`。
- 点类型：exact `pcl::PointXYZ` source cloud + exact `pcl::Normal` normals cloud。
- 数据读取：source 侧只读 xyz；normal 侧只读 `normal_x/normal_y/normal_z`。
- `Scalar` / 输出：float field，`PFHSignature125` 默认 5x5x5 histogram。
- 运行条件：`__RVV10__`、`use_cache_ == false`、`nr_split == 5`、`indices.size() >= 4`、
  source / normal cloud 各自满足 32-bit byte offset gate。

本阶段不验证：

- PointXYZ-like / Normal-like 泛型集合。
- `PointXYZI`、`PointXYZINormal`、自定义点类型或 `PointNormal` 以外的复合点型。
- `use_cache_ == true`、OMP path、非默认 bins、`Scalar=double`。

## 当前状态清单

| 项 | 当前状态 | 证据 |
| --- | --- | --- |
| adopted base family | direct AoS helper 已在 exact `PointNormal -> PointNormal` 下采纳。 | Phase 040/050。 |
| source traits | `common/include/pcl/rvv_point_traits.h` 已有 `RVVXYZAoSFloatLayout<PointT>`。 | common traits 文档和源码。 |
| normal traits | 已有 `RVVNormalFloatLayout<PointT>`，但没有公共 normal AoS layout gate。 | 本阶段用 PFH 本地 helper 收窄到 exact `pcl::Normal`。 |
| tests | 当前只有 `PointNormal` production helper gtest。 | `test-rvv/features/pfh/src/test_pfh.cpp`。 |
| bench | 当前 bench 只有 `PointNormal` component/public case。 | `test-rvv/features/pfh/src/bench_pfh.cpp`。 |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | 本阶段将生成 production-detail 和 production-public evidence；接入判断只看 `PointXYZ + Normal` 新证据。 |
| A/B boundary | Std build 原标量 `PFHEstimation<PointXYZ, Normal>` vs RVV build 同一 production helper / public overload。 |
| 当前决策问题 | `RVV-vs-scalar`：`PointXYZ + Normal` 是否值得纳入当前 production dispatch。 |
| diagnostic 是否可外推到 production | 不外推。已有 `PointNormal` production evidence 只说明同一 direct AoS family 可维护；本阶段必须重跑生产直连测试和板卡。 |
| comparison-boundary / baseline mismatch 风险 | 新 bench case 必须同时存在 component 和 public label，避免把旧 `PointNormal` case 混入新点型采纳结论。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段已经是 bounded production probe；若 repeated board 低于 positive bucket，不能采纳该点型扩展。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要。扩展沿用已采纳 family，决策问题是新点型是否 RVV-vs-scalar 正向。若另换实现 family 才需要 A/B。 |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| A1 扩展 production gate | `features/include/pcl/features/impl/pfh.hpp` | helper 支持 exact `PointNormal -> PointNormal` 和 exact `PointXYZ + Normal`；其它类型仍 fallback。 |
| A2 新增 correctness test | `test-rvv/features/pfh/src/test_pfh.cpp` | `PointXYZ + Normal` production helper 命中 RVV 并与标量 reference 对拍；小规模 / 非默认 bins fallback 可单独验证。 |
| A3 新增 bench case | `test-rvv/features/pfh/src/bench_pfh.cpp` | 新增 component 和 public `PointXYZ + Normal` case label，checksum 输出可被 summary/manifest 解析。 |
| A4 本地验证 | Make targets | `run_test_compare`、`dump_bench_rvv` 通过；asm 能归属到 production helper。 |
| A5 板卡验证 | board repeated | 使用新 output dir 运行 5-run repeated，生成 manifest、Doctor 和 registry 检查。 |
| A6 文档回填 | phase result、matrix、roadmap、`doc-rvv`、Handoff | 只在新证据 positive 后把 `PointXYZ + Normal` 写成 adopted；否则保持 fallback。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback | bench / board | asm | Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `pfh-pointxyz-normal-production-expansion` | fixed KSearch neighborhood indices | exact `PointXYZ + Normal`, float, source xyz AoS + normal AoS | planned | planned | planned | planned | planned |

## Fallback 矩阵

| 条件 | 预期行为 | 验证 |
| --- | --- | --- |
| 非 `PointNormal -> PointNormal` 且非 `PointXYZ + Normal` | helper compile-time 返回 `false`。 | 保持现有 Std/RVV tests；必要时新增 negative helper test。 |
| `nr_split != 5` | helper 返回 `false`，公开入口走标量。 | RVV-only fallback gtest。 |
| `indices.size() < 4` | helper 返回 `false`。 | RVV-only fallback gtest。 |
| source / normals 越界或负 index | helper 返回 `false`。 | 不新增未定义输入 public case；helper gate 可直接测。 |
| cache path | public entry 不尝试 RVV。 | 不扩大本阶段。 |

## 板卡复跑预算和决策桶

计划使用 5-run repeated board，`side=32`、`k=32`、`iterations=8`、`warmup=2`，新目录建议为
`test-rvv/features/pfh/log/board/pi3-pointxyz-normal/repeated`。决策桶：

- `positive`：component 和 public case mean / median 均明显大于 1，且无 `<1x` run。
- `weak_positive`：mean / median 大于 1，但存在少量 `<1x` 或 public 收益接近阈值；需要解释是否值得接入。
- `neutral / negative`：public case 接近 1 或低于 1；不采纳该点型扩展。
- `unstable`：预算内跨桶摇摆；降级为 blocked / 需要人工判断。

Evidence Doctor 必须在 EvidenceDecision 前运行。若只有 metadata suggestion 且 direction 稳定，可以不阻塞；
若出现 Error 或未解释 Warning，不能写成 adopted。

## 继续 / 停止条件

若 A1-A5 通过且板卡为 positive，本阶段可把 exact `PointXYZ + Normal` 写入 adopted scope，并刷新
`doc-rvv/features/pfh-RVV.zh.md` 的范围决策表。若 correctness、asm 或 board 失败，则保持 fallback，
写清原因并停止在 Phase 060 result；不得回滚 Phase 040/050 已采纳的 `PointNormal` 路径。
