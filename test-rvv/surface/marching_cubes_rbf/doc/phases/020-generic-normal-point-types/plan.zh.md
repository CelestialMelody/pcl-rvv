# 020 generic normal point types 扩展计划

## 阶段意图和边界

本阶段把已采纳的 exact `pcl::PointNormal` production RVV gate（生产 RVV 门控）扩展为
`pcl::rvv::RVVXYZNormalFloatLayout<PointNT>` traits gate（点类型字段特征门控）。目标是让满足
registered xyz + normal single-float AoS layout（已注册 xyz + normal 单 float 结构数组布局）的点类型
命中同一 RVV matrix fill 和 voxel evaluation 路径。

本阶段验证范围：

- production entry（生产入口）：`MarchingCubesRBF<PointNT>::voxelizeData()`。
- 点类型：`pcl::PointXYZINormal`、`pcl::PointXYZRGBNormal`，同时保留 `pcl::PointNormal` 回归覆盖。
- runtime gate：`__RVV10__`、traits layout 成立、`input_->size() >= 16`。
- fallback：小规模输入、非 RVV 构建和不满足 traits 的点型继续走标量。

本阶段不证明：

- 所有自定义 normal-like 点类型都已充分覆盖；traits gate 只证明布局前提，仍需要用户侧实例自己编译通过。
- Eigen solve、surface emission 或 per-iteration repeated summary 已优化。
- `input_->size() < 16` 值得走 RVV。

## 当前状态清单

| 项 | 当前状态 | 路径 / 证据 |
| --- | --- | --- |
| exact `PointNormal` production patch | adopted | `surface/include/pcl/surface/impl/marching_cubes_rbf.hpp` |
| 正式长期文档 | 已创建 | `doc-rvv/surface/marching_cubes_rbf-RVV.zh.md` |
| traits API | 已有 `RVVXYZNormalFloatLayout<PointT>` | `common/include/pcl/rvv_point_traits.h` |
| fallback 测试 | `PointXYZINormal` 当前作为 fallback 测试 | `test-rvv/surface/marching_cubes_rbf/src/test_marching_cubes_rbf.cpp` |
| bench case | 只有 `mcrbf_prod_pointnormal_*` | `test-rvv/surface/marching_cubes_rbf/src/bench_marching_cubes_rbf.cpp` |
| Evidence Doctor | 当前 production direct 为 weak-positive | `test-rvv/surface/marching_cubes_rbf/log/board/evidence_doctor.md` |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | 本阶段直接使用 `production_direct`。 |
| A/B boundary | public overload / production dispatch，即真实 `MarchingCubesRBF<PointNT>::voxelizeData()`。 |
| 当前决策问题 | `RVV-vs-scalar` 和 fallback correctness：traits-gated 点型是否可走 RVV 且比标量更快。 |
| diagnostic 是否可外推到 production | 不外推；`PointXYZINormal` / `PointXYZRGBNormal` 必须各自跑 production direct correctness 和 board bench。 |
| comparison-boundary / baseline mismatch 风险 | Std/RVV 使用同一 bench wrapper、同一输入构造和同一 checksum policy；点型字段额外语义不参与 RBF 公式。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已有 exact `PointNormal` production 行为可保留；若新增点型 production direct 非正向，本阶段只保留 traits gate 中实际正向点型，或回退到 exact gate。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不涉及新实现族选择；只是扩大同一已采纳 family 的 point type gate，但必须有每个新增点型的 Std/RVV production direct 证据。 |

## 优化矩阵

| candidate family | row source policy | point type / layout | scope and entry | correctness / fallback target | bench / board target | asm boundary | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| traits-gated normal AoS RVV path | single input cloud, off-surface pair expansion | `PointXYZINormal`, `PointXYZRGBNormal`, `RVVXYZNormalFloatLayout<PointNT>` | real `MarchingCubesRBF<PointNT>::voxelizeData()` | production direct gtest for each point type; small fallback still scalar | `mcrbf_prod_pointxyzinormal_*`, `mcrbf_prod_pointxyzrgbnormal_*` | production helper symbols + RVV double instructions | JSON manifest + Evidence Doctor | planned |
| exact `PointNormal` regression | same | exact `PointNormal` | same | existing production direct gtest | existing `mcrbf_prod_pointnormal_*` | existing | existing | adopted / regression |
| non-normal fallback | not_applicable | point type without normal fields | same | compile / fallback smoke if practical | not_applicable | not_applicable | not_applicable | optional |

## 实现和测试动作

1. production 头增加 `pcl/rvv_point_traits.h`，把 `marchingCubesRBFVoxelizeDataRVV()` 的 exact
   `PointNormal` compile-time gate 改为 `pcl::rvv::RVVXYZNormalFloatLayout<PointNT>::value`。
2. 保留 `input.size() < 16` runtime fallback。
3. 测试支撑的 `makeNormalPointCloud<PointT>()` 补 `PointXYZRGBNormal` 的 RGB 字段初始化；`PointXYZINormal`
   从 fallback case 升级为 production direct case。
4. gtest 增加 `PointXYZINormalVoxelizeMatchesScalarReference` 和
   `PointXYZRGBNormalVoxelizeMatchesScalarReference`；保留 small `PointNormal` fallback。
5. bench 增加 `mcrbf_prod_pointxyzinormal_*` 与 `mcrbf_prod_pointxyzrgbnormal_*` case。
6. manifest 脚本把新增 `mcrbf_prod_*` case 都识别为 production direct，并记录 point type。
7. 跑 QEMU correctness、QEMU bench smoke、asm dump、板卡 production smoke、Evidence Doctor。
8. 回填 phase result、optimization matrix、roadmap、evaluation 和 `doc-rvv`。

## fallback 矩阵

| 条件 | 行为 |
| --- | --- |
| 非 RVV 构建 | 只编译并执行标量 helper。 |
| traits gate 不成立 | RVV helper 编译期返回 false，公开入口走标量 helper。 |
| `input_->size() < 16` | 走标量 helper。 |
| Eigen solver、`createSurface()`、mesh 输出 | 保持原标量 / 既有路径。 |
| 新增点型 production direct 退化 | 不把该点型写成 adopted；必要时收窄 gate 或增加排除条件。 |

## 板卡预算和决策桶

- 初始 budget：新增 production bench 每个 case `5` 次 iteration、`2` 次 warm-up，执行一次全 case smoke。
- `positive`：新增点型 production direct case 均大于 `1.05x`，无 Evidence Doctor Error，correctness 与 asm 通过。
- `weak_positive`：新增点型均大于 `1.00x` 且实现无额外风险，可由用户 / reviewer 判断是否保留；若低于 `1.05x`，文档必须明确 weak-positive。
- `neutral/negative`：新增点型不优于标量，收窄或撤回该点型 gate。
- `unstable`：预算耗尽后方向跨桶摇摆，降级为需要人工判断。

## 文档更新清单

- 新建 `020-generic-normal-point-types/result.zh.md`。
- 更新 `doc/phases/README.zh.md`、`optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md`。
- 更新 `doc/marching_cubes_rbf-evaluation.zh.md`。
- 更新 `doc-rvv/surface/marching_cubes_rbf-RVV.zh.md` 的覆盖范围、fallback 矩阵、bench 表和后续方向。

## 继续 / 停止条件

继续条件：

- production gate 能用公共 traits 表达，不改变 public API。
- QEMU correctness 和新增点型 production direct 测试通过。
- 板卡可用并能执行 production smoke。

停止条件：

- traits gate 触发编译错误或误覆盖不满足 normal 字段语义的点型。
- 新增点型 correctness、asm 或板卡 evidence 不能闭合。
- 需要扩展到自定义点型、非 AoS layout 或 per-iteration analyzer 改造，超出本阶段范围。

`next_phase_default`: 执行本计划并按证据决定是否将 traits-gated normal AoS RVV path 写成 adopted。
