# SampleConsensusModelCylinder RVV 生产实现

## 当前生产状态

`SampleConsensusModelCylinder<PointT, PointNT>` 当前已采纳三条 production RVV（生产 RVV）路径：

- `countWithinDistance`
- `selectWithinDistance`
- `getDistancesToModel`

当前采纳范围是 direct indexed `indices_`、`PointXYZ + Normal`、`PointXYZI + Normal`、
`PointXYZRGB + Normal` 和 `PointXYZ + PointNormal` 四组代表点型、traits-gated xyz + normal AoS
（结构数组）float 字段布局、signed 32-bit `pcl::index_t`、可用 32-bit byte offset（字节偏移）表达的点云规模、
`Eigen::VectorXf` 模型系数和 RVV 构建。自定义点型全集、其它 layout（布局）、`Scalar=double`、
其它 row source policy（行来源策略）或完整 RANSAC workload（工作负载）需要单独证据，不从本次板卡结果外推。

## 稳定证据索引

| 证据 | 路径 / 命令 | 作用 |
| --- | --- | --- |
| Phase 020 result | `test-rvv/sample_consensus/sac_model_cylinder/doc/phases/020-cylinder-production-integration/result.zh.md` | 记录 count/select production integration 和当前三入口 production repeated summary。 |
| Phase 030 result | `test-rvv/sample_consensus/sac_model_cylinder/doc/phases/030-cylinder-getdistances-dense-output/result.zh.md` | 记录 getDistances dense output 接入、checksum 异常处理和采纳结论。 |
| production manifest | `test-rvv/sample_consensus/sac_model_cylinder/doc/phases/020-cylinder-production-integration/production-repeated-evidence-manifest.json` | 当前四组代表点型 × 三入口的 5-run board repeated（重复板卡测试）摘要。 |
| Evidence Doctor | `test-rvv/sample_consensus/sac_model_cylinder/doc/phases/020-cylinder-production-integration/production-repeated-evidence-doctor.md` | 证据体检结果，当前 Errors=0、Warnings=0、Suggestions=0。 |
| registry | `test-rvv/sample_consensus/sac_model_cylinder/log/evidence_registry.json` | 登记 production manifest / Doctor，并支持 freshness（新鲜度）检查。 |
| correctness | `make -C test-rvv/sample_consensus/sac_model_cylinder run_test_compare` | Std/RVV 两个构建各 11 个 gtest 通过。 |
| asm | `make -C test-rvv/sample_consensus/sac_model_cylinder clean_bench_rvv check_production_asm` | 三个 production RVV helper 均命中预期 RVV 指令。 |

## 函数语义

cylinder 模型系数包含轴线上一点 `(px, py, pz)`、轴线方向 `(dx, dy, dz)` 和半径 `radius`。三个公开入口都遍历
`indices_` 指向的输入点，并结合 `normals_` 中同 index 的法线：

```text
diff = point - line_point
dir = diff - dot(diff, normalized_line_dir) * normalized_line_dir
weighted_euclid = (1 - normal_distance_weight) * abs(norm(dir) - radius)
normal_angle = acute angle(normal[index], dir)
distance = abs(normal_distance_weight * normal_angle + weighted_euclid)
```

| public entry | 输出语义 | 当前 production 状态 |
| --- | --- | --- |
| `countWithinDistance` | `weighted_euclid` 未超阈值且 `distance < threshold` 的点计数。 | RVV adopted |
| `selectWithinDistance` | 按 `indices_` 顺序写内点 index，并把 `distance` 写入 `error_sqr_dists_`。 | RVV adopted |
| `getDistancesToModel` | 为每个 index 输出一个 dense double distance（连续 double 距离）。 | RVV adopted |

`countWithinDistanceStandardCylinder`、`selectWithinDistanceStandardCylinder` 和
`getDistancesToModelStandardCylinder` 保存原标量主体，用于 fallback（回退路径）和 public-vs-Standard correctness。

## 当前采用的优化方式

公开入口先执行原有 `isModelValid` 检查。RVV 构建中，若 point xyz 和 normal xyz 字段满足 traits-gated AoS
layout、`pcl::index_t` 是 signed 32-bit，且 input / normal 点云规模可以用 32-bit byte offset 表达，入口短路进入
对应 RVV helper。任一条件不满足时调用对应 Standard helper。

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| `countWithinDistanceRVVCylinder` | adopted | RVV indexed gather（索引离散加载）批量读取 point 和 normal，计算圆柱距离核，用 mask 和 `vcpop.m` 统计内点。 | 四组代表点型 board median 为 `5.3124x`、`5.5145x`、`5.3986x`、`5.4105x`，Doctor 0/0/0。 | 只证明当前 public direct indexed 代表点型 bench。 |
| `selectWithinDistanceRVVCylinder` | adopted | `vcompress.vm` 保持 chunk 内 lane（向量通道）顺序，压缩写回 index；距离用 `vfwcvt.f.f.v` 和 `vse64.v` 写 double。 | 四组代表点型 board median 为 `4.6445x`、`4.4819x`、`4.4373x`、`4.3314x`，Doctor 0/0/0。 | 输出顺序由 gtest 和 checksum 保护。 |
| `getDistancesToModelRVVCylinder` | adopted | 没有阈值早停，所有 lane 都计算 normal angle；`vfsqrt.v` 留在 RVV chunk 内，结果拓宽并连续写 double vector。 | 四组代表点型 board median 为 `6.9318x`、`6.7851x`、`7.3186x`、`6.2453x`，Doctor 0/0/0。 | checksum 只保护规模，逐项数值由 gtest 保护。 |
| Phase 000 diagnostic helper | historical | 它证明 count/select 实现族值得进入 production probe，但不能替代接入后的 evidence。 | diagnostic count `7.22x`、select `6.32x`。 | 不作为当前生产性能 truth。 |
| identity-index strided load | deferred | 已完成 sibling 主题中多次显示 identity strided load 不是默认升级；当前 shuffled gather 已强正向。 | 当前未运行 cylinder A/B。 | 只有 profile 显示 identity indices 是主 workload 时重开。 |
| 代表点型 / layout 扩展 | adopted | production gate 是 traits-based，Phase 040 已补 `PointXYZI + Normal`、`PointXYZRGB + Normal` 和 `PointXYZ + PointNormal` 的 correctness、asm 和 board repeated。 | typed medians 全部 positive，Doctor 0/0/0。 | 自定义点型全集或未测复合点型需另开窄 phase。 |

## VL Chunk 流程

三个 RVV helper 共用 `computeCylinderDistanceTermsRVV`：

1. `vsetvl` 选择当前 VL chunk（可变向量长度分块）。
2. 从 `indices_` 加载 32-bit index，并转成 point / normal 的 byte offset。
3. 用 RVV load wrapper（向量加载封装）离散读取 point xyz 和 normal xyz。
4. 计算点到圆柱轴线的径向向量 `dir`、`dir` 的长度和半径差。
5. 计算 normal 与 `dir` 的锐角夹角，再组合 `normal_distance_weight_`。

后半段按入口分流：count 用 `vcpop.m` 规约 mask；select 用 `vcompress.vm` 压缩命中的 index 和 distance；
getDistances 直接把每个 lane 的 distance 拓宽成 double 并连续写入 `distances`。

## 覆盖范围与 Fallback

| 条件 | 当前行为 | 证据 |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | 只编译并调用 Standard helper。 | Std 构建 `run_test_compare` 11/11。 |
| point xyz 或 normal xyz layout 不满足 | `if constexpr` 不实例化 RVV helper，回到 Standard helper。 | 源码 gate。 |
| `pcl::index_t` 不是 signed 32-bit | 不进入 RVV helper，避免 index load / compressed store 类型不匹配。 | 源码 gate。 |
| input / normal 规模超过 u32 byte offset 上限 | RVV helper 返回 `false`，public entry 调用 Standard helper。 | 源码 gate。 |
| normal cloud 小于 input cloud | 三入口均回到 Standard helper；count/select 保留标量懒读取语义。 | `PublicEntryFallsBackWhenNormalsDoNotCoverInput` 覆盖 count/select。 |
| model coefficients 无效 | 保持原公开入口副作用：count 返回 0，select / getDistances 清空或不输出。 | public entry dispatch 前调用 `isModelValid`。 |
| `PointXYZI + Normal`、`PointXYZRGB + Normal`、`PointXYZ + PointNormal` | 命中 traits gate 并进入同一生产 RVV helper。 | Phase 040 correctness、asm、5-run board repeated 和 Doctor。 |
| `PointXYZRGBA`、`PointXYZINormal`、custom normal-like 点型 | 可能满足 traits gate，但没有 dedicated board performance。 | 当前不外推。 |

## 范围决策表

| 范围 | 状态 | 证据 | 下一步 |
| --- | --- | --- | --- |
| `PointXYZ + Normal` direct indexed public count/select/getDistances | adopted | 11/11 gtest、production asm、5-run board repeated、Doctor 0/0/0。 | 当前 production 行为保留。 |
| `PointXYZI + Normal`、`PointXYZRGB + Normal`、`PointXYZ + PointNormal` | adopted | Phase 040 dedicated correctness、typed board repeated、Doctor 0/0/0。 | 当前 production 行为保留。 |
| identity `indices_` 专门路径 | deferred | 当前通用 shuffled gather 已强正向；没有 profile 证明需要特殊路径。 | profile 或用户点名后做 RVV-vs-RVV A/B。 |
| `Scalar=double` | not_applicable now | 当前公开入口使用 `Eigen::VectorXf` 系数。 | 若上游签名变化再评估。 |
| `optimizeModelCoefficients` / `projectPoints` / `doSamplesVerifyModel` | scalar-only | 不属于当前距离三入口 RVV scope。 | 需要独立 profile 和 phase。 |

## 标量路径与 RVV 路径差异

| 阶段 | Standard helper | RVV helper | 语义保持证据 |
| --- | --- | --- | --- |
| 模型有效性 | public entry 调用 `isModelValid`。 | public entry 先调用同一检查。 | 公开入口 tests 使用同一系数输入。 |
| 点和法线读取 | `input[index]` 和 `normals[index]`。 | index -> byte offset 后离散读取 xyz / normal xyz。 | shuffled indices gtest 和 bench checksum。 |
| 圆柱距离核 | Eigen 向量、标量 `norm` 和 `getAngle3D`。 | RVV `vfsqrt.v`、FMA 和 normal angle helper。 | public-vs-Standard tests，误差 `1e-5`。 |
| count 输出 | 标量累加计数。 | `vcpop.m` 统计 inlier mask。 | `CountPublicEntryMatchesProductionStandardHelper`。 |
| select 输出 | `push_back` index 和 double distance。 | 预分配后 `vcompress.vm` 保序写回。 | `SelectPublicEntryMatchesProductionStandardHelper`。 |
| getDistances 输出 | 每个 index 写一个 double distance。 | `vfwcvt.f.f.v + vse64.v` 连续写回。 | `GetDistancesBenchShapedPublicEntryMatchesStandardHelper`。 |

## 数值算例

设模型轴线点为 `(0.20, -0.30, 0.10)`，方向为 `(0, 0, 1)`，半径为 `1.0`，`normal_distance_weight=0.25`。
若某点位于半径 `1.03` 的圆柱面附近，法线相对径向方向偏转约 `0.03` rad，则：

```text
weighted_euclid = 0.75 * abs(1.03 - 1.0) = 0.0225
normal contribution = 0.25 * 0.03 = 0.0075
distance ~= 0.0300
```

阈值为 `0.10` 时，该点应被 count/select 视为内点；getDistances 应在对应输出位置写入约 `0.03` 的 double
距离。RVV chunk 内会同时处理多个此类 index，但输出顺序仍按 `indices_`。

## Bench 与证据

当前 production 数据来自接入后的板卡 repeated 结果。bench 输入是 65536 点 shuffled adjacent pairs，覆盖
`PointXYZ + Normal`、`PointXYZI + Normal`、`PointXYZRGB + Normal` 和 `PointXYZ + PointNormal`
四组代表点型；计时迭代 200 次，warmup 5 次。计时边界只包含公开入口调用，不包含点云、法线、indices
或系数构造。

| point type | public entry | Std 平均 ms | RVV 平均 ms | speedup min / median / max | 证据角色 |
| --- | --- | ---: | ---: | ---: | --- |
| `PointXYZ + Normal` | `countWithinDistance` | `11.656673` | `2.167619` | `5.1606x / 5.3124x / 5.6082x` | production direct |
| `PointXYZ + Normal` | `selectWithinDistance` | `12.588158` | `2.726701` | `4.5348x / 4.6445x / 4.6579x` | production direct |
| `PointXYZ + Normal` | `getDistancesToModel` | `15.146606` | `2.182286` | `6.8120x / 6.9318x / 7.0688x` | production direct |
| `PointXYZI + Normal` | `countWithinDistance` | `11.715698` | `2.138036` | `5.3724x / 5.5145x / 5.5245x` | production direct |
| `PointXYZI + Normal` | `selectWithinDistance` | `12.648999` | `2.826342` | `4.3949x / 4.4819x / 4.5514x` | production direct |
| `PointXYZI + Normal` | `getDistancesToModel` | `15.144458` | `2.244163` | `6.5743x / 6.7851x / 6.8318x` | production direct |
| `PointXYZRGB + Normal` | `countWithinDistance` | `11.698597` | `2.176810` | `5.2755x / 5.3986x / 5.4519x` | production direct |
| `PointXYZRGB + Normal` | `selectWithinDistance` | `12.637544` | `2.851038` | `4.3543x / 4.4373x / 4.5229x` | production direct |
| `PointXYZRGB + Normal` | `getDistancesToModel` | `15.145902` | `2.081939` | `7.1422x / 7.3186x / 7.3352x` | production direct |
| `PointXYZ + PointNormal` | `countWithinDistance` | `11.715236` | `2.167212` | `5.3094x / 5.4105x / 5.4691x` | production direct |
| `PointXYZ + PointNormal` | `selectWithinDistance` | `12.645315` | `2.977867` | `4.0728x / 4.3314x / 4.3635x` | production direct |
| `PointXYZ + PointNormal` | `getDistancesToModel` | `15.195919` | `2.443617` | `6.0669x / 6.2453x / 6.2818x` | production direct |

QEMU（仿真器）只用于 correctness、构建和日志形状。性能结论只来自 board（板卡）或目标硬件。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- |
| `countWithinDistance` / `selectWithinDistance` / `getDistancesToModel` | production public entry | 做模型有效性检查并按 RVV gate 分流。 | production boundary | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_cylinder.hpp` |
| `*StandardCylinder` helpers | production Std helper | 保存原标量 fallback 语义。 | fallback coverage | 同上 |
| `*RVVCylinder` helpers | production RVV helper | 执行 indexed gather、距离核、mask / compress / dense store。 | production direct / asm attribution | 同上 |
| `src/test_sac_model_cylinder.cpp` | correctness tests | 对拍 public entry、Standard helper、diagnostic candidate 和代表点型组合。 | QEMU correctness | `test-rvv/sample_consensus/sac_model_cylinder/src/test_sac_model_cylinder.cpp` |
| `src/bench_sac_model_cylinder.cpp` | bench wrapper | 计时 public 三入口、typed public entries 和历史 diagnostic candidate。 | board performance input | `test-rvv/sample_consensus/sac_model_cylinder/src/bench_sac_model_cylinder.cpp` |
| `generate_cylinder_board_evidence_manifest.py` | analysis script | 把 repeated board logs 转成 Evidence Doctor manifest。 | evidence summary | `test-rvv/sample_consensus/sac_model_cylinder/script/generate_cylinder_board_evidence_manifest.py` |
| production manifest / Doctor | evidence output summary | 保存四组代表点型 × 三入口的 5-run production public Std/RVV 对比和体检结果。 | EvidenceDecision input | `test-rvv/sample_consensus/sac_model_cylinder/doc/phases/020-cylinder-production-integration/` |
| evaluation | topic-local decision audit | 保存诊断到生产接入的取舍、doc suite 和未覆盖范围。 | decision trace | `test-rvv/sample_consensus/sac_model_cylinder/doc/sac_model_cylinder-evaluation.zh.md` |

## 正确性与高效性证据链

correctness（正确性）：`run_test_compare` 在 Std/RVV 两个构建中各运行 11 个 gtest，覆盖 count/select/getDistances
public-vs-Standard，对 Phase 000 diagnostic candidate 的历史回归，三组代表点型 public-vs-Standard 对拍，
以及 count/select normal 数不足时必须回退的边界。

path / asm（路径 / 反汇编）：`check_production_asm` 检查 RVV bench 反汇编，要求三个 production helper 出现预期 RVV 指令。
`getDistancesToModelRVVCylinder` 还要求 `vfsqrt.v`、`vfwcvt.f.f.v` 和 `vse64.v`，避免 dense output 回退到标量 lane loop。

performance（性能）：5-run board repeated summary 是当前唯一性能结论来源。四组代表点型 × 三条 public entry
的 median 和 min speedup 都大于 1.0，Evidence Doctor 为 0/0/0。

boundary（边界）：EvidenceDecision 只覆盖 direct indexed `indices_`、四组代表点型、traits-gated
xyz / normal AoS 生产分流、signed 32-bit index、u32 byte offset gate 和当前板卡。

risk（风险）：自定义点型全集、未测复合点型、其它 layout、真实 workload、identity-index 专门路径和其它硬件未由本次生产证据关闭。

## Production closeout

| 项 | 状态 |
| --- | --- |
| production patch | adopted；修改 `impl/sac_model_cylinder.hpp`，新增 Standard / RVV helper 并接入三条 public entry。 |
| public API | unchanged。 |
| test assets | retained；`test-rvv/sample_consensus/sac_model_cylinder/` 保存 correctness、bench、manifest、Doctor、registry 和 topic-local 文档。 |
| evidence policy | summary-only；raw board logs、QEMU logs 和 build output 默认不提交。 |
| rollback boundary | 若后续 reviewer 要求撤回，应同时撤回生产 helper / dispatch，并把本文档改回历史归档或删除。 |

## 后续方向

当前 topic 内没有建议默认继续推进的未阻塞优化方向。Phase 040 已经闭合代表点型扩展；继续扩大到
`PointXYZRGBA`、`PointXYZINormal` 或自定义点型应先有真实调用价值、profile 或用户点名范围。

当前不建议默认推进 identity-index strided load 或 `optimizeModelCoefficients` staging。前者需要真实 workload/profile
显示 identity indices 主导；后者需要 profile 证明 Eigen array staging 接近主成本。
