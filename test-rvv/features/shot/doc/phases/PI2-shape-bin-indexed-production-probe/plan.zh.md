# PI2 plan: shape-bin indexed production probe

## 阶段意图和边界

本阶段在 `features/include/pcl/features/impl/shot.hpp` 中接入一个有界 production probe（生产探针），只覆盖 `createBinDistanceShape` 的 indexed normal gather（按索引离散加载 normal 字段）、finite mask（有限值掩码）、dot/clamp 和 NaN 计数。它继续 PI1 的范围，不修改公开 API，不触碰 LRF、interpolation（插值）、color LAB/RGB、OpenMP 或跨 topic 公共接口。

用户已允许在接入后用板卡结果判断是否值得采纳；本阶段默认连续推进 PI2-PI5。PI5 无论正负都必须停在用户检查点：证据支持时等待用户确认保留 / 采纳，证据不支持时等待用户确认回滚。正式 `doc-rvv/features/shot-RVV.zh.md` 只在 PI5 生产证据支持且用户确认采纳后创建或刷新。

`validated_scope`：`SHOTEstimation` / `SHOTColorEstimation` 通过 `createBinDistanceShape` 共享的 shape-bin 路径；`PointNT` 满足 PCL normal traits（点类型字段特征）和 AoS（结构数组）布局 gate；`pcl::Indices` 已由上游 search 语义保证有效；输出为 `std::vector<double>`；`Scalar` 不作为模板参数扩展。

`unvalidated_scope`：非 normal 点类型、normal 字段非单个 `float` 的点类型、`normals_` 点云超过 32-bit byte offset 表达范围、极小邻域、indices / correspondences 的其它入口族、完整 interpolation / color 路径收益、其它 public API 或泛型 source/target 组合。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 040 是 `diagnostic`，本阶段新增 `production-detail` 和 `production-public`。 |
| A/B boundary | Phase 040 为 test helper；PI2 patch 后新增 production detail helper `createBinDistanceShape` 与 public overload `SHOTEstimation::computeFeature` / `SHOTColorEstimation::computeFeature`。 |
| 当前决策问题 | 先回答 RVV-vs-scalar（当前 RVV production path 是否快于标量），不回答 RVV-family-selection（实现族选择）。 |
| diagnostic 是否可外推到 production | 只能作为接入动机。Phase 040 覆盖 indexed normal AoS、NaN count 和 double bin 输出，但不覆盖真实 `normals_` / `frames_` 对象状态、`PCL_WARN` 副作用和 public entry 其它成本。 |
| comparison-boundary / baseline mismatch 风险 | 有。component bench 只测 shape-bin batch，public bench 还包含 search、interpolation、descriptor normalize 和 color path。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前 shape-bin indexed diagnostic 为正向，允许本阶段窄接。若 production public 或 production detail 板卡证据为 neutral / negative / unstable，PI5 必须暂停，不能自行采纳或回滚。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有既有 SHOT production RVV family，因此 PI5 以 production Std/RVV 为主；若后续另有 normalization 或 color production family 进入同一路径，再补 RVV-vs-RVV detail A/B。 |

## Fallback / dispatch 矩阵

| gate | production 行为 | PI5 前证据 |
| --- | --- | --- |
| `__RVV10__` 未定义 | 编译期只保留原标量 helper。 | Std build correctness。 |
| `PointNT` 不满足 normal AoS layout gate | 回退标量 `createBinDistanceShapeStd`。 | `PointXYZ -> PointNormal` 或等价 non-`pcl::Normal` direct test；构建证明模板实例可 fallback。 |
| `normals_` 为空或点云过大 | 回退标量；过大 gate 使用 `pcl::rvv::rvvMaxU32ByteOffsetElements<PointNT>()`。 | direct fallback test 或文档边界说明。 |
| 邻域规模小于阈值 | 回退标量，避免 VL setup 和 offset staging 成本吞掉收益。 | 小邻域 direct correctness；板卡 public run 判断阈值是否值得保留。 |
| NaN / Inf normal | RVV 写出 quiet NaN，`vcpop` 统计无效 lane，入口保留原 `PCL_WARN` 文本和比例。 | direct helper test 覆盖 NaN count；public tests 保持 descriptor 语义。 |
| color SHOT | 只通过共享 shape-bin 路径命中；color LAB/RGB 仍标量。 | SHOT1344 public correctness + board bench。 |

## 实现和测试动作

| action | artifact | done criteria |
| --- | --- | --- |
| 抽出标量 helper | `createBinDistanceShapeStd` 保留原循环和 warning 语义。 | 非 RVV build 与原 public tests 一致。 |
| 添加 RVV helper | `createBinDistanceShapeRVV` 使用 traits offset + `pcl::rvv_load::indexed_load3_fields_f32m2`，返回是否命中。 | RVV build 通过 direct test，asm 看到 `vluxei32.v` / `vcpop.m`。 |
| 添加 production direct tests | `test-rvv/features/shot/src/test_shot.cpp` 通过派生类调用 protected helper，覆盖 finite/clamp/NaN 和 fallback 点型。 | `run_test_shape_bin` 与 `run_test_compare` 通过。 |
| 添加 production direct bench case | `production_shape_bin_direct` 通过真实 helper 计时，不再使用 test-only component helper。 | board compare 可解析 checksum 和 timing。 |
| 更新 manifest/alias | case metadata、board alias 和 registry doc refs 标注 `production-detail`。 | Evidence Doctor 可读取 manifest；registry fresh。 |

## 板卡复跑预算和决策桶

先运行 production-detail `production_shape_bin_direct` 和 public `public_shot352_fixed_lrf` / `public_shot1344_fixed_lrf` targeted board compare。若 production-detail 与至少一个 public case bucket 同向 positive，最多追加 2 次同边界复跑；若 public 仍约 1.00x 但 production-detail 明显 positive，PI5 标为 explicit probe（显式探针）并等待用户判断是否因局部热点收益保留。若 production-detail 也 neutral / negative，PI5 标为 pending rollback confirmation。

默认 bucket：

- `positive`: median speedup >= 1.10x，且没有 checksum Error。
- `weak_positive`: 1.03x <= median speedup < 1.10x，且实现小、Warnings 可解释。
- `neutral`: 0.98x <= speedup < 1.03x。
- `negative`: speedup < 0.98x。
- `unstable`: 复跑预算内跨 positive / negative 或 Evidence Doctor Error 未能修复。

## Evidence Doctor 和 registry 规则

生产证据必须重新生成 topic-local manifest 和 Evidence Doctor（证据体检）结果。`production_shape_bin_direct` 的 metadata 使用 `production-detail` / `production detail helper`；public SHOT cases 使用 `production-public` / `public overload`。Errors 必须为 0 才能支撑生产采纳；Warnings 必须在 result、evaluation 和 Handoff 中解释或降级。

## 继续 / 停止条件

继续到 PI2-PI5，除非命中以下条件：

- production patch 需要扩大到 public API、interpolation、color、LRF、OMP 或其它 topic；
- traits / layout gate 无法让非覆盖点类型自然 fallback；
- correctness、asm 或板卡 Evidence Doctor 出现无法修复的 Error；
- 板卡不可达或复跑预算耗尽后 bucket 仍 unstable；
- dirty isolation 显示用户改动会被覆盖。

PI5 完成后必须停止在用户确认点，不进入 S11 closeout，不创建正式 `doc-rvv` production 文档。

## 文档更新清单

本阶段完成后更新：

- `result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/shot-evaluation.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/test-support-code-map.zh.md`
- `README.zh.md`
- `doc-rvv/library-screening/features/features-function-evaluation-queue.zh.md`
- current Handoff Packet（默认 local-only）

`doc-rvv/features/shot-RVV.zh.md`：本阶段只记录为 `pending_user_confirmation_adopt_production` 的候选动作，不在用户确认前创建。
