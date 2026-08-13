# registration/transformation_estimation_point_to_plane_lls RVV production 说明

## 当前状态摘要

当前已有板卡支撑的 EvidenceDecision：

```text
production-candidate/full-cloud-f32-aos-layout-gated-source-xyz-target-xyznormal-float-rvv-fused-formula-block-dispatch-representative-pointtypes
```

当前 production path（生产路径）覆盖全云顺序扫描（full-cloud，source 和 target 按相同下标一一对应）公开 overload，并把原 exact `PointNormal -> PointNormal, float` gate 扩成 f32 AoS layout-gated（字段为 float32、结构数组字节偏移可安全读取的布局 gate）：source 侧只要求 `x/y/z` 单个 `float` 字段，target 侧要求 `x/y/z/normal_x/normal_y/normal_z` 单个 `float` 字段，输出 `Scalar` 仍只覆盖 `float`。点字段 float32 和输出 `Scalar=float` 是两条不同边界。该候选默认只走 fused-formula production block；current block 只保留在 test-rvv 的 `include/impl/teptpl_reductions.hpp` 作为 diagnostic baseline，fallback 仍回原 `ConstCloudIterator` 标量路径。

`representative-pointtypes` 表示板卡证据覆盖了 gate 允许空间中的代表点型组合，不表示每一种满足 layout gate 的点型都已经逐类型上板。旧 exact `PointNormal -> PointNormal` 是当前 generic candidate 的子集。新增板卡 5-run 覆盖 `PointNormal -> PointNormal`、`PointXYZ -> PointNormal` 和 `PointXYZ -> PointXYZINormal` 三类 production-dispatch case；其它 gate-allowed f32 AoS 点型组合依靠 traits/layout gate、QEMU correctness 和 fallback 审查，不具备逐类型板卡结论。source 单侧索引（source-indexed）、双侧索引（dual-indices）、对应关系索引（correspondences）、weighted LLS 和 `Scalar=double` 仍不在本轮范围。

production patch 位于 `registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls.hpp`。它在 `__RVV10__` 构建下先尝试 generic f32 AoS full-cloud gate，命中时用 fused-formula RVV block-reduction 构造 point-to-plane LLS normal equation（法方程），再复用 Eigen 求解和 4x4 矩阵构造。gate 不满足时回到原 `ConstCloudIterator` 标量 helper。production 不再提供 current block selector。

证据摘要：

| 证据项                   | 当前结论                                                                                                                                                                                                                          | 边界                                                                                                         |
| ------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------ |
| production direct test   | 覆盖 public full-cloud dispatch、`accepted_points`、`ATA/ATb`、matrix、invalid lane、scale stress、小规模 fallback、`PointXYZ -> PointNormal`、`PointXYZ -> PointXYZINormal` 和 `Scalar=double` fallback。              | 不覆盖 indexed/correspondences/weighted/`Scalar=double` RVV。                                              |
| QEMU bench/log-shape     | `production-dispatch full-cloud pointnormal`、`pointxyz-to-pointnormal` 和 `pointxyz-to-pointxyzinormal` case 可构建、可解析，std/RVV checksum 在预算内。                                                                   | QEMU timing 不作为性能结论。                                                                                 |
| 反汇编                   | RVV 构建中可见 strided load、finite mask、逐点 fused-formula 的 `vfmsac/vfmacc`、normal-equation 阶段的`vfmacc`、`vcpop`、`vfredosum` 等指令形态。                                                                          | 反汇编证明路径形态，不单独证明收益。                                                                         |
| 板卡 production dispatch | fused-formula 三类 full-cloud production-dispatch case 都有 5-run 正向 speedup；见第 9 节摘要和`test-rvv/registration/transformation_estimation_point_to_plane_lls/output/board/production_dispatch_generic_representative_5run_summary.md`。 | 只覆盖三类代表点型组合；不外推到所有 gate-allowed 点型、indexed/correspondences/weighted/`Scalar=double`。 |

Evidence policy 是 `summary-only`：长期文档保留命令、摘要数字、证据边界和清理策略，不提交大批 raw run 目录。顶层 tracked output 日志会被工具覆盖，不作为稳定证据来源；稳定证据索引使用 `output/board/production_dispatch_generic_representative_5run_summary.md`。
当前 topic-local evidence registry（证据登记表）位于
`test-rvv/registration/transformation_estimation_point_to_plane_lls/log/evidence_registry.json`；
它记录 summary-only 证据和 QEMU correctness log 的 digest（摘要指纹），用于恢复时检查是否有未登记覆盖或 stale 文档。

函数级决策审计、测试 inventory、bench 审计和 Traceability Map（可追踪性地图）以
`test-rvv/registration/transformation_estimation_point_to_plane_lls/doc/transformation_estimation_point_to_plane_lls-evaluation.zh.md`
为主归属。跨阶段候选搜索空间和恢复条件以
`test-rvv/registration/transformation_estimation_point_to_plane_lls/doc/optimization-roadmap.zh.md`
为主归属。
测试类型、gtest 名称、bench label、evidence registry check 和测试支撑代码地图已拆到 topic-local doc suite：
`test-rvv/registration/transformation_estimation_point_to_plane_lls/README.zh.md`、
`doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md`、
`doc/optimization-evidence.zh.md` 和 `doc/test-support-code-map.zh.md`。

## 正确性与高效性证据链

当前 production candidate 的判断来自分层证据链。每一层只回答自己的问题，组合起来才支撑 EvidenceDecision。

| 判断维度            | 已有证据                                                                                                             | 能说明什么                                                                                               | 不能外推什么                                                                     |
| ------------------- | -------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------- |
| public entry        | `ProductionFullCloud*` tests 和 `production-dispatch` bench。                                                    | std/RVV 两侧都调用真实 full-cloud public overload，不是 test-only shim 或 public-entry-shaped 替代入口。 | 不证明 indexed、dual-indices、correspondences 或 weighted 入口。                 |
| row semantics       | full-cloud row 固定为`source[k] + target[k]`；RVV finite mask 逐字段覆盖 source xyz、target xyz 和 target normal。 | RVV path 复刻标量 iterator 的有效 row 过滤语义。                                                         | 不改变 indexed/correspondences 的非法 index production 行为。                    |
| `accepted_points` | production normal-equation tests 和 invalid-lane tests。                                                             | NaN/Inf row 不计入有效点；A/B/C/N block 分组不会重复计数。                                               | 只看最终 matrix 不能替代这层检查。                                               |
| `ATA/ATb`         | `ProductionFullCloudNormalEquationMatchesStdWithinBudget` 和 scale-stress tests。                                  | fused-formula RVV block-reduction 构造出的法方程在预算内对齐标量 reference。                           | 不承诺 bitwise 等价，因为逐点计算树、reduction tree 和中间精度改变。              |
| matrix output       | public overload matrix tests、generic source/target tests 和 scale-stress tests。                                    | 最终 4x4 输出在 production 预算内对齐标量路径。                                                          | 不单独证明 invalid lane 或`accepted_points` 正确。                             |
| fallback            | 小规模、`Scalar=double`、layout gate 失败和非 RVV 构建审查。                                                       | 非覆盖范围继续走原`ConstCloudIterator` 标量路径。                                                      | 不表示这些范围已有 RVV 性能结论。                                                |
| instruction shape   | `dump_bench_rvv` 和 asm grep。                                                                                     | RVV 构建中出现预期 strided load、finite mask、`vcpop`、`vfmacc` 和 `vfredosum` 指令形态。          | 反汇编只证明路径形态，不单独证明收益。                                           |
| performance         | Milkv-Jupiter 三类代表点型 production-dispatch 5-run summary。                                                       | 真实性能证据显示 64K/256K median 都正向，且 std/RVV 均走同一 public overload。                           | QEMU timing 不作性能结论；三类代表点型不等于所有 gate-allowed 点型都逐类型上板。 |

因此，当前优化可称为 production candidate 的条件是：

- 正确性成立在当前边界内：full-cloud、`Scalar=float`、source xyz / target xyz+normal f32 AoS layout-gated path。
- 高效性来自目标硬件 5-run production-dispatch，而不是 QEMU timing 或 test-only diagnostic。
- 结论必须保留 `representative-pointtypes` 边界：板卡只覆盖 `PointNormal -> PointNormal`、`PointXYZ -> PointNormal` 和 `PointXYZ -> PointXYZINormal` 三类代表组合，不外推到 indexed、correspondences、weighted、`Scalar=double` 或所有 gate-allowed 点型。

## 1. 函数入口作用与 production 标量路径

TEPTPL 是 point-to-plane least linear squares（点到平面线性最小二乘）估计器。它接收 source/target 点对，使用 target normal 构造 6 维线性系统，求解小角度旋转和平移，再生成 4x4 变换矩阵。四类 transformation estimation 数据流的统一背景见 `doc-rvv/registration/transformation_estimation_dataflows-RVV.zh.md`。

公开入口在源码中有四类：

| 公开 overload                                      | 标量 row 语义                                                                     | 当前 RVV 状态                                                                                                            |
| -------------------------------------------------- | --------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------ |
| `cloud_src, cloud_tgt`                           | full-cloud：`source[k] + target[k]`。标量模板不固定点型，但要求字段访问可编译。 | `Scalar=float` 且 source 满足 `RVVXYZAoSFloatLayout`、target 满足 `RVVXYZNormalFloatLayout` 时尝试 RVV；否则标量。 |
| `cloud_src, indices_src, cloud_tgt`              | source-indexed：`source[indices_src[k]] + target[k]`。                          | 标量。                                                                                                                   |
| `cloud_src, indices_src, cloud_tgt, indices_tgt` | dual-indices：`source[indices_src[k]] + target[indices_tgt[k]]`。               | 标量。                                                                                                                   |
| `cloud_src, cloud_tgt, correspondences`          | correspondences：`source[index_query] + target[index_match]`。                  | 标量。                                                                                                                   |

原标量 helper 通过 `ConstCloudIterator` 统一这些入口。每个 row 先检查 source xyz、target xyz 和 target normal 是否 finite；无效 row 被跳过。有效 row 的公式是：

```text
a = nz * sy - ny * sz
b = nx * sz - nz * sx
c = ny * sx - nx * sy
d = nx * dx + ny * dy + nz * dz - nx * sx - ny * sy - nz * sz
```

随后累加 `ATA += [a b c nx ny nz]^T * [a b c nx ny nz]` 和 `ATb += [a b c nx ny nz]^T * d`。循环结束后补齐 `ATA` 下三角，执行 `ATA.inverse() * ATb`，再调用矩阵构造逻辑生成 `transformation_matrix`。

## 2. Production patch 覆盖范围

production patch 拆出了几个窄 helper，用来让公开入口保持短 dispatch（分流逻辑）：

| helper                                       | 作用                                                                                                  |
| -------------------------------------------- | ----------------------------------------------------------------------------------------------------- |
| `buildPointToPlaneLLSFullCloudBlockRVVFusedFormula` | 默认 f32 AoS layout-gated fused-formula RVV block-reduction normal-equation 构造。            |
| `estimatePointToPlaneLLSFullCloudBlockRVVFusedFormula` | fused block 构造法方程后复用求解和矩阵构造。                                              |
| `estimatePointToPlaneLLSFullCloudRVV`      | 仅在`__RVV10__` 下定义的 RVV 尝试层；`Scalar=float` 且 layout gate 命中时只尝试 fused-formula。 |
| `estimateRigidTransformationFullCloudStd`  | full-cloud overload 的标量 fallback wrapper，内部仍构造`ConstCloudIterator` 并调用原标量 helper。   |

公开 full-cloud overload 的 production 结构是：

```text
size check
  -> #if defined(__RVV10__) estimatePointToPlaneLLSFullCloudRVV(...)
       if success return
  -> estimateRigidTransformationFullCloudStd(...)
```

这个 patch 不改变 public API，不改变 non-RVV 构建，不改变 indexed/correspondences overload 的调用链。`Scalar=double`、不满足 source xyz 或 target xyz+normal f32 AoS layout gate 的点型组合仍回到原模板标量路径。

一个需要保留的差异是 debug loss（调试日志中的 loss 复算）：当前 RVV fast path 成功后会在 full-cloud public overload 中提前 `return`，因此不会进入原 `ConstCloudIterator` helper 末尾的 `PCL_DEBUG` loss 计算。默认日志级别下这不改变输出矩阵；开启 debug verbosity 时，full-cloud RVV 命中路径少打印该 loss。补齐这项需要把 debug loss 复算抽成可复用 helper 或在 RVV fast path 后复刻一段调试计算；当前 production phase 将它记录为 remaining risk，不改变 production 行为。

## 3. Dispatch / Gate / Fallback 矩阵

| 条件                                                                                                      | RVV 行为                                        | fallback 行为                                   | 证据                                                                     |
| --------------------------------------------------------------------------------------------------------- | ----------------------------------------------- | ----------------------------------------------- | ------------------------------------------------------------------------ |
| 非`__RVV10__` 构建                                                                                      | 无 RVV 代码路径。                               | 原标量 helper。                                 | std 构建专项测试。                                                       |
| full-cloud`Scalar=float`，source 满足 `RVVXYZAoSFloatLayout`，target 满足 `RVVXYZNormalFloatLayout` | 若规模和 VLEN gate 满足，进入 fused-formula block-reduction。 | gate 失败则标量。                               | `ProductionFullCloud*` tests、三类代表点型 production-dispatch bench。 |
| `nr_points < 64`                                                                                        | 不进入 RVV。                                    | 标量 normal-equation。                          | `ProductionFullCloudSmallInputFallsBackToScalar`。                     |
| `vsetvlmax_e32m1() > 64`                                                                                | 固定缓冲和 block helper 不授权。                | 标量。                                          | gate 代码审查；专项构建覆盖 fallback 形态。                              |
| 点数乘`sizeof(PointSource)` 或 `sizeof(PointTarget)` 可能溢出 `uint32_t` offset                     | 不进入 RVV。                                    | 标量。                                          | gate 代码审查。                                                          |
| `Scalar=double` 或 layout gate 失败                                                                     | 不进入 RVV。                                    | 原模板标量路径。                                | `ProductionFullCloudScalarDoubleFallbackSmoke`、gate 代码审查。        |
| indexed / dual-indices / correspondences / weighted                                                       | 不接 production RVV。                           | 原`ConstCloudIterator` 或 weighted 标量实现。 | 入口代码审查、历史 diagnostic tests。                                    |

fallback 后的可见语义来自原标量 helper。production RVV gate 是 opportunistic dispatch，不是新的 API 合同。

## 4. RVV Block-Reduction 设计与数值边界

block-reduction 的目标是避免早期 fused-reduction 同时持有 27 个 vector accumulator 带来的寄存器压力。它把 21 个 `ATA` 上三角项和 6 个 `ATb` 项分为四组：

| 组 | 累加项                                                | 特点                            |
| -- | ----------------------------------------------------- | ------------------------------- |
| A  | `aa, ab, ac, anx, any, anz, ad`                     | 同时更新`accepted_points`。   |
| B  | `bb, bc, bnx, bny, bnz, bd`                         | 复用同一 block 的`b` 相关项。 |
| C  | `cc, cnx, cny, cnz, cd`                             | 复用同一 block 的`c` 相关项。 |
| N  | `nxnx, nxny, nxnz, nyny, nynz, nznz, nxd, nyd, nzd` | target normal 相关项。          |

每个 row block 默认覆盖 `8 * vlmax` 行。每组重新读取 source/target 字段，用跨步加载（stride load，按固定字节间隔读取结构数组字段）读取 AoS（array of structures，结构数组）布局：source 使用 `RVVXYZAoSFloatLayout<PointSource>::kX/kY/kZ` 和 `sizeof(PointSource)`，target 使用 `RVVXYZNormalFloatLayout<PointTarget>::kX/kY/kZ/kNX/kNY/kNZ` 和 `sizeof(PointTarget)`。随后计算 `a/b/c/d`，用有限值掩码（finite mask）将 invalid lane 合并为零，再在向量寄存器中做 partial sums，最后用 `vfredosum` 横向规约并写回 `PointToPlaneLLSNormalEquation`。

当前板卡覆盖的代表点型组合如下：

| 组合                            | source gate                 | target gate                                        | 板卡证据                                                    |
| ------------------------------- | --------------------------- | -------------------------------------------------- | ----------------------------------------------------------- |
| `PointNormal -> PointNormal`  | source xyz 为 f32 AoS。     | target xyz+normal 为 f32 AoS。                     | production-dispatch 5-run。                                 |
| `PointXYZ -> PointNormal`     | source 只需要 xyz f32 AoS。 | target xyz+normal 为 f32 AoS。                     | production-dispatch 5-run。                                 |
| `PointXYZ -> PointXYZINormal` | source 只需要 xyz f32 AoS。 | target xyz+normal 为 f32 AoS，额外字段不参与公式。 | fused production-dispatch 5-run；未见明显低谷。 |

其它满足 gate 的 source/target 点型类别会命中同一 production dispatch，但当前没有逐类型板卡覆盖。新增特殊 stride、padding、alignment 或字段组合时，应补 production direct、反汇编和板卡抽样后再把结论写成该点型已覆盖。

这个设计有三个有意取舍：

1. 它重复 load/formula，以换取较少的同时活跃 accumulator 和更稳定的寄存器压力。
2. 它改变 reduction tree。标量路径按 row 顺序用 double 累加；RVV 路径先在 float vector partial sums 中规约，再写入 double normal-equation。因此测试使用 `ATA/ATb` 范数预算和 matrix 预算，不承诺 bitwise 等价。
3. 默认 production 已采用 fused-formula 逐点树；current block 只保留在 test-rvv 的 `include/impl/teptpl_reductions.hpp` 作为 diagnostic baseline。`ATA/ATb` 乘积累加阶段仍使用 `vfmacc`。fused 与 current 的差异集中在逐点 `a/b/c/d` 计算树，不是 API 或 layout gate 的扩大。

fused-formula 在 `test-rvv` 中仍保留 `block-fused-formula` direct helper，作为 current vs fused 归因入口。production 默认路径使用同一条 fused 逐点计算树：`a/b/c` 使用 `vfmsac` 形态，`d` 使用 `nx*(dx-sx) + ny*(dy-sy) + nz*(dz-sz)` 后用 `vfmacc` 累加。它明确改变逐点计算树，因此测试和文档只承诺预算内对齐，不承诺 bitwise 等价。

本轮补齐了 `FullCloudBlockFusedFormulaNearCancellationMatchesStdWithinBudget`。该测试使用大绝对坐标、小 source/target 位移、scale-stress（高动态范围压力）和一个 invalid lane（无效向量通道），专门约束 `d` 公式中 `dx-sx`、`dy-sy`、`dz-sz` 接近抵消时的 `accepted_points`、`ATA/ATb` 和 matrix 预算。QEMU std/RVV `run_test_compare` 已通过。

板卡 5-run diagnostic direct A/B 摘要记录在 `test-rvv/registration/transformation_estimation_point_to_plane_lls/output/board/block_fused_formula_5run_summary.md`：current block 64K/256K median 为 `1.37x/1.33x`，fused-formula block 64K/256K median 为 `1.43x/1.38x`。该结果仍只是 direct helper 归因摘要；production 接入使用 production-facing correctness、production-symbol asm attribution 和三类代表点型 production-dispatch 5-run A/B。

## 5. Invalid Lane / Accepted Points / ATA/ATb / Matrix 合同

production RVV 和标量路径必须共同满足四层合同：

| 合同                | 说明                                                                           | 保护测试                                                                                                       |
| ------------------- | ------------------------------------------------------------------------------ | -------------------------------------------------------------------------------------------------------------- |
| invalid lane        | source xyz、target xyz、target normal 中任一 NaN/Inf 都不参与法方程。          | `ProductionFullCloudInvalidLanesMatchStdWithinBudget`、`FullCloudBlockReductionInvalidLanesWithinBudget`。 |
| `accepted_points` | 只统计 finite row；RVV block A 组统计`vcpop`，其它组不重复计数。             | production normal-equation tests。                                                                             |
| `ATA/ATb`         | 上三角和右端项在 reduction-tree 预算内对齐标量 reference。                     | `ProductionFullCloudNormalEquationMatchesStdWithinBudget`。                                                  |
| matrix              | 最终`estimateRigidTransformation` 输出矩阵在预算内对齐标量 public overload。 | `ProductionFullCloudPublicOverloadMatrixMatchesStdWithinBudget`、scale-stress tests。                        |

只看 matrix 不能证明 invalid lane 或 `accepted_points` 正确，因此保留 normal-equation 级测试。

## 6. 数值算例与 VL chunk 图示

下面用 `vl = 4` 的单个 RVV chunk 说明 full-cloud f32 AoS fast path 如何对应标量 row。source 可以是 `PointXYZ` 或其它满足 xyz strong gate 的点型；target 可以是 `PointNormal`、`PointXYZINormal` 或其它满足 xyz+normal strong gate 的点型。为便于手算，假设 target normal 都是单位 z 方向 `(nx, ny, nz) = (0, 0, 1)`，target 点比 source 点在 z 方向高 `1`。第 2 个 lane 含 NaN，因此应被剔除。

```text
lane:             0          1          2          3
source xyz:       (1,2,3)    (2,1,4)    (NaN,0,1)  (0,3,2)
target xyz:       (1,2,4)    (2,1,5)    (0,0,2)    (0,3,3)
target normal:    (0,0,1)    (0,0,1)    (0,0,1)    (0,0,1)
finite mask:      true       true       false      true
accepted_points:  +1         +1         +0         +1
```

对每个有效 lane，公式是：

```text
a = nz * sy - ny * sz = sy
b = nx * sz - nz * sx = -sx
c = ny * sx - nx * sy = 0
d = nx * dx + ny * dy + nz * dz - nx * sx - ny * sy - nz * sz = dz - sz
```

因此这一 chunk 的 staged row 是：

```text
lane:  0      1      2 invalid -> zeroed      3
a:     2      1      0                       3
b:    -1     -2      0                       0
c:     0      0      0                       0
d:     1      1      0                       1
nx:    0      0      0                       0
ny:    0      0      0                       0
nz:    1      1      0                       1
```

block-reduction 会把同一 block 分成 A/B/C/N 四组分别规约：

| 组 | 这一 chunk 会累加的项                                 | 示例结果                                                                                           |
| -- | ----------------------------------------------------- | -------------------------------------------------------------------------------------------------- |
| A  | `aa, ab, ac, anx, any, anz, ad`                     | `aa = 2*2 + 1*1 + 3*3 = 14`，`ab = 2*(-1) + 1*(-2) + 3*0 = -4`，`ad = 2*1 + 1*1 + 3*1 = 6`。 |
| B  | `bb, bc, bnx, bny, bnz, bd`                         | `bb = (-1)^2 + (-2)^2 + 0^2 = 5`，`bd = -1*1 + -2*1 + 0*1 = -3`。                              |
| C  | `cc, cnx, cny, cnz, cd`                             | 本例`c` 全为 0，所以这些项都是 0。                                                               |
| N  | `nxnx, nxny, nxnz, nyny, nynz, nznz, nxd, nyd, nzd` | `nznz = 1*1 + 1*1 + 1*1 = 3`，`nzd = 1*1 + 1*1 + 1*1 = 3`。                                    |

每组内部的 partial sums 先留在 vector accumulator 中；到 block 结束时，`vfredosum` 把每个 vector accumulator 横向规约成一个标量，再写回 `ATA/ATb`。lane 2 已在 finite mask 后置零，并且没有计入 `accepted_points`，所以它不会影响任何 normal-equation 项。

这个例子也说明数值合同为什么使用误差预算而不是 bitwise 等价。标量路径按 row 0、row 1、row 3 的顺序用 double 累加；RVV block path 先在 lane 内用 float partial sums，再按 A/B/C/N 组横向规约并写回 double 矩阵。两者数学项相同，但加法树和中间精度不同。

## 7. Test-RVV Diagnostic 保留策略

RowSourcePolicy（行来源策略）是 test-rvv diagnostic 框架，不是 production dispatch 层。它把“row 从哪里来”和“shared math pipeline（共享数学流水线）如何做 finite mask、公式和 normal-equation”分开，方便分别审查 full-cloud、source-indexed、dual-indices 和 correspondences。production 仍按每个数据流独立证据批准；当前只改 full-cloud `Scalar=float` 的 f32 AoS layout-gated 路径，indexed/correspondences/weighted 没有扩大。

Diagnostic 资产保留策略如下：

| 分组                  | 保留项                                                                 | 证据角色                                                                                              | 不能证明什么                                             |
| --------------------- | ---------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------- | -------------------------------------------------------- |
| production direct     | `ProductionFullCloud*`                                               | 真实 public full-cloud overload 的 dispatch、fallback、数值预算和 generic source/target layout gate。 | indexed/correspondences/weighted/`Scalar=double` RVV。 |
| fallback              | 小规模、layout gate 失败、`Scalar=double`                            | gate 不误伤非覆盖路径。                                                                               | `Scalar=double` 的 RVV 可行性。                        |
| RVV-only diagnostic   | full-cloud block invalid/scale、`InvalidLaneMaskMatchesStd`          | 保护当前 block math 的局部合同。                                                                      | 真实 dispatch。                                          |
| historical diagnostic | safe/trusted/fused/grouped/source-indexed/dual-indices/correspondences | 解释历史方案取舍和不扩展理由。                                                                        | production evidence。                                    |
| bench-only            | component-only、public-entry-shaped shim                               | 归因、形态兼容或成本拆分。                                                                            | 默认生产分流。                                           |

只有入口形态、输入构造、断言和 adversarial 条件完全被其它测试包含时才适合删除。本轮没有删除或重命名测试。

## 8. Bench Case 含义

bench 文件保留三类含义：

| bench case                                                        | 入口                                             | 证明点                                                                            | 边界                                                                  |
| ----------------------------------------------------------------- | ------------------------------------------------ | --------------------------------------------------------------------------------- | --------------------------------------------------------------------- |
| `lls normal-equation full-cloud pointnormal`                    | test-rvv safe staging candidate。                | 历史 full-cloud staging+tail 端到端成本。                                         | 不是 production dispatch。                                            |
| `trusted-dense`                                                 | test-rvv-only dense 消融。                       | 跳过 finite mask 和`vcompress` 后成本是否变化。                                 | 当前 production 仍逐点检查 finite。                                   |
| `fused-reduction` / `grouped-reduction` / `block-reduction` | test-rvv direct helper。                         | reduction 组织的正确性和历史性能比较。                                            | 只有 block 通过 production-dispatch case 才成为 production evidence。 |
| `block-fused-formula`                                           | test-rvv direct helper。                         | 归因 fused 逐点公式树；near-cancellation 测试已补。                                                     | direct helper 不替代 production-dispatch；QEMU timing 不证明性能。    |
| `public-entry-shaped full-cloud block-reduction`                | std 侧 public overload，RVV 侧 bench-only shim。 | full-cloud 输入/输出形态下 block helper 与公开入口形态兼容。                      | public-entry-shaped 不等于 production dispatch。                      |
| `production-dispatch full-cloud pointnormal`                    | std/RVV 两侧都调用真实公开 full-cloud overload。 | exact PointNormal 子集的 QEMU/板卡 A/B。                                          | 不能外推到 indexed/correspondences/weighted。                         |
| `production-dispatch full-cloud pointxyz-to-pointnormal`        | std/RVV 两侧都调用真实公开 full-cloud overload。 | `PointXYZ -> PointNormal` generic source dispatch 的 QEMU/板卡 A/B。            | QEMU timing 不证明性能；性能结论只使用板卡 5-run。                    |
| `production-dispatch full-cloud pointxyz-to-pointxyzinormal`    | std/RVV 两侧都调用真实公开 full-cloud overload。 | `PointXYZ -> PointXYZINormal` generic source+target dispatch 的 QEMU/板卡 A/B。 | fused 5-run 未见明显低谷；QEMU timing 不证明性能。                    |
| indexed / dual-indices / correspondences rows                     | test-rvv diagnostic wrappers。                   | 历史负向和分布敏感性。                                                            | diagnostic evidence 不等于 production evidence。                      |
| `--component-only` rows                                         | 局部分段消融。                                   | load/gather、formula、mask/compress、tail、no-solve 的成本线索。                  | 不是完全正交 profile，不单独决定 production。                         |

correspondences 和 indexed 路径的退化不能单因归因为 gather。对应关系 case 还包含 query/match 展开、容器访问、baseline 差异和分布局部性；没有 profile 或额外消融时只能写成受证据约束的假设。

## 9. QEMU、反汇编和板卡证据边界

QEMU 用途：

- 构建 std/RVV 二进制。
- 验证专项测试和 checksum。
- 验证 bench 日志格式和 case filter。

QEMU timing 不作为性能结论。性能结论只来自板卡或目标硬件。

反汇编用途：

- 确认 RVV 构建中出现预期 strided load、finite mask、公式、`vcpop`、normal-equation 阶段的 `vfmacc` 和 `vfredosum` 指令形态。
- 辅助定位 helper 是否被编译为 RVV 路径。

板卡证据：

| case                                                       | runs | 64K speedup                         | 256K speedup                        | 解释                                                                                                      |
| ---------------------------------------------------------- | ---: | ----------------------------------- | ----------------------------------- | --------------------------------------------------------------------------------------------------------- |
| block-reduction diagnostic direct                          |    5 | median`1.27x`                     | median`1.27x`                     | 证明 test-rvv direct helper 在目标硬件上有正向，但不是 production dispatch。                              |
| public-entry-shaped block                                  |    5 | median`2.67x`, min `2.64x`      | median`2.69x`, min `2.67x`      | 强形态信号，不等于真实生产分流。                                                                          |
| production-dispatch full-cloud pointnormal                 |    5 | median/min/p10`2.80x/2.77x/2.78x` | median/min/p10`2.82x/2.81x/2.81x` | fused exact PointNormal 子集的 production-dispatch 性能证据。                                             |
| production-dispatch full-cloud pointxyz-to-pointnormal     |    5 | median/min/p10`3.13x/3.11x/3.12x` | median/min/p10`3.15x/3.14x/3.14x` | fused generic source xyz gate 的代表点型 production-dispatch 性能证据。                                   |
| production-dispatch full-cloud pointxyz-to-pointxyzinormal |    5 | median/min/p10`3.11x/3.11x/3.11x` | median/min/p10`3.14x/3.12x/3.12x` | fused generic target xyz+normal gate 的代表点型 production-dispatch 性能证据；未见明显低谷。              |

稳定证据索引：`test-rvv/registration/transformation_estimation_point_to_plane_lls/output/board/production_dispatch_generic_representative_5run_summary.md`。该文件记录命令、case filter、current/fused run1..run5 来源、analyzer SHA256、当轮本机临时 raw archive 路径和完整 values；长期审计以 summary artifact 内的 values、命令和 analyzer hash 为准，不依赖临时归档永久存在。output 策略是 summary-only：raw run 目录和临时 QEMU 目录不作为长期提交内容；文档保留摘要数字和命令边界。

## 10. 负向历史方案

历史方案只保留为方案取舍，不继续做性能探索：

| 方案                       | 当前处理            | 原因                                                                                                                |
| -------------------------- | ------------------- | ------------------------------------------------------------------------------------------------------------------- |
| safe staging + scalar tail | 不接 production。   | full-cloud/source-indexed 没有稳定板卡收益，tail 和 staging 成本不能覆盖。                                          |
| trusted-dense              | 不接 production。   | 需要改变 finite 检查合同；当前 production 仍逐点检查 NaN/Inf。                                                      |
| fused-reduction            | 不接 production。   | 27 个长期 vector accumulator 带来寄存器压力，256K 稳定性不足；这不等于禁止未来对逐点公式或累加阶段继续做 FMA 审计。 |
| grouped-reduction          | 不接 production。   | 数值可控但板卡负向，不能覆盖维护成本。                                                                              |
| indexed / dual-indices     | 不接 production。   | 分布敏感，独立双索引负向；需要单独 profile 和更窄策略。                                                             |
| correspondences            | 不接 production。   | query/match 展开、容器访问和双侧 index stream 共同影响，不能只按 gather 归因。                                      |
| weighted                   | 不在本 topic 范围。 | weighted 语义和 evidence 单独维护。                                                                                 |

这些负向结论不能被 full-cloud production 正向收益抵消。

## 11. 生产接入评估与仍不覆盖范围

生产接入评估：

| 项         | 状态                                                                                                                               |
| ---------- | ---------------------------------------------------------------------------------------------------------------------------------- |
| patch 大小 | 窄 helper + full-cloud public overload 短路，维护边界可控。                                                                        |
| public API | 不变。                                                                                                                             |
| fallback   | 非覆盖路径回原标量 helper。                                                                                                        |
| 数值风险   | reduction tree 改变，用`accepted_points`、`ATA/ATb` 和 matrix 预算测试约束。                                                   |
| 性能       | fused-formula 三类代表点型 full-cloud production-dispatch 板卡 5-run 都正向，未见明显低谷。 |
| 可审性     | test-rvv 仍保留历史诊断，但文档明确区分 evidence 角色。                                                                            |
| debug loss | RVV fast path 成功后提前返回，不打印原 scalar helper 的 debug loss。默认输出矩阵不变；debug verbosity 下日志行为不同。             |

仍不覆盖：

- `Scalar=double`。
- 不满足 source xyz / target xyz+normal f32 AoS layout gate 的点型组合。
- 满足 layout gate 但未被三类代表点型覆盖的逐类型板卡结论。
- source-indexed、dual-indices、correspondences。
- weighted transformation estimation。
- 非 RVV 目标上的性能收益。
- 更复杂对象生命周期或上游完整 registration 场景。
- debug verbosity 下的 loss 日志等价性。

实现结构已经把 full-cloud 标量 normal-equation reference 放在 `test-rvv/registration/transformation_estimation_point_to_plane_lls/include/impl/teptpl_common.hpp`，production fallback 继续走原 `ConstCloudIterator` 标量 helper。production header 仍保留 RVV normal-equation、求解和矩阵构造 helper；RVV block helper 内部也有 A/B/C/N 四组重复 load/formula。它们让边界清晰，但 helper size 和重复逻辑仍是审查项。若合入前要求更紧凑的源码形态，可考虑压缩 helper，并重跑 correctness。

## 12. Evidence / Output 策略

本主题使用 `summary-only` evidence policy：

- 不提交 `output/board/*_run*/`、`output/qemu/*/` 等 raw run 目录。
- 不把已跟踪顶层 output 日志作为长期证据；它们会被 runner 覆盖。
- 文档记录命令、摘要数字、case 边界和不能证明的范围。
- 若未来需要提交日志，应先运行 `make sanitize_output_logs` 和 `make check_output_logs_sanitized`，或使用 `test-rvv/script/sanitize_evidence_logs.py` 对指定日志脱敏检查。

当前验证记录包含 QEMU correctness、dump bench、production-dispatch QEMU bench，以及三类 representative pointtypes production-dispatch 板卡 5-run。generic EvidenceDecision 已基于这些摘要升级为 fused-formula 默认 production candidate，但结论名保留 `representative-pointtypes` 边界。

fused-formula variant 另有 `block_fused_formula_5run_summary.md` 作为 diagnostic direct board A/B 摘要。near-cancellation 数值测试已经加入 `run_test_compare`，并且 production-facing fused correctness、asm attribution 和 5-run production-dispatch A/B 已闭合。

bench label 字典、checksum 口径、Evidence Doctor 边界和 registry check 命令不在长期主题文档重复维护；
主归属是 `test-rvv/registration/transformation_estimation_point_to_plane_lls/doc/benchmark-and-evidence.zh.md`。

## 13. 后续方向

当前状态是：full-cloud f32 AoS layout-gated `Scalar=float` path 已有三类代表点型 fused production-dispatch 板卡 5-run 支撑；exact `PointNormal -> PointNormal` 是该候选的子集。full-cloud 之外的数据流保持标量或作为独立 follow-up 重新取证。

如果以后继续扩展当前主题，应拆成独立 follow-up：

| 方向                      | 需要先补的证据                                                                                                                                                                               |
| ------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 更多 f32 AoS 点型实例     | 在现有 source`RVVXYZAoSFloatLayout` / target `RVVXYZNormalFloatLayout` gate 基础上继续逐类型确认 traits/POD/layout；若新增特殊布局或字段组合，需要补 production direct、asm 和板卡抽样。 |
| `Scalar=double`         | 单独数值预算、求解矩阵对拍、目标硬件 bench。                                                                                                                                                 |
| indexed / correspondences | profile 或消融拆分 gather、query/match 展开、baseline 和分布局部性；不能沿用 full-cloud 结论。                                                                                               |
| trusted-dense             | 明确`is_dense` 是否足以改变 finite 合同，并补 invalid-lane 负向测试。                                                                                                                      |
