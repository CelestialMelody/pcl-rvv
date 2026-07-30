# registration/transformation_estimation_point_to_plane_lls_weighted RVV production 说明

## 当前状态

`TransformationEstimationPointToPlaneLLSWeighted::estimateRigidTransformation` 当前结论是：

```text
production-candidate/full-cloud-f32-aos-layout-gated-weighted-block-dispatch-representative-pointtypes
```

已接入 production（生产源码）的范围只有 full-cloud public overload（全云公开入口，source 和 target 按相同下标一一对应）：`Scalar=float`、连续 `weights_`、source 满足 `RVVXYZAoSFloatLayout`、target 满足 `RVVXYZNormalFloatLayout`、规模和 VLEN（向量寄存器最大长度）验收条件成立时，进入 weighted full-cloud block-reduction（带权全云分块规约）RVV 分流。其它情况回到原 `ConstCloudIterator` 标量路径。

当前不覆盖：

- source-indexed（源索引路径）、dual-indices（双索引路径）和 correspondences（对应关系路径）的 production RVV 分流。
- `Scalar=double`。
- 非连续权重存储。
- 不满足 source xyz f32 AoS layout 或 target xyz+normal f32 AoS layout 的点型组合。
- 满足 layout gate 但未逐类型上板的点型性能结论。

稳定证据索引：

| 证据                                                    | 路径                                                                                                                                                             | 用途                                                                                    |
| ------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------- |
| production-dispatch 代表点型 5-run board summary        | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/output/board/production_dispatch_weighted_generic_representative_5run_summary.md` | 当前 EvidenceDecision 的性能主证据。                                                    |
| historical`PointNormal -> PointNormal` subset summary | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/output/board/production_dispatch_weighted_full_block_reduction_5run_summary.md`   | 历史`PointNormal -> PointNormal` 子集证据，已被 generic representative summary 扩展。 |
| full-cloud block-reduction diagnostic summary           | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/output/board/weighted_full_block_reduction_5run_summary.md`                       | 测试专用 helper 的 current-vs-block A/B 诊断证据，不替代 production evidence。          |
| board gtest log                                         | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/output/board/run_test.log`                                                        | board 正确性日志，25 tests passed。                                                     |

QEMU correctness（QEMU 正确性验证）用于构建、路径和日志形状，不作为性能结论。10-case board bench 只保留为 single-run board diagnostic signal（单次板卡诊断信号），不能升级成 repeated board performance conclusion（重复板卡性能结论）。

## 函数语义

该类用于 point-to-plane ICP（点到平面迭代最近点）中的 weighted LLS（带权线性最小二乘）变换估计。公开入口有四类：

| 公开入口                                                                                | 点对来源                                            | 权重来源                  | 当前 production RVV    |
| --------------------------------------------------------------------------------------- | --------------------------------------------------- | ------------------------- | ---------------------- |
| `estimateRigidTransformation(cloud_src, cloud_tgt, matrix)`                           | `source[k] + target[k]`                           | 成员`weights_[k]`       | 仅这一类可能进入 RVV。 |
| `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, matrix)`              | `source[indices_src[k]] + target[k]`              | 成员`weights_[k]`       | 保持标量。             |
| `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, indices_tgt, matrix)` | `source[indices_src[k]] + target[indices_tgt[k]]` | 成员`weights_[k]`       | 保持标量。             |
| `estimateRigidTransformation(cloud_src, cloud_tgt, correspondences, matrix)`          | `index_query/index_match`                         | `correspondence.weight` | 保持标量。             |

原标量 helper 通过 `ConstCloudIterator` 把不同入口统一成同步前进的 source 点、target 点和 weight。逐点流程是：

```text
finite check(source xyz, target xyz, target normal)
  -> normal *= weight
  -> a/b/c/d point-to-plane row formula
  -> accumulate 21 个 ATA 上三角项和 6 个 ATb 项
  -> 补齐 ATA 下三角
  -> Eigen 6x6 solve
  -> construct 4x4 transformation matrix
```

finite check（有限值检查）只覆盖 source/target 坐标和 target normal。weight 为 NaN/Inf 时仍参与计算，这是当前标量语义，RVV 路径也保持这一点。

逐点公式使用加权 normal：

```text
nx = target.normal_x * weight
ny = target.normal_y * weight
nz = target.normal_z * weight

a = nz * sy - ny * sz
b = nx * sz - nz * sx
c = ny * sx - nx * sy
d = nx * dx + ny * dy + nz * dz - nx * sx - ny * sy - nz * sz
```

Eigen solve 和 4x4 矩阵构造每次 estimate 只执行一次，规模固定，不属于当前 RVV 热点。

## 当前采用的优化方式

### Dispatch 与 fallback

full-cloud public overload 完成原有点数和 `weights_.size()` 检查后，在 `__RVV10__` 构建中尝试 `estimatePointToPlaneLLSWeightedFullCloudRVV`。该 helper 只在 `Scalar=float` 时继续；任何 layout、规模、VLEN 或 byte-offset gate 失败都会返回 `false`，公开入口随后自然进入原 `ConstCloudIterator` 标量 helper。

这条分流不改变 public API（公开接口），不改变 indexed/correspondences overload，也不改变非 RVV 构建。

### Generic layout gate

生产 RVV 不再硬编码 `pcl::PointNormal` 的 offset 或 `sizeof`。source 和 target 分别验收：

| 侧     | Gate                                     | 证明内容                                                                                                                                  | 不证明内容                                                      |
| ------ | ---------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------- |
| source | `RVVXYZAoSFloatLayout<PointSource>`    | `x/y/z` 是 PCL traits 注册的单个 `float` 字段，POD/standard-layout、`sizeof(PointSource)` 和字段 offset 满足 AoS f32 跨步加载前提。 | 不证明 normal 字段，也不证明额外字段参与语义。                  |
| target | `RVVXYZNormalFloatLayout<PointTarget>` | `x/y/z/normal_x/normal_y/normal_z` 都是单个 `float` 字段，并满足 AoS f32 跨步加载前提。                                               | 不证明所有 normal 点型都逐类型上板，也不证明`Scalar=double`。 |

source 和 target 分别使用自己的 stride、POD layout 和字段 offset。当前 board representative pointtypes（代表性点类型）覆盖 `PointNormal -> PointNormal`、`PointXYZ -> PointNormal` 和 `PointXYZ -> PointXYZINormal`。其它 gate-allowed 点型只能继承 correctness 和 layout 证据，不能写成逐类型性能已证明。

### Contiguous `weights_`

production 只使用 full-cloud 成员 `weights_`，因此 weight 是连续 `std::vector<float>` 数据。RVV chunk 用 `vle32.v` 连续加载 `weights_[i..i+vl)`，并在计算公式前做：

```text
weighted normal = target normal * weight
```

这条语义不同于 correspondences 入口：correspondences 的权重来自每个 `pcl::Correspondence::weight`，需要先展开成局部 vector。该展开路径没有接 production。

### block-reduction

早期 full-cloud diagnostic baseline 使用 `vcompress + 64-lane fixed buffer + tail`：先把有效 lane 的 `a/b/c/d/nx/ny/nz` 压缩到固定缓冲，再由 tail 累加。它的优点是保留有效 lane 顺序，便于解释 finite mask；风险是额外 store/load、固定 buffer、以及编译器可能把 tail 自动变成 `vfredosum.vs`，使规约树不再由源码显式控制。

当前 production 采用 block-reduction，是因为它直接在向量寄存器中累加 normal-equation partial sums（法方程部分和），避免把每个有效 lane 写回中间 buffer。每个 block 重复读取同一段 source/target/weights 四次，分成 A/B/C/N 四组，以减少同时持有的向量寄存器数量。重复 load 是有意取舍：它用更多规则跨步加载换取更少 buffer 写回和更清晰的显式 `vfmacc` partial-sum accumulation（部分和累加）与 `vfredosum` 横向规约归属。

### A/B/C/N block groups

每个 block 覆盖 `8 * vlmax` 行左右，四组分别累加 21 个 `ATA` 上三角项和 6 个 `ATb` 项：

| 组 | 累加项                                                                                         | 说明                                                                    |
| -- | ---------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------- |
| A  | `a*a`、`a*b`、`a*c`、`a*nx`、`a*ny`、`a*nz`、`a*d`                               | 同时统计`accepted_points`，因为 finite mask 在 A 组已经覆盖该 block。 |
| B  | `b*b`、`b*c`、`b*nx`、`b*ny`、`b*nz`、`b*d`                                        | 复用同一行公式，但只保留 B 组 partial sums。                            |
| C  | `c*c`、`c*nx`、`c*ny`、`c*nz`、`c*d`                                                 | 继续累加旋转相关交叉项。                                                |
| N  | `nx*nx`、`nx*ny`、`nx*nz`、`ny*ny`、`ny*nz`、`nz*nz`、`nx*d`、`ny*d`、`nz*d` | 累加法线和平移相关项。                                                  |

每个 VL chunk 只更新当前组的 vector partial sums；同一 A/B/C/N 组扫完整个 block 后，再用显式 `vfredosum.vs` 把这些向量 partial sums 横向规约成标量，并累加到 double normal-equation 中。最后复用原 Eigen solve 和矩阵构造。

### 单个 VL chunk 内部流程

每个 VL chunk（可变向量长度分块）执行相同的行公式：

```text
vsetvl
  -> source xyz: vlse32.v stride load, stride = sizeof(PointSource)
  -> target xyz + normal: vlse32.v stride load, stride = sizeof(PointTarget)
  -> weight: vle32.v contiguous load
  -> finite mask: source/target xyz + target normal
  -> nx/ny/nz = target normal * weight
  -> a/b/c/d formula
  -> invalid lane merge to zero
  -> vfmacc.vv 更新该组 partial sums
```

invalid lane（无效向量通道）被合并为零，因此不贡献 `ATA/ATb`。weight 不参与 finite mask；如果 point/normal 有限但 weight 非有限，RVV 路径会和标量路径一样产生非有限 normal-equation。`vfredosum.vs` 不属于单个 chunk 的内部步骤；它发生在 A/B/C/N 某一组扫完整个 block 之后，用来把该组的 vector partial sums 横向规约成标量。

### 为什么 fused formula 暂缓

unweighted TEPTPL 已经有 fused formula（融合公式）生产候选：它保留 A/B/C/N block-reduction 组织，把逐点 `a/b/c` 改成 `vfmsac` 形态，并把 `d` 改成 `nx*(dx-sx) + ny*(dy-sy) + nz*(dz-sz)` 后用 `vfmacc` 累加。weighted 不能直接继承这个结论。weighted 公式多了 `weight * normal`，并且非有限 weight 语义必须保持可观察；如果把 `d` 的六项和、`d` 的 displacement 形态或 `a/b/c` 改成 FMA contraction（融合乘加收缩），需要重新证明 float 中间舍入、near-cancellation（近似抵消）、`accepted_points`、`ATA/ATb` 和 matrix 预算仍成立。

因此当前 production block-reduction 的状态是：normal-equation partial sums 已经使用 `vfmacc` 做逐 chunk 累加；暂缓的是 `a/b/c/d` point formula tree（逐点公式树）的 fused contraction。当前逐点公式仍保持 `vfmul` + `vfadd/vfsub` 形态。fused formula 进入 production 前需要补：same-chain（同构链路）测试、weighted near-cancellation 样本、非有限 weight 语义保护、production helper 符号内 asm 归属和目标板卡 A/B。

### fused formula follow-up 设计

该 follow-up 只建议先做 test_support diagnostic/component A/B，不先改 production。候选拆成小步验证，避免把 partial-sum `vfmacc` 已成立误写成逐点公式融合也已成立：

| 候选 | 公式树变化 | 主要风险 | 建议顺序 |
| --- | --- | --- | --- |
| `abc-fused` | `a/b/c` 使用 unweighted 同款 `vfmsac` 形态，`d` 保持当前六项展开。 | 改变旋转三项的 float 舍入；weight 已乘到 normal 后会放大 scale-stress 误差。 | 第一批，可隔离 `a/b/c`。 |
| `d-six-term-fma` | 保留 `nx*dx + ny*dy + nz*dz - nx*sx - ny*sy - nz*sz` 六项形态，但用 FMA 做累加。 | 仍改变六项累加舍入；非有限 weight 与大坐标组合可能产生不同 NaN/Inf 传播。 | 第一批或第二批，可和 only-d 对照。 |
| `d-displacement-fused` | 使用 unweighted 同款 `dx-sx`、`dy-sy`、`dz-sz` 后 `vfmacc` 累加。 | 对 near-cancellation 最敏感；`Inf * finite - Inf * finite` 与 `Inf * 0` 语义可能不同。 | 只在非有限 weight 和 near-cancellation 预算闭合后尝试。 |
| `abcd-fused` | 同时采用 `abc-fused` 和 `d` fused 形态。 | 多个舍入树同时变化，难以归因。 | 只作为前面 isolated candidates 通过后的组合项。 |

测试预算必须至少覆盖：

- 非有限 weight：finite point/normal 搭配 `NaN`、`Inf`、`-Inf`、极大有限 weight，确认 current 标量合同、`accepted_points` 和 matrix 输出预算。
- near-cancellation：大绝对坐标、小 source/target 位移，分别覆盖 `d-six-term-fma` 和 `d-displacement-fused`。
- scale-stress（高动态范围压力）：normal、weight、source/target 坐标跨多个数量级，检查 `ATA/ATb` 范数预算和最终 4x4 matrix 预算。
- accepted_points：invalid point/normal lane 仍只由 finite mask 排除，非有限 weight 不改变计数语义。
- normal-equation：对比 `accepted_points`、`ATA/ATb`、matrix 三层预算，避免只看最终 solve 掩盖中间态漂移。

test_support candidate 建议放在现有 weighted `test_support/teptplw_reductions.hpp` 和 `teptplw_candidates.hpp` 中，新增 direct helper 而不是生产 selector。bench case 建议先做 component A/B：

```text
weighted lls component full-cloud block-fused-abc no-solve pointnormal
weighted lls component full-cloud block-fused-d-six-term no-solve pointnormal
weighted lls component full-cloud block-fused-d-displacement no-solve pointnormal
weighted lls normal-equation full-cloud block-fused-abcd pointnormal
```

asm 归属必须在 fused diagnostic helper 符号范围内确认：`abc-fused` 看到逐点 `vfmsac`，`d` candidates 看到逐点公式树里的 `vfmacc/vfmsac`，并且仍能区分 A/B/C/N partial-sum `vfmacc` 和组结束后的显式 `vfredosum`。全二进制里的 Eigen、bench harness 或自动向量化 FMA 不能作为该 helper 的证据。

板卡证据建议采用 representative 5-run A/B summary-only artifact。第一轮只比较 `PointNormal -> PointNormal` 64K/256K 的 current block vs isolated fused candidates；若某个 candidate 稳定正向且数值预算闭合，再扩到 `PointXYZ -> PointNormal` 和 `PointXYZ -> PointXYZINormal`。QEMU timing 不作为性能结论。

建议先提交当前 block-reduction production candidate，再另开 fused formula follow-up worker。理由是当前 production evidence 已围绕 block-reduction 闭合；fused formula 会改变逐点舍入树和非有限 weight 传播，适合用独立 diagnostic/component A/B 和独立板卡摘要审查。

## 范围决策表

| 方向                                         | 状态               | 证据 / 理由                                                                                                                      |
| -------------------------------------------- | ------------------ | -------------------------------------------------------------------------------------------------------------------------------- |
| generic point type gate                      | adopted            | source/target 分别用 PCL RVV layout traits、offset 和 stride；三类代表点型有 production direct tests 和 board 5-run。            |
| block-reduction                              | adopted            | 25 tests、production helper asm attribution、representative production-dispatch 5-run board summary 均闭合。                     |
| representative pointtypes evidence           | adopted            | 板卡覆盖`PointNormal -> PointNormal`、`PointXYZ -> PointNormal`、`PointXYZ -> PointXYZINormal`；文档明确不外推逐类型性能。 |
| fused formula                                | deferred           | 当前已用 `vfmacc` 做 partial-sum accumulation；缺的是 `a/b/c/d` 逐点公式树 fused contraction 的 weighted same-chain、near-cancellation、非有限 weight、asm 和 board A/B 证据。 |
| source-indexed production                    | deferred           | 只有 diagnostic correctness 和 single-run 弱正向，缺 production direct/fallback、repeated board 和符号归属。                     |
| dual-indices production                      | deferred           | 10-case board diagnostic 为负向，且缺 production direct/fallback。                                                               |
| correspondences production                   | deferred           | 10-case board diagnostic 为负向，且包含 index/weight 展开、gather、压缩和 tail 多个成本源；缺消融。                              |
| `Scalar=double`                            | deferred           | 当前 RVV 公式和 layout gate 只批准`Scalar=float`，double 已由 fallback test 保护。                                             |
| 把 diagnostic 10-case 当 production evidence | rejected / not_now | 10-case board 是 single-run diagnostic signal；真实 production 性能只看 production-dispatch representative 5-run。               |

## RowSourcePolicy 与 WeightPolicy 边界

test-rvv 仍保留四类 row source diagnostic，用于说明每条公开入口的数据流差异。它们不等于 production 批准。

| 入口形态        | RowSourcePolicy 取点                                  | WeightPolicy 取权重                               | Bench 计时边界                                                                                                   | 当前结论                                 |
| --------------- | ----------------------------------------------------- | ------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------- | ---------------------------------------- |
| full-cloud      | `source[k] + target[k]`，两侧 stride load。         | `weights_[k]` 连续 load。                       | 输入和权重计时前构造；estimate 内 normal-equation、solve、matrix 计时。                                          | 生产候选只批准这一条的 block-reduction。 |
| source-indexed  | `source[indices_src[k]] + target[k]`。              | `weights_[k]` 连续 load。                       | `pcl::Indices` 输入计时前构造；candidate 内 valid-index scan、`uint32_t` staging、byte-offset prepare 计时。 | 仅保留诊断证据。                         |
| dual-indices    | `source[indices_src[k]] + target[indices_tgt[k]]`。 | `weights_[k]` 连续 load。                       | 两条 index stream 的 staging 和 gather 计时。                                                                    | 仅保留诊断证据，board 单次负向。         |
| correspondences | 展开`index_query/index_match` 后双侧 gather。       | 展开`correspondence.weight` 到临时连续 vector。 | index/weight 展开、offset prepare、gather、solve、matrix 都计时。                                                | 仅保留诊断证据，board 单次负向。         |

## 数值算例

单个有效点：

```text
source = (1, 2, 3)
target = (1.1, 1.9, 3.2)
normal = (0, 0, 1)
weight = 0.5
weighted normal = (0, 0, 0.5)

a = 0.5 * 2 - 0 * 3 = 1
b = 0 * 3 - 0.5 * 1 = -0.5
c = 0 * 1 - 0 * 2 = 0
d = 0.5 * 3.2 - 0.5 * 3 = 0.1
```

该点对 `ATb` 的贡献是 `[0.1, -0.05, 0, 0, 0, 0.05]`，对 `ATA` 的贡献进入 6x6 上三角。RVV block-reduction 计算同一组行项，但把多个 lane 的同类项先放在向量 partial sums 中，再通过 `vfredosum` 写入 normal-equation。因此它和标量 row-order double 累加不是 bitwise 等价，测试使用 `accepted_points`、`ATA/ATb` 和 matrix 的误差预算。

如果一个 VL chunk 有 4 个 lane，lane 2 的 target normal 为 NaN，finite mask 是 `1,1,0,1`。RVV 会把 lane 2 的 `a/b/c/d/nx/ny/nz` 合并为零，A 组 `vcpop` 只统计 3 个 accepted points。weight 即使非有限也不会改变 mask，这是 weighted 标量合同的一部分。

## Bench 与性能证据

QEMU 10-case diagnostic bench 可构建、可解析，checksum 基本对齐；QEMU timing 不作为性能结论。10-case board bench 覆盖 full/current、full/block、source、dual、correspondences 的 64K/256K，但只是一轮 single-run diagnostic signal：full/current 与 full/block 单次正向，source 单次弱正向，dual/correspondences 单次负向。

当前生产性能结论只来自真实 public overload 的 production-dispatch representative 5-run board summary：

| case                            | runs |    64K median/min |   256K median/min | 边界                                                           |
| ------------------------------- | ---: | ----------------: | ----------------: | -------------------------------------------------------------- |
| `pointnormal`                 |    5 | `2.69x / 2.66x` | `2.71x / 2.11x` | `PointNormal -> PointNormal` 子集；256K 有一轮低谷但仍正向。 |
| `pointxyz-to-pointnormal`     |    5 | `2.81x / 2.79x` | `2.83x / 2.69x` | generic source xyz f32 AoS representative。                    |
| `pointxyz-to-pointxyzinormal` |    5 | `2.81x / 2.80x` | `2.85x / 2.83x` | generic source + target xyz/normal f32 AoS representative。    |

这些 case 的 std/RVV 两侧都调用真实 full-cloud public overload，并都通过 `setCorrespondenceWeights(weights)` 使用连续 `weights_`。它们不覆盖 indexed/correspondences、`Scalar=double`、非连续权重或所有 gate-allowed 点型。

## 正确性与高效性证据链

| 链路                        | 证据                                                                                                                                                                       | 结论                                                                                                                                                                                                | 边界                                                                                       |
| --------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------ |
| correctness（正确性）       | QEMU`run_test_compare` std/RVV 各 25 tests passed；board gtest 25 tests passed。                                                                                         | public full-cloud direct、production helper normal-equation、near-cancellation、scale-stress、非有限 point/normal、非有限 weight、`accepted_points`、`ATA/ATb`、matrix 和 fallback 均在预算内。 | 不覆盖非法 index、所有 correspondence 分布、`Scalar=double` RVV 或未上板点型性能。       |
| path / asm（路径 / 反汇编） | `buildPointToPlaneLLSWeightedFullCloudBlockRVV` 独立符号内确认 `vlse32.v`、`vle32.v`、`vfmacc.vv` A/B/C/N partial sums、`vcpop/vmerge` 和显式 `vfredosum.vs`。 | RVV 指令归属于 production helper，不把 Eigen 或其它库里的 FMA/reduction 误归因到当前 helper。                                                                                                       | 全二进制仍有其它 vector 指令，负向路径需要更细消融才能归因。                               |
| performance（性能）         | production-dispatch representative 5-run board summary。                                                                                                                   | 三类代表点型在 64K/256K 上 median/min 均正向，支持当前 production candidate。                                                                                                                       | 10-case diagnostic 不替代 production evidence；QEMU timing 不作性能结论。                  |
| boundary（边界）            | EvidenceDecision 名称和文档均保留`representative-pointtypes`。                                                                                                           | 结论只覆盖 full-cloud、`Scalar=float`、contiguous `weights_`、source xyz f32 AoS、target xyz/normal f32 AoS。                                                                                   | 未逐类型上板的 gate-allowed 点型没有逐类型性能证明；source/dual/correspondences 保持标量。 |

## Fallback 矩阵

| 条件                                                           | RVV 行为                   | 标量语义                                                       |
| -------------------------------------------------------------- | -------------------------- | -------------------------------------------------------------- |
| 非`__RVV10__` 构建                                           | 没有 RVV 尝试。            | 原 full-cloud overload 构造 iterator 并进入 protected helper。 |
| `Scalar` 不是 `float`                                      | RVV helper 返回 false。    | 原模板标量路径；`Scalar=double` 有 fallback test。           |
| `nr_points < 64`                                             | 不进入 block-reduction。   | 小规模保留标量，避免 dispatch 成本。                           |
| `cloud_tgt.size()` 或 `weights_.size()` 不等于 source size | 公开入口原错误路径。       | 保持原错误处理。                                               |
| source 或 target layout gate 失败                              | 不读取 RVV 字段 offset。   | 原标量模板路径；double-normal target fallback 已覆盖。         |
| `vlmax_e32m1() > 64`                                         | 固定 block helper 不授权。 | 标量。                                                         |
| source/target 元素数超过 32-bit byte offset gate               | 不进入 RVV。               | 标量。                                                         |
| source-indexed、dual-indices、correspondences                  | 无 production RVV 分流。   | 原 iterator / correspondence weight 路径。                     |

## 遗留风险与后续条件

- 更多 gate-allowed 点型：当前 generic gate 允许更多 f32 AoS 点型，但板卡只覆盖三类代表组合。新增点型性能结论需要对应 production direct、asm 和 board 抽样。
- fused formula：当前 block-reduction 已有 partial-sum `vfmacc`；后续只评估 `a/b/c/d` 逐点公式树 fused contraction。需要 weighted FMA same-chain、near-cancellation、非有限 weight 语义、符号级 asm 和板卡 A/B 后才能评估。
- source/dual/correspondences：不应直接接 production。下一步应先做消融，分离 index/weight 展开、`vluxei32.v` gather、`vcompress` 写回、自动 tail reduction 和 block-reduction 成本。
- 负向归因：dual/correspondences 单次板卡负向说明当前诊断路径不适合直接接生产，但不能证明 gather、buffer 或 weight 展开是唯一主因。
