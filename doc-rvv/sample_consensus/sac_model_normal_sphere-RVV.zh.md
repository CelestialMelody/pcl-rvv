# SampleConsensusModelNormalSphere RVV 生产实现

## 当前生产状态

`SampleConsensusModelNormalSphere<PointT, PointNT>` 当前已采纳三条 production RVV（生产 RVV）路径：

- `selectWithinDistance`
- `countWithinDistance`
- `getDistancesToModel`

当前采纳范围是 direct indexed `indices_`（直接索引输入）、`PointXYZ + Normal`、`PointXYZI + Normal`、
`PointXYZRGB + Normal` 和 `PointXYZRGBA + Normal`，其中 source 点型必须满足
`pcl::rvv::RVVXYZAoSFloatLayout<PointT>`，normal 点型当前收窄为 `pcl::Normal`。生产 gate（门控条件）还要求
RVV 构建、signed 32-bit `pcl::index_t`、source / normal cloud 都能用 32-bit byte offset（字节偏移）表达，
normal cloud 覆盖 input cloud，并且 `indices_` 规模不小于 16。其它 normal 点型、自定义 registered point type
（已注册自定义点型）、`Scalar=double`、其它 layout（布局）、非法 index、真实 RANSAC workload（工作负载）
和其它硬件需要单独证据，不能从本次板卡结果外推。

## 稳定证据索引

| 证据 | 路径 / 命令 | 作用 |
| --- | --- | --- |
| Phase 060 result | `test-rvv/sample_consensus/sac_model_normal_sphere/doc/phases/060-production-integration-execution/result.zh.md` | 记录 production integration loop（生产接入闭环）执行、PI5 决策和文档 closeout。 |
| production summary | `test-rvv/sample_consensus/sac_model_normal_sphere/doc/phases/060-production-integration-execution/board-evidence-summary.md` | 当前四种 source 点型 × 三条公开入口的 5-run board repeated（重复板卡测试）中位数摘要。 |
| production manifest | `test-rvv/sample_consensus/sac_model_normal_sphere/doc/phases/060-production-integration-execution/board-evidence-manifest.json` | Evidence Doctor（证据体检）输入，记录 A/B 边界、checksum、binary hash 和 run count。 |
| Evidence Doctor | `test-rvv/sample_consensus/sac_model_normal_sphere/doc/phases/060-production-integration-execution/board-evidence-doctor.md` | 当前结果为 Errors=0、Warnings=0、Suggestions=0。 |
| registry | `test-rvv/sample_consensus/sac_model_normal_sphere/log/evidence_registry.json` | 登记 Phase060 summary / manifest / Doctor，并支持 freshness（新鲜度）检查。 |
| correctness | `make -C test-rvv/sample_consensus/sac_model_normal_sphere run_test_compare` | Std/RVV 两个构建各 8 个 gtest 通过。 |
| asm | `make -C test-rvv/sample_consensus/sac_model_normal_sphere dump_bench_rvv` | RVV bench 反汇编可见 normal-sphere production helper 和关键 RVV 指令。 |

## 函数语义

normal-sphere 模型系数包含球心 `(cx, cy, cz)` 和半径 `radius`。三个公开入口都先检查 `normals_`、
`isModelValid` 和模型系数，然后按 `indices_` 同时索引 source cloud 与独立 normal cloud。每个样本计算：

```text
dir = point - center
weighted_euclid = (1 - normal_distance_weight) * abs(norm(dir) - radius)
d_normal = acute angle(normal[index], dir)
distance = abs(normal_distance_weight * d_normal + weighted_euclid)
```

`selectWithinDistance` 和 `countWithinDistance` 在标量路径中有 early continue（提前跳过）：当
`weighted_euclid > threshold` 时不再计算法线夹角。RVV 路径在 chunk（分块）内用同一球壳 mask
把不满足 early 条件的 lane（向量通道）合并为阈值距离，随后再执行最终阈值判断。`getDistancesToModel`
没有 early continue，会为每个 index 写回一个 double distance（双精度距离）。

| public entry | 输出语义 | 当前 production 状态 |
| --- | --- | --- |
| `countWithinDistance` | `weighted_euclid` 未超阈值且 `distance < threshold` 的点计数。 | RVV adopted |
| `selectWithinDistance` | 按 `indices_` 顺序写内点 index，并把 `distance` 写入 `error_sqr_dists_`。 | RVV adopted |
| `getDistancesToModel` | 为每个 index 输出一个 dense double distance（连续 double 距离）。 | RVV adopted |

`countWithinDistanceStandardNormalSphere`、`selectWithinDistanceStandardNormalSphere` 和
`getDistancesToModelStandardNormalSphere` 保存原标量主体，用于 fallback（回退路径）和 Std/RVV 对拍。

## 当前采用的优化方式

公开入口保留原有 `normals_` 和 `isModelValid` 检查。RVV 构建中，若 source xyz AoS（结构数组）布局、
normal 点型、index 类型、规模和 normal 覆盖条件都满足，入口短路进入对应 RVV helper；任一条件不满足时，
调用 Standard helper 保持原标量语义。

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| `countWithinDistanceRVVNormalSphere` | adopted | RVV indexed gather（索引离散加载）批量读取 source xyz 和 normal xyz，计算球壳距离与法线夹角，用 mask 和 `vcpop.m` 统计内点。 | Phase060 四组点型 median 为 `3.332x`、`3.358x`、`3.308x`、`3.411x`，Doctor 0/0/0。 | 只证明当前 public direct indexed 代表点型 bench。 |
| `selectWithinDistanceRVVNormalSphere` | adopted | `vcompress.vm` 保持 chunk 内命中 lane 的顺序，压缩写回 index；distance 用 `vfwcvt.f.f.v` 和 `vse64.v` 写 double。 | Phase060 四组点型 median 为 `3.039x`、`2.900x`、`2.973x`、`2.906x`，Doctor 0/0/0。 | 输出顺序由 gtest 和 checksum 保护。 |
| `getDistancesToModelRVVNormalSphere` | adopted | 没有阈值输出压缩，所有 lane 都计算 distance；结果拓宽为 double 后连续写入 `distances`。 | Phase060 四组点型 median 为 `4.456x`、`4.157x`、`4.290x`、`4.161x`，Doctor 0/0/0。 | 不证明其它 normal 点型或 `Scalar=double`。 |
| Phase 000-030 diagnostic helper | historical | 它证明候选值得进入 production probe（生产探针），但不能替代接入后的 evidence（证据）。 | 早期 single board smoke 均为正向，但有 `low_run_count` Warning。 | 不作为当前生产性能 truth。 |
| `pcl::Normal` exact normal gate | adopted narrow | 当前 public production evidence 只覆盖独立 `pcl::Normal` normal cloud。 | Phase060 四组 source 点型均使用 `pcl::Normal`。 | 其它 normal-like 点型需另开窄 phase。 |
| identity-index 专门路径 | not_now | 当前 shuffled indexed gather 已有 2.9x-4.5x 生产收益，没有 profile 证明 identity indices 是主 workload。 | Phase060 production-public 全正向。 | 只有真实 workload/profile 指向 identity indices 主导时重开。 |

## VL Chunk 流程

三个 RVV helper 共用 `computeNormalSphereDistanceRVV`：

1. `vsetvl` 选择当前 VL chunk（可变向量长度分块）。
2. 从 `indices_` 加载 signed 32-bit index，再转成 source / normal 的 byte offset。
3. 用 RVV load wrapper（向量加载封装）离散读取 source xyz 和 normal xyz。
4. 计算 `dir = point - center`、`norm(dir)`、球壳距离和 normal norm。
5. 将 `dir` 与 normal 归一化后调用 `getAcuteAngle3DRVV_f32m2` 计算锐角夹角。
6. 组合 `normal_distance_weight_` 与球壳距离，得到每个 lane 的 normal-sphere distance。

后半段按入口分流：count 用 `vcpop.m` 规约 mask；select 用 `vcompress.vm` 压缩命中的 index 和 distance；
getDistances 把每个 lane 的 distance 拓宽成 double 并连续写入 `distances`。

## 覆盖范围与 Fallback

| 条件 | 当前行为 | 证据 |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | 只编译并调用 Standard helper。 | Std 构建 `run_test_compare` 8/8。 |
| source xyz layout 不满足 | `if constexpr` 不实例化 RVV helper，回到 Standard helper。 | 源码 gate：`RVVXYZAoSFloatLayout<PointT>`。 |
| normal 点型不是 `pcl::Normal` | 回到 Standard helper。 | `RVVNormalSphereLayoutGate<PointT, PointNT>`。 |
| `pcl::index_t` 不是 signed 32-bit | 不进入 RVV helper，避免 index load / compressed store 类型不匹配。 | 源码 gate。 |
| input / normal 规模超过 u32 byte offset 上限 | RVV helper 返回 `false`，public entry 调用 Standard helper。 | 源码 gate。 |
| normal cloud 小于 input cloud | 三入口均回到 Standard helper。 | `ProductionFallbacksKeepPublicReferenceSemantics`。 |
| `indices_.size() < 16` | 三入口回到 Standard helper，避免小规模分流开销。 | `ProductionFallbacksKeepPublicReferenceSemantics`。 |
| 模型系数无效 | 保持原公开入口副作用：count 返回 0，select / getDistances 清空或不输出。 | public entry dispatch 前调用 `isModelValid`。 |
| `PointXYZ/PointXYZI/PointXYZRGB/PointXYZRGBA + Normal` | 命中当前 production RVV helper。 | Phase060 correctness、asm、5-run board repeated 和 Doctor。 |
| 自定义点型、其它 normal-like 点型、`Scalar=double` | 当前不采纳，继续走标量或保持未覆盖。 | 当前证据未覆盖。 |

## 范围决策表

| 范围 | 状态 | 证据 | 下一步 |
| --- | --- | --- | --- |
| `PointXYZ + Normal` direct indexed public 三入口 | adopted | Std/RVV correctness、production asm、5-run board repeated、Doctor 0/0/0。 | 当前 production 行为保留。 |
| `PointXYZI + Normal` direct indexed public 三入口 | adopted | 同上；Phase060 三入口 median 全部 positive。 | 当前 production 行为保留。 |
| `PointXYZRGB + Normal` direct indexed public 三入口 | adopted | 同上；颜色字段不参与公式，只验证 source xyz layout。 | 当前 production 行为保留。 |
| `PointXYZRGBA + Normal` direct indexed public 三入口 | adopted | 同上；Phase060 三入口 median 全部 positive。 | 当前 production 行为保留。 |
| 其它 normal 点型 | deferred | 当前 RVV gate 明确收窄到 `pcl::Normal`。 | 用户指定 normal 点型后补 traits、fallback、correctness、asm、board 和 Doctor。 |
| 自定义 source 点型 | deferred | 当前只验证 PCL 内置 PointXYZ-like source 点型。 | 用户指定点型或真实 workload 后另开扩展 phase。 |
| identity `indices_` 专门路径 | not_now | 当前 shuffled indexed gather 已强正向；没有 workload/profile 证明需要特殊路径。 | profile 或用户点名后做 RVV-vs-RVV A/B。 |
| `Scalar=double` | not_applicable now | 当前公开入口使用 `Eigen::VectorXf` 系数和 float point fields。 | 若上游签名或字段策略变化再评估。 |
| `optimizeModelCoefficients` / `projectPoints` / `doSamplesVerifyModel` | scalar-only | 不属于当前距离三入口 RVV scope。 | 需要独立 profile 和 phase。 |

## 标量路径与 RVV 路径差异

| 阶段 | Standard helper | RVV helper | 语义保持证据 |
| --- | --- | --- | --- |
| 模型有效性 | public entry 调用 `isModelValid`。 | public entry 先调用同一检查。 | 公开入口 tests 使用同一系数输入。 |
| 点和法线读取 | `input[index]` 和 `normals[index]`。 | index -> byte offset 后离散读取 source xyz / normal xyz。 | 乱序 indices gtest 和 bench checksum。 |
| 球壳距离 | Eigen `dir.norm()` 和 `std::abs`。 | RVV `vfsqrt.v` 和 `vfsgnjx` 计算。 | public-vs-Standard tests。 |
| normal angle | 标量 `pcl::getAngle3D` 后取锐角。 | 归一化 normal/dir 后调用 RVV acute angle helper。 | `ProductionRVVDetailHelpersMatchPublicReference`。 |
| count 输出 | 标量累加计数。 | `vcpop.m` 统计 inlier mask。 | production detail helper tests。 |
| select 输出 | `push_back` index 和 double distance。 | `vcompress.vm` 保序写回 index 和 compressed distance。 | select 顺序、error 对拍和 checksum。 |
| getDistances 输出 | 每个 index 写一个 double distance。 | `vfwcvt.f.f.v + vse64.v` 连续写回。 | dense distance tests。 |

## 数值算例

设球心为 `(1.0, -2.0, 0.5)`，半径为 `2.0`，`normal_distance_weight = 0.35`。若一个点到球心的距离为
`2.04`，法线与 `dir = point - center` 的锐角约为 `0.05` rad，则：

```text
weighted_euclid = 0.65 * abs(2.04 - 2.0) = 0.026
normal contribution = 0.35 * 0.05 = 0.0175
distance ~= 0.0435
```

阈值为 `0.22` 时，该点应被 count/select 视为内点；getDistances 应在对应输出位置写入约 `0.0435` 的
double 距离。RVV chunk 内会同时处理多个 index，但 select 的输出顺序仍按 `indices_` 中命中的先后顺序。

## Bench 与证据

当前 production 数据来自接入后的板卡 repeated 结果。bench 输入是 65536 点 direct indexed normal-sphere cloud，
覆盖 `PointXYZ + Normal`、`PointXYZI + Normal`、`PointXYZRGB + Normal` 和 `PointXYZRGBA + Normal`；
计时迭代 200 次，warmup 5 次。计时边界只包含公开入口调用，不包含点云、法线、indices 或系数构造。

| point type | public entry | Std median ms | RVV median ms | speedup median | 证据角色 |
| --- | --- | ---: | ---: | ---: | --- |
| `PointXYZ + Normal` | `selectWithinDistance` | 8.2819 | 2.7093 | `3.039x` | production public |
| `PointXYZ + Normal` | `countWithinDistance` | 7.5855 | 2.2804 | `3.332x` | production public |
| `PointXYZ + Normal` | `getDistancesToModel` | 11.0847 | 2.4861 | `4.456x` | production public |
| `PointXYZI + Normal` | `selectWithinDistance` | 8.2453 | 2.8430 | `2.900x` | production public |
| `PointXYZI + Normal` | `countWithinDistance` | 7.6386 | 2.2700 | `3.358x` | production public |
| `PointXYZI + Normal` | `getDistancesToModel` | 11.1768 | 2.6948 | `4.157x` | production public |
| `PointXYZRGB + Normal` | `selectWithinDistance` | 8.2390 | 2.7747 | `2.973x` | production public |
| `PointXYZRGB + Normal` | `countWithinDistance` | 7.5953 | 2.2915 | `3.308x` | production public |
| `PointXYZRGB + Normal` | `getDistancesToModel` | 11.1085 | 2.5904 | `4.290x` | production public |
| `PointXYZRGBA + Normal` | `selectWithinDistance` | 8.2208 | 2.8293 | `2.906x` | production public |
| `PointXYZRGBA + Normal` | `countWithinDistance` | 7.5989 | 2.2367 | `3.411x` | production public |
| `PointXYZRGBA + Normal` | `getDistancesToModel` | 11.0953 | 2.6681 | `4.161x` | production public |

QEMU（仿真器）只用于 correctness、构建和日志形状。性能结论只来自 board（板卡）或目标硬件。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- |
| `countWithinDistance` / `selectWithinDistance` / `getDistancesToModel` | production public entry | 做模型有效性检查并按 RVV gate 分流。 | production boundary | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp` |
| `*StandardNormalSphere` helpers | production Std helper | 保存原标量 fallback 语义。 | fallback coverage | 同上 |
| `*RVVNormalSphere` helpers | production RVV helper | 执行 indexed gather、距离核、mask / compress / dense store。 | production public / asm attribution | 同上 |
| `RVVNormalSphereLayoutGate` / `canUseRVVNormalSphere` | production dispatch / fallback | 限定 source layout、normal 点型、index 类型、规模和小输入。 | fallback coverage | 同上 |
| `src/test_sac_model_normal_sphere.cpp` | correctness tests | 对拍 public entry、production detail helper、fallback 和代表点型组合。 | QEMU correctness | `test-rvv/sample_consensus/sac_model_normal_sphere/src/test_sac_model_normal_sphere.cpp` |
| `src/bench_sac_model_normal_sphere.cpp` | bench wrapper | 计时 public 三入口和历史 diagnostic candidate。 | board performance input | `test-rvv/sample_consensus/sac_model_normal_sphere/src/bench_sac_model_normal_sphere.cpp` |
| `generate_normal_sphere_evidence_manifest.py` | analysis script | 把 board logs 转成 summary 和 Evidence Doctor manifest。 | evidence summary | `test-rvv/sample_consensus/sac_model_normal_sphere/script/generate_normal_sphere_evidence_manifest.py` |
| Phase060 manifest / Doctor | evidence output summary | 保存四组 source 点型 × 三入口的 5-run production public Std/RVV 对比和体检结果。 | EvidenceDecision input | `test-rvv/sample_consensus/sac_model_normal_sphere/doc/phases/060-production-integration-execution/` |
| evaluation | topic-local decision audit | 保存诊断到生产接入的取舍、doc suite 和未覆盖范围。 | decision trace | `test-rvv/sample_consensus/sac_model_normal_sphere/doc/sac_model_normal_sphere-evaluation.zh.md` |

## 正确性与高效性证据链

correctness（正确性）：`run_test_compare` 在 Std/RVV 两个构建中各运行 8 个 gtest，覆盖 public entries 与
reference 对拍、`PointXYZI` / RGB / RGBA layout、球心退化方向、production RVV detail helper、large input
public dispatch 和 fallback 语义。

path / asm（路径 / 反汇编）：`dump_bench_rvv` 生成 RVV bench 反汇编；抽查可见
`computeNormalSphereDistanceRVV`、`vfsqrt.v`、`vcpop.m`、`vcompress.vm`、`vfwcvt.f.f.v` 和 `vse64.v`。
反汇编用于证明 RVV 指令存在并可归属，不用于性能结论。

performance（性能）：5-run board repeated summary 是当前唯一性能结论来源。四组 source 点型 × 三条 public entry
的 median speedup 全部大于 1.0，Evidence Doctor 为 0/0/0。

boundary（边界）：EvidenceDecision 只覆盖 direct indexed `indices_`、四组 source 点型、`pcl::Normal` normal cloud、
source xyz AoS 生产分流、signed 32-bit index、u32 byte offset gate、`indices_.size() >= 16` 和当前板卡。

risk（风险）：其它 normal 点型、自定义点型、其它 layout、非法 index、真实 workload、identity-index 专门路径、
`Scalar=double` 和其它硬件未由本次生产证据关闭。

## Production Closeout

| 项 | 状态 |
| --- | --- |
| production patch | adopted；修改 `impl/sac_model_normal_sphere.hpp`，新增 Standard / RVV helper 并接入三条 public entry。 |
| public API | unchanged。 |
| test assets | retained；`test-rvv/sample_consensus/sac_model_normal_sphere/` 保存 correctness、bench、manifest、Doctor、registry 和 topic-local 文档。 |
| evidence policy | summary-only；raw board logs、QEMU logs 和 build output 默认不提交。 |
| rollback boundary | 若后续 reviewer 要求撤回，应同时撤回生产 helper / dispatch，并把本文档改回历史归档或删除。 |

## 后续方向

当前 topic 内没有建议默认继续推进的未阻塞优化方向。Phase060 已经证明三条公开入口在四种内置 source 点型和
`pcl::Normal` normal cloud 上都有稳定 production-public 收益，收益幅度足以覆盖 gather、normal angle、`vcompress`
和 dense double store 的维护成本。

继续扩大到其它 normal 点型、自定义 registered point type、真实 workload/profile、identity-index 专门路径、
`Scalar=double` 或其它硬件，都会改变当前 scope（范围）。这些方向需要新 phase 或 follow-up topic，并先补
traits / layout、fallback、correctness、asm、board repeated 和 Evidence Doctor 证据；在没有真实调用价值、
profile 或用户点名范围前，不建议继续在当前 topic 默认堆优化。
