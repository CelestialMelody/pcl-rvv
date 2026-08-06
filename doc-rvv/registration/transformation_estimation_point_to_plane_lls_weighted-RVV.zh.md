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
| fused formula PointNormal diagnostic 5-run summary       | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/output/board/weighted_fused_formula_pointnormal_5run_summary.md`                  | fused formula follow-up 的测试专用 A/B 诊断证据，结论为不接 fused production。          |
| fused formula production-shaped PointNormal 5-run summary | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/output/board/weighted_fused_formula_production_shaped_pointnormal_5run_summary.md` | A 为真实 public production block baseline，B 为测试专用 fused helper；只支持继续复核，不替代 production evidence。 |
| fused formula generic abc representative 5-run summary | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/output/board/weighted_fused_formula_generic_abc_representative_5run_summary.md` | `abc-fused`/`abc-fused-ilp` 的三类代表点型 B/A；结论仍为不接 fused production。 |
| fused formula generic formula representative 5-run summary | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/output/board/weighted_fused_formula_generic_formula_representative_5run_summary.md` | `abc`、D 项和 `abcd` 的三类代表点型 warm-up RVV-vs-RVV B/A；`abcd` 最稳，但仍不接 fused production。 |
| QEMU gtest logs                                          | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/output/qemu/run_test_{std,rvv}.log`                                               | QEMU std/RVV 各 31 tests passed，包含 generic abc 与 D/ABCD fused representative correctness。 |
| board gtest log                                         | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/output/board/run_test.log`                                                        | board 31 tests passed，包含 generic abc 与 D/ABCD fused representative correctness。 |

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

### fused formula follow-up A/B 结果

unweighted TEPTPL 已经有 fused formula（融合公式）生产候选：它保留 A/B/C/N block-reduction 组织，把逐点 `a/b/c` 改成 `vfmsac` 形态，并把 `d` 改成 `nx*(dx-sx) + ny*(dy-sy) + nz*(dz-sz)` 后用 `vfmacc` 累加。weighted 不能直接继承这个结论。weighted 公式多了 `weight * normal`，并且非有限 weight 语义必须保持可观察；如果把 `d` 的六项和、`d` 的 displacement 形态或 `a/b/c` 改成 FMA contraction（融合乘加收缩），需要重新证明 float 中间舍入、near-cancellation（近似抵消）、`accepted_points`、`ATA/ATb` 和 matrix 预算仍成立。

当前 production block-reduction 的状态不变：normal-equation partial sums 已经使用 `vfmacc` 做逐 chunk 累加；fused formula A/B 只改变 `a/b/c/d` point formula tree（逐点公式树）的 contraction。production 仍保持 `vfmul` + `vfadd/vfsub` 逐点公式树，不采用 fused point formula。

本节使用三种对比口径，不能混读：

| 口径 | A 侧 | B 侧 | 指标 | 能证明什么 |
| --- | --- | --- | --- | --- |
| direct diagnostic A/B | test-rvv block-reduction helper | test-rvv fused helper | `B/A = A_rvv_ms / B_rvv_ms`，`>1` 表示 B 更快 | 只证明测试专用 helper 之间的相对形状。 |
| production-shaped A/B | 真实 full-cloud public overload，经当前 production block dispatch | test-rvv fused helper | `B/A = A_rvv_ms / B_rvv_ms`，`>1` 表示 B 更快 | 说明 fused 是否值得进入 production integration loop；B 侧仍不是生产 fused path。 |
| std/RVV speedup | 同一 case 的 std 构建 | 同一 case 的 RVV 构建 | `std_ms / rvv_ms` | 只说明该 case 自身 RVV 形状，不等于 fused 相对 block baseline 的收益。 |

本轮在 `test_support/teptplw_reductions.hpp` 和 `teptplw_candidates.hpp` 中加入四类 testing-only（仅测试使用）diagnostic/component candidates，并和当前 block baseline、标量 reference 对拍：

| 候选 | 公式树变化 | QEMU correctness / numeric | 板卡 A/B 结论 |
| --- | --- | --- | --- |
| `abc-fused` / `abc-fused-ilp` | `a/b/c` 使用 unweighted 同款 `vfmsac` 形态，`d` 保持当前六项展开；ILP 版只重排源码顺序。 | 31 tests 覆盖 same-chain、near-cancellation、scale-stress、非有限 point/normal、非有限 weight、`accepted_points`、`ATA/ATb`、matrix 和三类代表点型。 | 当前二进制中 `abc` 与 `abc-ilp` RVV 指令序列相同；只保留诊断。 |
| `d-six-term-fma` / `d-six-term-fma-ilp` | 保留 `nx*dx + ny*dy + nz*dz - nx*sx - ny*sy - nz*sz` 六项形态，但用 FMA 做累加；ILP 版交错 target/source accumulator。 | 同上，并补三类代表点型 correctness。 | generic warm-up 5-run 中 `PointNormal` component no-solve 不稳；ILP 版 asm 与非 ILP 等价。 |
| `d-displacement-fused` / `d-displacement-fused-ilp` | 使用 unweighted 同款 `dx-sx`、`dy-sy`、`dz-sz` 后 `vfmacc` 累加；ILP 版交错独立差值和 abc 项。 | 同上。 | 两个 `PointXYZ` 代表组合强，但 `PointNormal` component no-solve 不稳；ILP 版 asm 与非 ILP 等价。 |
| `abcd-fused` / `abcd-fused-ilp` | 同时采用 `abc-fused` 和 `d-displacement-fused`。 | 同上，并补 generic formula warm-up 5-run 与当前二进制 asm 归因。 | 本轮最稳，适合作为下一轮 production-loop 起点；仍缺真实 production fused helper 和 production-symbol asm attribution。 |

`abc-fused` 的“减少公式指令”只相对当前非 fused block baseline 而言：baseline 的 `a/b/c` 每项通常是两个 `vfmul` 加一个 `vfsub`，fused 形态每项是一个 `vfmul` 加一个 `vfmsac`。`abc-fused` 和 `abc-fused-ilp` 彼此的 intrinsic 数相同，都是三条 seed multiply 加三条 `vfmsac`；ILP 版只改变源码顺序，只有反汇编显示 hot path 确实不同，才能把它视为独立机器码候选。

新增 tests 明确检查非有限 weight 语义、near-cancellation、scale-stress、`accepted_points`、`ATA/ATb` 和 matrix。非有限 point/normal 仍由 finite mask 排除；非有限 weight 不改变 `accepted_points`，但会继续影响 normal-equation，这一点已经和标量 reference 对齐。

新增 bench case 分两层：component no-solve 只测 normal-equation 构造成本，full estimate 包含 solve 和 matrix 构造。当前 case 是 diagnostic direct helper，不是真实 production dispatch：

```text
weighted lls component full-cloud block-fused-abc no-solve pointnormal
weighted lls full-cloud block-fused-abc pointnormal
weighted lls component full-cloud block-fused-d-six-term no-solve pointnormal
weighted lls full-cloud block-fused-d-six-term pointnormal
weighted lls component full-cloud block-fused-d-displacement no-solve pointnormal
weighted lls full-cloud block-fused-d-displacement pointnormal
weighted lls component full-cloud block-fused-abcd no-solve pointnormal
weighted lls full-cloud block-fused-abcd pointnormal
--case-filter generic-fused-formula 覆盖三类代表点型的 block-fused-* component no-solve 和 production-shaped full estimate
```

asm 归属在 fused diagnostic helper 模板符号范围内确认：逐点公式树里的 `vfmsac/vfmacc` 可归因到 fused candidates，A/B/C/N partial-sum `vfmacc` 仍是 block-reduction 累加，`vfredosum` 仍只属于每组 block 扫完后的横向规约。全二进制里的 Eigen、bench harness 或自动向量化 FMA 不能作为该 helper 的证据。

板卡证据分四层。`output/board/weighted_fused_formula_pointnormal_5run_summary.md` 是 direct diagnostic A/B，std/RVV 双侧调用 test-rvv diagnostic helpers；其中 std/RVV speedup 不能直接读成 fused 相对 block baseline 的收益，必须看文件内新增的 fused-vs-block B/A 表。`output/board/weighted_fused_formula_production_shaped_pointnormal_5run_summary.md` 使用真实 public production block baseline 作为 A、测试专用 fused helper 作为 B；它显示 `abc-fused` 和 `abcd-fused` 在 `PointNormal -> PointNormal` 上有正向 B/A。`output/board/weighted_fused_formula_generic_abc_representative_5run_summary.md` 进一步把 `abc-fused` 和 `abc-fused-ilp` 扩展到三类代表点型。最新 `output/board/weighted_fused_formula_generic_formula_representative_5run_summary.md` 覆盖 `abc`、D 项和 `abcd`，在 warm-up 5-run 中 `abcd` / `abcd-ilp` 最稳，但 B 侧仍不是 production fused path。QEMU timing 不作为性能结论。

fused follow-up 的结论是：

```text
no-production-for-fused/generic-formula-diagnostic
```

它不改变当前 weighted production implementation，也不把 source-indexed、dual-indices 或 correspondences 推进到 production。

## 范围决策表

| 方向                                         | 状态               | 证据 / 理由                                                                                                                      |
| -------------------------------------------- | ------------------ | -------------------------------------------------------------------------------------------------------------------------------- |
| generic point type gate                      | adopted            | source/target 分别用 PCL RVV layout traits、offset 和 stride；三类代表点型有 production direct tests 和 board 5-run。            |
| block-reduction                              | adopted            | production helper asm attribution、representative production-dispatch 5-run board summary、QEMU 31 tests 和 board 31 tests 均闭合。 |
| representative pointtypes evidence           | adopted            | 板卡覆盖`PointNormal -> PointNormal`、`PointXYZ -> PointNormal`、`PointXYZ -> PointXYZINormal`；文档明确不外推逐类型性能。 |
| fused formula                                | attempted / no-production | test-only candidates 已通过 QEMU/board 31 tests、diagnostic asm attribution、direct diagnostic B/A、production-shaped PointNormal 5-run B/A、generic abc representative 5-run 和 generic formula warm-up 5-run；`abcd` 最稳但仍缺 production-symbol fused evidence，因此不接 production。 |
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

fused formula follow-up 另有 PointNormal direct diagnostic 和 production-shaped 5-run A/B summary。它们的用途是判断逐点公式树 fused contraction 是否值得进入 production loop，不改变上表的生产结论。这里的 B/A 固定表示 `A_rvv_ms / B_rvv_ms`，`>1` 才表示 fused candidate 比 block baseline 更快；direct diagnostic 的 std/RVV speedup 看起来有正向项，但 fused-vs-block B/A 才是主判断：

| candidate | direct 64K B/A median/min | direct 256K B/A median/min | production-shaped 64K B/A median/min | production-shaped 256K B/A median/min | 结论 |
| --- | ---: | ---: | ---: | ---: | --- |
| `abc-fused` | `1.08x / 0.99x` | `0.96x / 0.88x` | `1.03x / 1.02x` | `1.11x / 1.09x` | PointNormal production-shaped 正向；generic 代表点型复核见下表，仍不闭合。 |
| `d-six-term-fma` | `1.14x / 1.03x` | `0.98x / 0.65x` | `1.06x / 1.05x` | `1.13x / 0.80x` | 256K 有明显退化 run，不接 production。 |
| `d-displacement-fused` | `1.12x / 1.00x` | `0.81x / 0.61x` | `1.05x / 1.05x` | `1.10x / 0.72x` | 256K 有明显退化 run，不接 production。 |
| `abcd-fused` | `1.11x / 0.78x` | `0.92x / 0.60x` | `1.05x / 1.05x` | `1.13x / 1.09x` | PointNormal production-shaped 正向，但组合收益不可单独归因，缺代表点型和 production-symbol 证据。 |

generic `abc-fused` representative 5-run B/A 进一步覆盖三类代表点型。这里的 A 是 component block baseline 或真实 public production block baseline；B 是 test-rvv generic fused helper。重点行如下：

| candidate / layer | `pointnormal` 64K / 256K median-min | `pointxyz-to-pointnormal` 64K / 256K median-min | `pointxyz-to-pointxyzinormal` 64K / 256K median-min | 结论 |
| --- | ---: | ---: | ---: | --- |
| `abc-fused` component no-solve | `1.01x / 1.00x`、`0.96x / 0.89x` | `1.02x / 1.02x`、`1.33x / 0.73x` | `1.02x / 1.01x`、`1.02x / 0.40x` | component 层有 256K 低谷。 |
| `abc-fused` production-shaped full | `1.03x / 1.03x`、`1.05x / 1.05x` | `1.04x / 1.04x`、`1.05x / 1.04x` | `1.04x / 1.03x`、`0.85x / 0.63x` | generic target 256K 负向，不接 production。 |
| `abc-fused-ilp` production-shaped full | `1.03x / 1.03x`、`1.05x / 0.96x` | `1.04x / 1.04x`、`1.04x / 0.76x` | `1.04x / 1.03x`、`1.22x / 1.05x` | 有正向项，但 asm 不是独立公式形状。 |

## 正确性与高效性证据链

| 链路                        | 证据                                                                                                                                                                       | 结论                                                                                                                                                                                                | 边界                                                                                       |
| --------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------ |
| correctness（正确性）       | QEMU`run_test_compare` std/RVV 各 31 tests passed；board gtest 31 tests passed。                                                                                         | public full-cloud direct、production helper normal-equation、near-cancellation、scale-stress、非有限 point/normal、非有限 weight、`accepted_points`、`ATA/ATb`、matrix、fallback 和 generic fused representative correctness 均在预算内。 | 不覆盖非法 index、所有 correspondence 分布、`Scalar=double` RVV 或未上板点型性能。 |
| path / asm（路径 / 反汇编） | `buildPointToPlaneLLSWeightedFullCloudBlockRVV` 独立符号内确认 `vlse32.v`、`vle32.v`、`vfmacc.vv` A/B/C/N partial sums、`vcpop/vmerge` 和显式 `vfredosum.vs`；fused diagnostic helper 模板符号内确认逐点 `vfmsac/vfmacc`。 | production RVV 指令归属于 block helper；generic fused helper 符号实例有独立归属，D/ABCD 公式收缩进入当前二进制；当前所有 `*-ilp` 与对应非 ILP mode 的 RVV 指令序列相同。 | fused asm 不是 production-symbol attribution；`PointNormal` D/ABCD 有 out-of-line `group_n` 和 vector save/restore，需要生产路径继续消融。 |
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
- fused formula：当前 block-reduction 已有 partial-sum `vfmacc`。逐点公式树 fused contraction 已完成 diagnostic/component A/B；`abc`、D 项和 `abcd` 已迁移成 generic layout-gated test_support helper 并补 warm-up 5-run B/A。`abcd` / `abcd-ilp` 当前最稳，但二者 asm 等价，且仍缺真实 production fused helper 和 production-symbol asm attribution。结论是 `no-production-for-fused/generic-formula-diagnostic`。
- source/dual/correspondences：不应直接接 production。下一步应先做消融，分离 index/weight 展开、`vluxei32.v` gather、`vcompress` 写回、自动 tail reduction 和 block-reduction 成本。
- 负向归因：dual/correspondences 单次板卡负向说明当前诊断路径不适合直接接生产，但不能证明 gather、buffer 或 weight 展开是唯一主因。
