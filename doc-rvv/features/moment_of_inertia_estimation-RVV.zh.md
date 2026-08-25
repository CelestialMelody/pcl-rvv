# MomentOfInertiaEstimation RVV production closeout

## 当前状态

`pcl::MomentOfInertiaEstimation<PointT>::compute()` 在 `__RVV10__` 构建下已经采用 projected covariance fusion（投影协方差融合）RVV（RISC-V Vector，可变长度向量）helper。该 helper 只接管 angle scan（角度扫描）里“把点投影到当前平面后再计算 covariance（协方差）”这一步；`computeMeanValue()`、主 covariance、Eigen 特征求解、`calculateMomentOfInertia()` 和 `computeOBB()` 仍保持原标量实现。

| item | final state | evidence |
| --- | --- | --- |
| production files | adopted | `features/include/pcl/features/moment_of_inertia_estimation.h`、`features/include/pcl/features/impl/moment_of_inertia_estimation.hpp` |
| public API | unchanged | helper 为 private 声明，未新增公开接口 |
| compile gate | `__RVV10__` only | 非 RVV 构建不声明、不编译、不调用 helper |
| fallback | adopted | helper 返回 `false` 时回到原 `getProjectedCloud()` + projected `computeCovarianceMatrix()` 标量路径 |
| adopted scope | `PointXYZ` 和常见 PointXYZ-like typed scope | phase040 / phase050 production-public board evidence |

## 函数语义与标量路径

`compute()` 先通过 `PCLBase` 的 `input_` 和 `indices_` 读取 indexed cloud（索引点云），然后依次计算 mass center（质心）、AABB（axis aligned bounding box，轴对齐包围盒）、主 covariance、特征值和特征向量。随后它遍历 `theta` 和 `phi` 生成当前轴：

1. `calculateMomentOfInertia(current_axis, mean_value_)` 对原始 indexed cloud 做单轴惯性矩规约。
2. 原标量路径调用 `getProjectedCloud(current_axis, mean_value_, projected_cloud)`，为当前平面写出一份临时投影点云。
3. 原标量路径再对这份临时点云调用 `computeCovarianceMatrix(projected_cloud, covariance_matrix)`。
4. `computeEccentricity(covariance_matrix, current_axis)` 使用该 covariance 生成离心率。

RVV 优化只替换第 2-3 步，直接在一次 indexed gather（按索引离散加载）中计算投影后的 6 个 covariance 项，避免每个角度都 materialize projected cloud（实体化投影点云）。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| dispatch（分流逻辑） | adopted | `compute()` 中先尝试 `computeProjectedCovarianceRVV()`，失败时进入原标量块 | `make run_test_compare`，fallback isolation tests | 不改变 public API |
| layout gate（布局门控） | adopted | `RVVXYZAoSFloatLayout<PointT>` 确认 xyz 是单个 `float` 字段，并用 `sizeof(PointT)` 生成 byte offset | phase050 typed public tests and board | custom point type 需要新 phase |
| indexed gather | adopted | production 输入来自 `indices_`，RVV 用 signed i32 index load 转成 u32 byte offsets 后执行三路 xyz gather | `make dump_bench_rvv` 命中 `vluxei32` | 输入规模受 32-bit byte offset gate 约束 |
| vector reduction（向量规约） | adopted | 每个 VL chunk 累加 projected `xx/xy/xz/yy/yz/zz`，最后乘以与标量一致的 factor | correctness tests，asm 中 `vfredusum` | 浮点规约顺序不同，容差由测试覆盖 |
| fused formula（融合公式） | adopted | 不写 projected cloud，直接计算 `p = d - dot(d,n) * n` 后规约 | phase010 diagnostic 和 phase040 production-public evidence | 不改变 `computeEccentricity()` 输入定义 |
| mean/AABB RVV | rejected | phase030 public compute 收益不稳定且 Evidence Doctor 有 Error | phase030 result | 已回滚，不在当前 production behavior 中 |
| full compute replacement | not_now | Eigen solver、角度扫描状态和输出顺序混在一起，不能证明单一收益来源 | roadmap | 需要新 scope 和更细消融 |

VL chunk（可变向量长度分块）内部流程：

```cpp
if constexpr (!RVVXYZAoSFloatLayout<PointT>::value)
  return false;
if (!input_ || !indices_ || indices_->empty())
  return false;

for each VL chunk in indices_:
  load i32 indices
  convert index * sizeof(PointT) to u32 byte offsets
  gather x/y/z from input_->points
  dx, dy, dz = point - mean_value_
  dot = dx * nx + dy * ny + dz * nz
  projected = d - dot * normal
  reduce xx, xy, xz, yy, yz, zz with vfredusum
write symmetric 3x3 covariance matrix and return true
```

这个流程覆盖的是原标量路径中“投影点生成 + 投影点 covariance”这一个完整阶段。后续的 eccentricity 计算仍由原标量函数执行，因此输出顺序、容器状态和 getter 行为保持原语义。

## 范围决策表

| scope | status | evidence | boundary / next action |
| --- | --- | --- | --- |
| public `MomentOfInertiaEstimation<PointT>::compute()` | adopted | phase040 / phase050 public compute tests and board | 只覆盖 `__RVV10__` 构建下 helper 命中分支 |
| row source: `indices_` indexed cloud | adopted | tests 使用 shuffled / non-contiguous indices；bench 使用 indexed cloud | 不覆盖其它外部 row source |
| `PointXYZ` / `float` / AoS | adopted | phase040 median `1.984x`，0/5 退化，Doctor 0 Error / 0 Warning | 当前主生产证据 |
| `PointXYZI` | adopted | phase050 median `2.132x`，Doctor 0 Error / 0 Warning | common typed scope |
| `PointXYZRGB` | adopted | phase050 median `2.395x`，Doctor 0 Error / 0 Warning | common typed scope |
| `PointXYZRGBA` | adopted | phase050 median `2.487x`，Doctor 0 Error / 0 Warning | common typed scope |
| `PointXYZRGBNormal` | adopted | phase050 median `2.784x`，Doctor 0 Error / 0 Warning | common typed scope |
| non-`float` xyz layout | scalar fallback | `MomentOfInertiaProductionRVVFallback.NonFloatXYZUsesScalarFallback` | 不进入 RVV helper |
| empty input or empty indices | scalar fallback | `MomentOfInertiaProductionRVVFallback.MissingInputOrIndicesReturnFalse` | 保持原路径处理 |
| oversized cloud for u32 byte offsets | scalar fallback by source gate | `input_->size() > rvvMaxU32ByteOffsetElements<PointT>()` returns `false` | 不硬造超大动态测试 |
| `Scalar=double` | not_applicable | production implementation uses `float` matrices and `Eigen::Vector3f` | 需要新 scope |
| custom point type / exotic layout | deferred | 当前证据不覆盖 | 另开 point type / layout expansion phase |

## 标量路径与 RVV 路径差异

| stage | scalar path | RVV path | evidence |
| --- | --- | --- | --- |
| mean / AABB | `computeMeanValue()` | unchanged scalar | phase030 rejected mean/AABB-only probe |
| main covariance | indexed loop in `computeCovarianceMatrix(Eigen::Matrix&)` | unchanged scalar | correctness compare |
| eigen vectors | Eigen `SelfAdjointEigenSolver` | unchanged scalar | not in RVV hot helper |
| moment of inertia | `calculateMomentOfInertia()` | unchanged scalar | correctness compare |
| projected covariance in angle scan | allocate projected cloud, then compute covariance | direct gather + fused projection + six vector reductions | phase040 / phase050 evidence |
| eccentricity | `computeEccentricity()` | unchanged scalar consumer of covariance matrix | output compare |
| OBB | `computeOBB()` | unchanged scalar | output compare |

## Fallback 矩阵

| condition | behavior | evidence |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | 只编译标量 public compute 路径 | `MomentOfInertiaProductionFallback.StdBuildUsesScalarPublicCompute` |
| `PointT` 不满足 `RVVXYZAoSFloatLayout<PointT>` | helper 返回 `false`，`compute()` 进入原 projected cloud 标量路径 | `MomentOfInertiaProductionRVVFallback.NonFloatXYZUsesScalarFallback` |
| `input_` 为空或 `indices_` 为空 | helper 返回 `false` | `MomentOfInertiaProductionRVVFallback.MissingInputOrIndicesReturnFalse` |
| `input_->size()` 超过 u32 byte offset 上界 | helper 返回 `false` | 源码 gate：`rvvMaxU32ByteOffsetElements<PointT>()` |
| indices 非连续或重排 | RVV gather 仍按 `indices_` 读取并与标量对拍 | reduction / projected covariance tests 使用非连续 indices |
| helper 未命中 | public `compute()` 使用原 `getProjectedCloud()` + projected `computeCovarianceMatrix()` | `compute()` 中 fallback block 保留原实现 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `MomentOfInertiaEstimation<PointT>::compute()` | production public entry | 真实公开入口，串联标量阶段和 RVV projected covariance dispatch | 用户代码 | private helpers | production boundary | `features/include/pcl/features/impl/moment_of_inertia_estimation.hpp` |
| `computeProjectedCovarianceRVV()` | production RVV helper | 在 angle scan 内直接计算投影 covariance | `compute()` | `computeEccentricity()` | adopted RVV implementation and fallback gate | `features/include/pcl/features/impl/moment_of_inertia_estimation.hpp` |
| `computeCovarianceMatrix(PointCloudConstPtr, ...)` | production Std helper | projected cloud 标量 covariance fallback | `compute()` fallback block | `computeEccentricity()` | scalar fallback reconstruction | `features/include/pcl/features/impl/moment_of_inertia_estimation.hpp` |
| `RVVXYZAoSFloatLayout<PointT>` | production layout gate | 判断 xyz 单 float AoS 布局是否安全 | RVV helper | `rvv_load::indexed_load3_f32m2` | layout boundary | `common/include/pcl/rvv_point_traits.h` |
| `rvv_load::indexed_load3_f32m2` | RVV load helper | 根据 byte offsets 离散加载 x/y/z | RVV helper | projection formula | asm / data layout evidence | `common/include/pcl/rvv_point_load.h` |
| `test_moi` | correctness gate | Std/RVV 对拍 public output、helper output 和 fallback isolation | Makefile `run_test_compare` | QEMU logs | correctness and fallback coverage | `test-rvv/features/moment_of_inertia_estimation/src/test_moi.cpp` |
| `bench_moi` | bench wrapper | public compute 和 helper-only case-filter 计时 | Makefile / board targets | repeated summary script | board performance evidence | `test-rvv/features/moment_of_inertia_estimation/src/bench_moi.cpp` |
| `generate_moi_repeated_summary.py` | analysis script | 生成 repeated board summary / manifest | board logs | Evidence Doctor / registry | summary evidence | `test-rvv/features/moment_of_inertia_estimation/script/generate_moi_repeated_summary.py` |
| phase040 board summary | evidence output summary | `PointXYZ` public compute repeated board 结果 | board repeated target | phase result / evaluation | production-public performance | `test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase040_projected_covariance_production/summary.md` |
| phase050 typed summaries | evidence output summary | typed public compute repeated board 结果 | phase050 board targets | phase result / evaluation | typed production-public performance | `test-rvv/features/moment_of_inertia_estimation/log/board/repeated_phase050_*_production/summary.md` |
| evaluation | documentation section | 候选取舍、Traceability Map 和 EvidenceDecision 主归属 | phase docs / code / evidence | reviewer | decision audit | `test-rvv/features/moment_of_inertia_estimation/doc/moment_of_inertia_estimation-evaluation.zh.md` |

## 数值算例

设某个点相对质心的向量为 `d=(2, -1, 0.5)`，当前平面法向量为单位向量 `n=(0, 0, 1)`。标量投影路径先计算：

```text
dot = d . n = 0.5
p = d - dot * n = (2, -1, 0)
covariance contribution:
xx=4, xy=-2, xz=0, yy=1, yz=0, zz=0
```

RVV 路径在一个 VL chunk 中对多个点同时执行同一公式。每个 lane（向量通道）先 gather 当前 index 的 x/y/z，减去 `mean_value_` 得到 `dx/dy/dz`，再用 FMA（融合乘加）形成 `dot`，最后用 `vfredusum` 把每个 lane 的 `px*px`、`px*py` 等 6 个贡献规约成标量累计值。chunk 结束后的累计值乘以 `1 / max(n - 1, 1)`，得到和原 projected cloud covariance 一样的矩阵定义。

## Bench 与证据

| case-filter | entry | points / iterations | RVV hit | proof |
| --- | --- | --- | --- | --- |
| `moi_public_compute` | `MomentOfInertiaEstimation<PointXYZ>::compute()` | phase040 board: 65536 / 5 with 5 repeated runs | yes, projected covariance helper | `PointXYZ` production-public speedup |
| `moi_public_compute_pointxyzi` | `MomentOfInertiaEstimation<PointXYZI>::compute()` | phase050 board: 65536 / 5 with 5 repeated runs | yes | typed scope speedup |
| `moi_public_compute_pointxyzrgb` | `MomentOfInertiaEstimation<PointXYZRGB>::compute()` | phase050 board: 65536 / 5 with 5 repeated runs | yes | typed scope speedup |
| `moi_public_compute_pointxyzrgba` | `MomentOfInertiaEstimation<PointXYZRGBA>::compute()` | phase050 board: 65536 / 5 with 5 repeated runs | yes | typed scope speedup |
| `moi_public_compute_pointxyzrgbnormal` | `MomentOfInertiaEstimation<PointXYZRGBNormal>::compute()` | phase050 board: 65536 / 5 with 5 repeated runs | yes | typed scope speedup |
| `moi_reductions` / `moi_projected_covariance` | test-only helpers | diagnostic board runs | helper-only | 候选归因，不作为 production adoption evidence |

QEMU 只用于 correctness（正确性）和日志形状，不作为性能结论。`bench_moi` 的 dataset label 已按 typed case-filter 输出真实点型，避免 raw log 统一写成 `PointXYZ`。

## 正确性与高效性证据链

| gate | evidence | result |
| --- | --- | --- |
| correctness | `test-rvv/features/moment_of_inertia_estimation` 下 `make run_test_compare` | Std 8/8、RVV 14/14 通过；覆盖 public output、helper hit 和 fallback isolation |
| asm attribution（反汇编归属） | `make dump_bench_rvv` | public bench binary 中命中 `vluxei32`、`vfmacc`、`vfnmsac`、`vfredusum` |
| PointXYZ public board | `log/board/repeated_phase040_projected_covariance_production/summary.md` | 5-run median `1.984x`，min `1.877x`，max `2.076x`，0/5 退化，bucket `positive` |
| PointXYZ Evidence Doctor | `log/board/repeated_phase040_projected_covariance_production/evidence_doctor.md` | Errors=0，Warnings=0，Suggestions=1 |
| typed public board | phase050 四组 summary | PointXYZI `2.132x`，PointXYZRGB `2.395x`，PointXYZRGBA `2.487x`，PointXYZRGBNormal `2.784x`；全部 `positive` |
| typed Evidence Doctor | phase050 四组 Evidence Doctor | 全部 Errors=0，Warnings=0，Suggestions=1 |
| evidence registry | `make evidence_status_phase040 && make evidence_status_phase050` | fresh |

Evidence Doctor 的 suggestion 均为 `binary_identity_missing`。它不阻塞当前采纳；如果后续出现方向反转或跨机器复核，应在 repeated summary / manifest 中补 binary hash 或等价 build identity 后重跑。

## 已拒绝方向

phase030 曾尝试 mean/AABB-only production probe。该补丁在 public `compute()` 边界上 5-run median 只有 `1.018x`，min `0.987x`，2/5 run 低于 1，Evidence Doctor 有 `ba_degradation_frequency` Error。该方向已按用户确认回滚，不属于当前 production behavior。

## 后续边界

当前没有新的默认 RVV 算法优化方向值得继续推进。custom point type、exotic layout（非常规布局）、`Scalar=double` 或更真实 workload 可作为新的 scope expansion phase（范围扩展阶段）重新打开，但不能从本证据链直接外推。
