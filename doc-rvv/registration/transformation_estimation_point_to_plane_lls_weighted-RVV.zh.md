# registration/transformation_estimation_point_to_plane_lls_weighted RVV production 说明

## 当前状态

`TransformationEstimationPointToPlaneLLSWeighted::estimateRigidTransformation` 当前结论是：

```text
bounded-production-candidate/full-cloud-adopted-and-source-indexed-block-fused-probe-f32-aos-valid-index-positive-with-warnings
```

已接入 production（生产源码）的范围有两条 public overload：

- full-cloud public overload（全云公开入口，source 和 target 按相同下标一一对应）。`Scalar=float`、连续 `weights_`、source 满足 `RVVXYZAoSFloatLayout`、target 满足 `RVVXYZNormalFloatLayout`、规模和 VLEN（向量寄存器最大长度）验收条件成立时，进入 weighted full-cloud block-reduction（带权全云分块规约）RVV 分流。其它情况回到原 `ConstCloudIterator` 标量路径。
- source-indexed public overload（源索引公开入口，source 由 `indices_src` 指定，target 顺序扫描）。`Scalar=float`、连续 `weights_`、source index 全部有效、source 满足 `RVVXYZAoSFloatLayout`、target 满足 `RVVXYZNormalFloatLayout`、规模和 VLEN 验收条件成立时，Phase 031 后先进入 source-indexed `block-fused-abcd-ilp` production probe（生产探针）RVV 分流；probe helper 失败时回 staged-gather / compressed-tail helper，再失败时回到原 `ConstCloudIterator` 标量路径。当前证据是 bounded production candidate，不是 clean adopted：真实 production probe 的 6 个代表 case median 均正向，但 source-indexed-specific asm、binary identity、taskset metadata 和 262144 长尾解释仍未闭合。Phase 030 的 source-indexed-family repeated diagnostic 保留为 historical diagnostic / harness-risk signal。

当前不覆盖：

- dual-indices（双索引路径）和 correspondences（对应关系路径）的 production RVV 分流。
- `Scalar=double`。
- 非连续权重存储。
- 不满足 source xyz f32 AoS layout 或 target xyz+normal f32 AoS layout 的点型组合。
- 满足 layout gate 但未逐类型上板的点型性能结论。

稳定证据索引：

| 证据                                                    | 路径                                                                                                                                                             | 用途                                                                                    |
| ------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------- |
| production-dispatch 代表点型 5-run board summary        | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/production_dispatch_fused_abcd_ilp/summary.md` | 当前 production default 的 std/RVV 性能主证据。                                                    |
| row-source 诊断触发日志 | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/run_board_bench_row_sources/analyze_bench_compare.log` | 记录 source-indexed 进入 production integration loop 的单次板卡诊断信号；不作为最终 production 性能结论。 |
| production-source-indices prior 5-run board summary | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/production_source_indices_staged_gather/summary.md` | source-indexed staged-gather / compressed-tail 的 Phase 031 前 repeated std/RVV 性能证据；当前作为 rollback baseline。 |
| production-source-indices block-fused probe 5-run board summary | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/production_source_indices_block_fused_abcd_ilp_probe/summary.md`；`evidence_doctor.md` | Phase 031 当前 source-indexed production direct probe 主证据；6 个代表 case median 正向，Doctor 为 `0E / 9W / 12S`。 |
| source-indexed-family repeated diagnostic | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/source_indexed_family_repeated/summary.md`；`evidence_doctor.md` | Phase 030 的 historical diagnostic / harness-risk signal；不替代 production direct。 |
| production-default trace / asm | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/production_default_fused_abcd_ilp/` | 当前默认 RVV path 的 trace、checksum 和符号级 asm 归因。 |
| evaluation 文档 | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/doc/transformation_estimation_point_to_plane_lls_weighted-evaluation.zh.md` | 候选取舍、历史 diagnostic A/B 和接入风险的主归属。 |
| optimization evidence 文档 | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/doc/optimization-evidence.zh.md` | 每种 RVV 优化方式到代码路径、target、bench / board 证据和边界的索引。 |
| topic README | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/README.zh.md` | 文档导航、常用命令和证据白名单入口。 |
| topic 测试文档 | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/doc/testing-overview.zh.md`；`doc/correctness-tests.zh.md`；`doc/benchmark-and-evidence.zh.md`；`doc/test-support-code-map.zh.md` | 测试类型、gtest 字典、bench/checksum/asm 口径和 test support 调用关系。 |
| QEMU gtest logs                                          | `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/qemu/run_test_std.log`；`test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/qemu/run_test_rvv.log`；`test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/qemu/run_test_source_indices_std.log`；`test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/qemu/run_test_source_indices_rvv.log` | `run_test_compare` 与 `run_test_source_indices_compare` 生成；QEMU std 45 passed + 1 skipped，RVV 47 passed，source-indexed 细粒度 std 6 passed、RVV 7 passed。 |

QEMU correctness（QEMU 正确性验证）用于构建、路径和日志形状，不作为性能结论。10-case board bench 只保留为 single-run board diagnostic signal（单次板卡诊断信号），不能升级成 repeated board performance conclusion（重复板卡性能结论）。

## 函数语义

该类用于 point-to-plane ICP（点到平面迭代最近点）中的 weighted LLS（带权线性最小二乘）变换估计。公开入口有四类：

| 公开入口                                                                                | 点对来源                                            | 权重来源                  | 当前 production RVV    |
| --------------------------------------------------------------------------------------- | --------------------------------------------------- | ------------------------- | ---------------------- |
| `estimateRigidTransformation(cloud_src, cloud_tgt, matrix)`                           | `source[k] + target[k]`                           | 成员`weights_[k]`       | full-cloud block-reduction RVV。 |
| `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, matrix)`              | `source[indices_src[k]] + target[k]`              | 成员`weights_[k]`       | source-indexed block-fused probe RVV；失败后 staged-gather / 标量。 |
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

source-indexed public overload 完成原有 index count、target 点数和 `weights_.size()` 检查后，在 `__RVV10__` 构建中尝试 `estimatePointToPlaneLLSWeightedSourceIndicesRVV`。该 helper 同样只在 `Scalar=float` 时继续；layout、规模、VLEN、byte-offset 或 source index 有效性检查失败时返回 `false`，公开入口随后进入原 iterator 标量 helper。

这两条分流不改变 public API（公开接口），不改变 dual-indices / correspondences overload，也不改变非 RVV 构建。

### Generic layout gate

生产 RVV 不再硬编码 `pcl::PointNormal` 的 offset 或 `sizeof`。source 和 target 分别验收：

| 侧     | Gate                                     | 证明内容                                                                                                                                  | 不证明内容                                                      |
| ------ | ---------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------- |
| source | `RVVXYZAoSFloatLayout<PointSource>`    | `x/y/z` 是 PCL traits 注册的单个 `float` 字段，POD/standard-layout、`sizeof(PointSource)` 和字段 offset 满足 AoS f32 跨步加载前提。 | 不证明 normal 字段，也不证明额外字段参与语义。                  |
| target | `RVVXYZNormalFloatLayout<PointTarget>` | `x/y/z/normal_x/normal_y/normal_z` 都是单个 `float` 字段，并满足 AoS f32 跨步加载前提。                                               | 不证明所有 normal 点型都逐类型上板，也不证明`Scalar=double`。 |

source 和 target 分别使用自己的 stride、POD layout 和字段 offset。当前 board representative pointtypes（代表性点类型）覆盖 `PointNormal -> PointNormal`、`PointXYZ -> PointNormal` 和 `PointXYZ -> PointXYZINormal`。其它 gate-allowed 点型只能继承 correctness 和 layout 证据，不能写成逐类型性能已证明。

### Contiguous `weights_`

production full-cloud 和 source-indexed 都使用成员 `weights_`，因此 weight 是连续 `std::vector<float>` 数据。RVV chunk 用 `vle32.v` 连续加载 `weights_[i..i+vl)`，并在计算公式前做：

```text
weighted normal = target normal * weight
```

这条语义不同于 correspondences 入口：correspondences 的权重来自每个 `pcl::Correspondence::weight`，需要先展开成局部 vector。该展开路径没有接 production。

### Source-indexed block-fused probe 与 staged-gather rollback

Phase 031 后，source-indexed production 先尝试 `block-fused-abcd-ilp` probe helper。这个 helper 保留 valid-index scan 和 `uint32_t` staging，然后把 source gather、target stride load、weight load 接到 A/B/C/N block groups。staged-gather / compressed-tail helper 保留为 rollback：如果 block-fused helper 因 layout、规模、VLEN、byte-offset 或 index gate 返回 `false`，source-indexed wrapper 再尝试 staged-gather；两者都失败时才回到原标量 iterator。

生产代码路径是：

```text
estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, matrix)
  -> detail::estimatePointToPlaneLLSWeightedSourceIndicesRVV(...)
  -> detail::buildPointToPlaneLLSWeightedSourceIndicesBlockFusedAbcdIlpRVV(...)
       -> source gather + target stride load + A/B/C/N block groups
     else detail::buildPointToPlaneLLSWeightedSourceIndicesStagedRVV(...)
       -> source gather + target stride load + vcompress + scalar tail
  -> detail::solvePointToPlaneLLSWeightedNormalEquation(...)
```

该路径先做 valid-index scan。每个 `indices_src[row]` 必须非负，并且必须小于 `cloud_src.size()`。检查通过后，helper 把 index staging 成 `std::vector<std::uint32_t>`。这样 RVV gather 只读取已经验收过的 32-bit byte offset，不在 gather 指令里处理非法索引。

单个 VL chunk 的 source-indexed block-fused probe 流程是：

```text
vsetvl_e32m1
  -> source index: vle32.v load uint32_t staging
  -> source byte offsets: index * sizeof(PointSource)
  -> source xyz: vluxei32.v gather
  -> target xyz + normal: vlse32.v stride load
  -> weight: vle32.v contiguous load
  -> finite mask: source/target xyz + target normal
  -> nx/ny/nz = target normal * weight
  -> a/b/c/d formula
  -> invalid lane merge to zero
  -> vfmacc.vv 更新当前 A/B/C/N 组的 partial sums
```

staged-gather rollback 的单个 VL chunk 仍使用 `vcompress` 后 scalar tail accumulation。source-indexed 标量路径每行通过 `ConstCloudIterator` 访问 source 和 target，再逐行累加 normal-equation。RVV 路径把 source 离散访问批量化，target 和 weight 保持连续 / 跨步加载。它没有改 solver，也没有改 weight finite 语义。非有限 point/normal 被 mask 掉；非有限 weight 仍参与计算。

这条路径的边界是 valid source index。invalid source index 会让 RVV helper 返回 `false`，公开入口回到原 iterator 标量边界。当前测试不把非法 index 的 defensive skip 写成 public API 合同。

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

### 代码路径与标量差异

生产源码中，full-cloud 公开入口先执行原有数量检查。检查通过后，RVV 构建才会尝试生产 RVV helper：

```text
estimateRigidTransformation(cloud_src, cloud_tgt, matrix)
  -> detail::estimatePointToPlaneLLSWeightedFullCloudRVV(...)
  -> detail::buildPointToPlaneLLSWeightedFullCloudBlockRVV(...)
  -> detail::loadPointToPlaneLLSWeightedFullReductionVectors(...)
  -> A/B/C/N block group accumulation
  -> detail::solvePointToPlaneLLSWeightedNormalEquation(...)
```

任何一步 gate 失败时，公开入口继续使用原标量路径：

```text
ConstCloudIterator<PointSource> source_it(cloud_src)
ConstCloudIterator<PointTarget> target_it(cloud_tgt)
weights_it = weights_.begin()
estimateRigidTransformation(source_it, target_it, weights_it, matrix)
```

这意味着当前 RVV 实现没有改变公开入口的对象状态，也没有改变 dual-indices 或 correspondences overload。full-cloud 路径用 block-reduction。source-indexed 路径用 valid-index scan + block-fused probe，staged-gather 作为 rollback。

| 阶段 | 标量实现 | full-cloud RVV | source-indexed RVV | 差异 | 对应测试 / 证据 |
| --- | --- | --- | --- | --- | --- |
| 输入数量检查 | public full/source/dual overload 检查 target、indices 和 `weights_` 数量。 | 保留原检查，检查失败时不尝试 RVV。 | 保留原检查，检查失败时不尝试 RVV。 | RVV 不接管错误输入路径。 | `run_test_input_semantics`。 |
| 入口选择 | full/source/dual/correspondences 都转成 `ConstCloudIterator`。 | 只在 full-cloud public overload 尝试 `estimatePointToPlaneLLSWeightedFullCloudRVV`。 | 只在 source-indexed public overload 尝试 `estimatePointToPlaneLLSWeightedSourceIndicesRVV`。 | full-cloud 和 source-indexed 的 RVV 入口分开。 | `run_test_public_semantics`、`run_test_source_indices`。 |
| 点型与 `Scalar` gate | 标量模板按字段访问，支持更宽点型和 `Scalar`。 | `canUsePointToPlaneLLSWeightedFullCloudRVV` 检查 source xyz f32 AoS、target xyz+normal f32 AoS、`Scalar=float`、VLEN 和 byte offset。 | `canUsePointToPlaneLLSWeightedSourceIndicesRVV` 额外检查 index count 和 source index 有效性。 | source-indexed 多了 index gate。 | `run_test_production_direct`、`run_test_source_indices`。 |
| row load | `ConstCloudIterator` 每次读一个 source、target 和 weight。 | 每个 VL chunk 用 `vlse32.v` 读 source/target AoS 字段，用 `vle32.v` 读连续 weights。 | 每个 VL chunk 先 `vle32.v` 读 staging index，再 `vluxei32.v` gather source，target 用 `vlse32.v`，weight 用 `vle32.v`。 | full-cloud 以 stride load 为主；source-indexed 额外有 gather。 | asm attribution、production-dispatch bench、source-indexed bench。 |
| finite mask | 标量 `if` 检查 source xyz、target xyz 和 target normal，失败则 `continue`。 | `loadPointToPlaneLLSWeightedFullReductionVectors` 构造 vector mask，invalid lane merge to zero。 | block-fused probe 的 `loadPointToPlaneLLSWeightedSourceIndexedBlockVectors` 构造 vector mask，invalid lane merge to zero；rollback helper 仍用 staged-gather mask。 | mask 条件相同；weight 仍不参与 mask。 | 非有限 point/normal 和非有限 weight tests。 |
| `a/b/c/d` 公式 | 标量逐点计算，按 double 累加到 `ATA/ATb`。 | chunk 内用加权 normal 生成 `a/b/c/d` 向量；当前公式块为 fused-abcd-ilp code shape。 | chunk 内用加权 normal 生成 `a/b/c/d` 向量。 | source-indexed 复用同一逐点公式。 | `run_test_candidates`、`run_bench_generic_fused_formula`。 |
| 行尾处理 | 每个有效点直接更新 normal-equation。 | `buildPointToPlaneLLSWeightedFullCloudBlockRVV` 按 A/B/C/N 四组维护 vector partial sums，组结束后 `vfredosum` 到 double normal-equation。 | `buildPointToPlaneLLSWeightedSourceIndicesBlockFusedAbcdIlpRVV` 按 A/B/C/N 四组维护 vector partial sums；staged rollback 用 `vcompress` 后 scalar tail 累加压缩行。 | source-indexed 当前优先 block-fused probe，保留 compressed tail rollback。 | `run_test_candidates`、`run_test_source_indices`、production probe summary。 |
| solve 和 matrix | Eigen 6x6 solve，构造 4x4 matrix。 | 复用同一 solve 和 matrix 构造。 | 复用同一 solve 和 matrix 构造。 | 当前 RVV 不优化 solver。 | public matrix tests、production direct tests。 |

采用 block-reduction 的代码差异集中在 normal-equation 构造阶段。早期 staged-row diagnostic 会把有效 lane 写回固定 buffer，再由标量 tail 累加。当前 production helper 去掉 staged row buffer，在 A/B/C/N 组内直接累加向量 partial sums。这个差异对应 `block-reduction` adopted 状态。

采用 generic point type gate 的代码差异集中在 load 地址计算阶段。production helper 通过 PCL RVV layout traits 分别读取 source 和 target 的 offset、stride 和字段类型，不依赖 `PointNormal` 的固定 offset。这个差异对应 `generic point type gate` adopted 状态。

采用 fused formula 的代码差异集中在 `loadPointToPlaneLLSWeightedFullReductionVectors` 的 `a/b/c/d` 公式块。它保留 block group 和 finite mask，只改变 chunk 内公式树。这个差异对应 `fused formula` adopted / accepted-risk 状态。

### fused formula follow-up A/B 结果

unweighted TEPTPL 已经采用 fused formula（融合公式）生产路径：它保留 A/B/C/N block-reduction 组织，把逐点 `a/b/c` 改成 `vfmsac` 形态，并把 `d` 改成 `nx*(dx-sx) + ny*(dy-sy) + nz*(dz-sz)` 后用 `vfmacc` 累加。weighted 路径需要独立验证。weighted 公式多了 `weight * normal`，并且非有限 weight 语义必须保持可观察；如果把 `d` 的六项和、`d` 的 displacement 形态或 `a/b/c` 改成 FMA contraction（融合乘加收缩），需要重新证明 float 中间舍入、near-cancellation（近似抵消）、`accepted_points`、`ATA/ATb` 和 matrix 预算仍成立。

当前 production 仍保留 block-reduction、A/B/C/N partial sums、layout gate、finite mask 和 scalar fallback；变化点是逐点 `a/b/c/d` formula tree（公式树）采用 fused-abcd-ilp 形态。它直接写入 production helper，不再额外保留一套 production fused helper。

本节使用三种对比口径，不能混读：

| 口径 | A 侧 | B 侧 | 指标 | 能证明什么 |
| --- | --- | --- | --- | --- |
| direct diagnostic A/B | test-rvv block-reduction helper | test-rvv fused helper | `B/A = A_rvv_ms / B_rvv_ms`，`>1` 表示 B 更快 | 只证明测试专用 helper 之间的相对形状。 |
| production-shaped A/B | test-rvv layout-gated block helper | test-rvv fused helper | `B/A = A_rvv_ms / B_rvv_ms`，`>1` 表示 B 更快 | 同边界比较 full estimate；这是接入前证据，不能代替接入后的默认 trace。 |
| std/RVV speedup | 同一 case 的 std 构建 | 同一 case 的 RVV 构建 | `std_ms / rvv_ms` | 只说明该 case 自身 RVV 形状，不等于 fused 相对 block baseline 的收益。 |

本轮在 `include/impl/teptplw_reductions.hpp` 和 `include/impl/teptplw_candidates.hpp` 中加入四类 testing-only（仅测试使用）diagnostic/component candidates，并和同边界 block baseline、标量 reference 对拍：

| 候选 | 公式树变化 | QEMU correctness / numeric | 板卡 A/B 结论 |
| --- | --- | --- | --- |
| `abc-fused` / `abc-fused-ilp` | `a/b/c` 使用 unweighted 同款 `vfmsac` 形态，`d` 保持当前六项展开；ILP 版只重排源码顺序。 | QEMU 47 tests 覆盖 same-chain、near-cancellation、scale-stress、非有限 point/normal、非有限 weight、输入语义、source-indexed production、`accepted_points`、`ATA/ATb`、matrix 和三类代表点型。 | 当前二进制中 `abc` 与 `abc-ilp` RVV 指令序列相同；只保留诊断。 |
| `d-six-term-fma` / `d-six-term-fma-ilp` | 保留 `nx*dx + ny*dy + nz*dz - nx*sx - ny*sy - nz*sz` 六项形态，但用 FMA 做累加；ILP 版交错 target/source accumulator。 | 同上，并补三类代表点型 correctness。 | generic warm-up 5-run 中 `PointNormal` component no-solve 不稳；ILP 版 asm 与非 ILP 等价。 |
| `d-displacement-fused` / `d-displacement-fused-ilp` | 使用 unweighted 同款 `dx-sx`、`dy-sy`、`dz-sz` 后 `vfmacc` 累加；ILP 版交错独立差值和 abc 项。 | 同上。 | 两个 `PointXYZ` 代表组合强，但 `PointNormal` component no-solve 不稳；ILP 版 asm 与非 ILP 等价。 |
| `abcd-fused` / `abcd-fused-ilp` | 同时采用 `abc-fused` 和 `d-displacement-fused`。 | 同上，并补 generic formula warm-up 5-run、production-symbol asm 和默认 production correctness。 | 接入前最稳；按“RVV 优于 std、静态实现质量更高”为主要标准，默认 production 采用 `abcd-fused-ilp`，并接受已记录的运行态风险。 |

`abc-fused` 的“减少公式指令”只相对接入前非 fused block baseline 而言：baseline 的 `a/b/c` 每项通常是两个 `vfmul` 加一个 `vfsub`，fused 形态每项是一个 `vfmul` 加一个 `vfmsac`。`abc-fused` 和 `abc-fused-ilp` 彼此的 intrinsic 数相同，都是三条 seed multiply 加三条 `vfmsac`；ILP 版只改变源码顺序，只有反汇编显示 hot path 确实不同，才能把它视为独立机器码候选。

新增 tests 明确检查非有限 weight 语义、near-cancellation、scale-stress、`accepted_points`、`ATA/ATb` 和 matrix。非有限 point/normal 仍由 finite mask 排除；非有限 weight 不改变 `accepted_points`，但会继续影响 normal-equation，这一点已经和标量 reference 对齐。

新增 bench case 分两层：component no-solve 只测 normal-equation 构造成本，full estimate 包含 solve 和 matrix 构造。当前 case 走 diagnostic direct helper，不走真实 production dispatch：

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

板卡证据分为两类。第一类是接入前的 direct diagnostic、production-shaped、generic representative 和 production-symbol 多轮 A/B；这些 raw log 不作为本次提交内容，关键结论保留在本节和 topic evaluation 文档中。第二类是接入后的默认 production trace：通过 topic-local `collect_teptplw_board_rvv_ba.py` 采集，使用 `summarize_teptplw_trace.py` 汇总，并单独用 `generate_teptplw_asm_attribution.py` 做默认 helper 归因。接入后的默认路径没有旧 block/fused pair，因此 `production-default-fused-abcd-ilp` 不再产生成对 B/A 表。QEMU timing 不作为性能结论。

fused follow-up 的接入结论是：

```text
production-adopted/fused-abcd-ilp-default-accepted-risk
```

它先改变 full-cloud、`Scalar=float`、满足 layout/size/VLEN gate 的 weighted production implementation。Phase 031 又在 source-indexed public overload 上尝试同 family 的 block-fused production probe，并保留 staged-gather rollback。dual-indices 和 correspondences 仍不进入 production RVV。

## 范围决策表

| 方向                                         | 状态               | 证据 / 理由                                                                                                                      |
| -------------------------------------------- | ------------------ | -------------------------------------------------------------------------------------------------------------------------------- |
| generic point type gate                      | adopted            | source/target 分别用 PCL RVV layout traits、offset 和 stride；三类代表点型有 production direct tests 和 board 5-run。            |
| block-reduction                              | adopted            | production-default asm attribution（带边界列的反汇编归因）、representative production-dispatch 5-run board summary 和 QEMU 47 tests 均闭合。 |
| representative pointtypes evidence           | adopted            | 板卡覆盖`PointNormal -> PointNormal`、`PointXYZ -> PointNormal`、`PointXYZ -> PointXYZINormal`；文档明确不外推逐类型性能。 |
| fused formula                                | adopted / accepted-risk | correctness、generic representative pointtypes、production-symbol asm 和接入前多轮板卡数据已闭合静态实现证据；旧 20-run 中 `PointNormal -> PointNormal` avg B/A median 为 `0.976x`、低于 `1.0x` 为 `12/20`，按人工接入标准接受该运行态风险。 |
| source-indexed production                    | bounded production candidate | Phase 031 production direct、fallback、benchmark 和 repeated board probe 已正向；clean adopted 仍缺 source-indexed-specific asm、binary identity、taskset metadata 和可选 extended-run。 |
| dual-indices production                      | deferred           | 有 public semantics、row-source correctness、`run_bench_row_sources` 和 `collect_board_row_sources_repeated` 入口；缺 production direct/fallback 和符号归属。 |
| correspondences production                   | deferred           | 有 public semantics、row-source correctness、`run_bench_row_sources` 和 `collect_board_row_sources_repeated` 入口；缺 index/weight 展开消融、production direct/fallback 和符号归属。 |
| `Scalar=double`                            | deferred           | 当前 RVV 公式和 layout gate 只批准`Scalar=float`，double 已由 fallback test 保护。                                             |
| 把 diagnostic 10-case 当 production evidence | rejected / not_now | 10-case board 是 single-run diagnostic signal；真实 production 性能只看 production-dispatch representative 5-run。               |

## RowSourcePolicy 与 WeightPolicy 边界

test-rvv 仍保留四类 row source diagnostic，用于说明每条公开入口的数据流差异。`run_bench_default_diagnostic` 是综合诊断入口，包含 full-cloud、block/fused 和 row-source candidate。复核数据源取舍时使用 `run_bench_row_sources`，必要时再接 `collect_board_row_sources_repeated`。这些入口主要覆盖 full-cloud、dual-indices 和 correspondences candidate；source-indexed 已经进入 production direct probe，不再用 diagnostic wrapper 直接决定当前生产表现。

| 入口形态        | RowSourcePolicy 取点                                  | WeightPolicy 取权重                               | Bench 计时边界                                                                                                   | 当前结论                                 |
| --------------- | ----------------------------------------------------- | ------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------- | ---------------------------------------- |
| full-cloud      | `source[k] + target[k]`，两侧 stride load。         | `weights_[k]` 连续 load。                       | 输入和权重计时前构造；estimate 内 normal-equation、solve、matrix 计时。                                          | production RVV 接入只覆盖这一条的 block-reduction。 |
| source-indexed  | `source[indices_src[k]] + target[k]`。              | `weights_[k]` 连续 load。                       | `pcl::Indices` 输入计时前构造；production 内 valid-index scan、`uint32_t` staging、gather、A/B/C/N block groups、solve 和 matrix 计时；staged rollback 仍含 `vcompress`。 | bounded production candidate。          |
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

该点对 `ATb` 的贡献是 `[0.1, -0.05, 0, 0, 0, 0.05]`，对 `ATA` 的贡献进入 6x6 上三角。RVV block-reduction 计算同一组行项，但把多个 lane 的同类项先放在向量 partial sums 中，再通过 `vfredosum` 写入 normal-equation。因此它和标量 row-order double 累加允许 bitwise 差异，测试使用 `accepted_points`、`ATA/ATb` 和 matrix 的误差预算。

如果一个 VL chunk 有 4 个 lane，lane 2 的 target normal 为 NaN，finite mask 是 `1,1,0,1`。RVV 会把 lane 2 的 `a/b/c/d/nx/ny/nz` 合并为零，A 组 `vcpop` 只统计 3 个 accepted points。weight 即使非有限也不会改变 mask，这是 weighted 标量合同的一部分。

## Bench 与性能证据

QEMU 10-case diagnostic bench 可构建、可解析，checksum 基本对齐；QEMU timing 不作为性能结论。10-case board bench 覆盖 full/current、full/block、source、dual、correspondences 的 64K/256K，但只是一轮 single-run diagnostic signal。source-indexed 的正向信号已经进入 production integration loop，并补了 dedicated production bench 和 repeated board summary。

当前生产性能结论来自真实 public overload 的 production-dispatch repeated 5-run board summary。该组使用 262144 点、20 iterations 和 5 warm-up iterations：

| case                            | runs |   256K median/min | 边界                                                           |
| ------------------------------- | ---: | ----------------: | -------------------------------------------------------------- |
| `pointnormal`                 |    5 | `2.76x / 2.73x` | `PointNormal -> PointNormal`；5 轮均正向。 |
| `pointxyz-to-pointnormal`     |    5 | `2.98x / 2.95x` | generic source xyz f32 AoS representative。                    |
| `pointxyz-to-pointxyzinormal` |    5 | `3.00x / 2.96x` | generic source + target xyz/normal f32 AoS representative。    |

这些 case 的 std/RVV 两侧都调用真实 full-cloud public overload，并都通过 `setCorrespondenceWeights(weights)` 使用连续 `weights_`。它们不覆盖 source-indexed、dual-indices、correspondences、`Scalar=double`、非连续权重或所有 gate-allowed 点型。source-indexed 使用下一张表。

source-indexed current production probe performance 来自 `production_source_indices_block_fused_abcd_ilp_probe/summary.md`。该组使用 65536 和 262144 点、5 runs、20 iterations 和 5 warm-up iterations：

| case | runs | 262144 median/min | 65536 median/min | 边界 |
| --- | ---: | ---: | ---: | --- |
| `pointnormal` | 5 | `1.64x / 1.17x` | `1.71x / 1.66x` | `PointNormal -> PointNormal` source gather；262144 下有长尾 warning。 |
| `pointxyz-to-pointnormal` | 5 | `1.54x / 1.06x` | `1.64x / 1.59x` | source generic xyz f32 AoS representative；262144 下有长尾 warning。 |
| `pointxyz-to-pointxyzinormal` | 5 | `1.69x / 1.41x` | `1.69x / 1.68x` | source + target generic layout representative；262144 下有长尾 warning。 |

这些 case 的 std/RVV 两侧都调用真实 source-indexed public overload。它们不覆盖 dual-indices、correspondences、invalid source index public API 行为、`Scalar=double` 或非连续权重。旧 staged-gather summary `production_source_indices_staged_gather/summary.md` 保留为 Phase 031 前 rollback baseline：三类代表点型 262144 median/min 分别为 `2.33x / 1.99x`、`2.22x / 2.18x`、`2.37x / 2.28x`。

fused formula follow-up 另有接入前 PointNormal direct diagnostic 和 production-shaped 5-run A/B summary。它们的用途是筛选逐点公式树 fused contraction，并解释为什么需要补 generic representative、production-symbol 和接入后默认 trace。这里的 B/A 固定表示 `A_rvv_ms / B_rvv_ms`，`>1` 才表示 fused candidate 比同边界 block baseline 更快；direct diagnostic 的 std/RVV speedup 不能替代 fused-vs-block B/A：

| candidate | direct 64K B/A median/min | direct 256K B/A median/min | production-shaped 64K B/A median/min | production-shaped 256K B/A median/min | 结论 |
| --- | ---: | ---: | ---: | ---: | --- |
| `abc-fused` | `1.08x / 0.99x` | `0.96x / 0.88x` | `1.03x / 1.02x` | `1.11x / 1.09x` | PointNormal production-shaped 正向；generic 代表点型复核见下表，仍不闭合。 |
| `d-six-term-fma` | `1.14x / 1.03x` | `0.98x / 0.65x` | `1.06x / 1.05x` | `1.13x / 0.80x` | 256K 有明显退化 run，不接 production。 |
| `d-displacement-fused` | `1.12x / 1.00x` | `0.81x / 0.61x` | `1.05x / 1.05x` | `1.10x / 0.72x` | 256K 有明显退化 run，不接 production。 |
| `abcd-fused` | `1.11x / 0.78x` | `0.92x / 0.60x` | `1.05x / 1.05x` | `1.13x / 1.09x` | PointNormal production-shaped 正向；该历史表只支持继续复核，后续已补代表点型和 production-symbol 证据。 |

generic `abc-fused` representative 5-run B/A 进一步覆盖三类代表点型。这里的 A 是同一 test_support layout-gated 边界下的 block baseline，B 是 test-rvv generic fused helper；早期 mixed-boundary cross-check 只作为历史排查线索，不作为严格 A/B。重点行如下：

| candidate / layer | `pointnormal` 64K / 256K median-min | `pointxyz-to-pointnormal` 64K / 256K median-min | `pointxyz-to-pointxyzinormal` 64K / 256K median-min | 结论 |
| --- | ---: | ---: | ---: | --- |
| `abc-fused` component no-solve | `1.01x / 1.00x`、`0.96x / 0.89x` | `1.02x / 1.02x`、`1.33x / 0.73x` | `1.02x / 1.01x`、`1.02x / 0.40x` | component 层有 256K 低谷。 |
| `abc-fused` production-shaped full | `1.03x / 1.03x`、`1.05x / 1.05x` | `1.04x / 1.04x`、`1.05x / 1.04x` | `1.04x / 1.03x`、`0.85x / 0.63x` | generic target 256K 负向，不接 production。 |
| `abc-fused-ilp` production-shaped full | `1.03x / 1.03x`、`1.05x / 0.96x` | `1.04x / 1.04x`、`1.04x / 0.76x` | `1.04x / 1.03x`、`1.22x / 1.05x` | 有正向项；asm 与非 ILP 公式形状相同。 |

## 正确性与高效性证据链

| 链路                        | 证据                                                                                                                                                                       | 结论                                                                                                                                                                                                | 边界                                                                                       |
| --------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------ |
| correctness（正确性）       | QEMU `run_test_compare`：`run_test_std.log` 为 45 passed + 1 skipped，`run_test_rvv.log` 为 47 passed；`run_test_source_indices_compare` 的 std 为 6 passed、RVV 为 7 passed。 | public full-cloud direct、public source-indexed direct、production helper normal-equation、near-cancellation、scale-stress、非有限 point/normal、非有限 weight、输入数量不匹配、0/负权重、`accepted_points`、`ATA/ATb`、matrix、fallback 和 generic fused representative correctness 均在预算内。 | 不覆盖非法 index public API 合同、所有 correspondence 分布、`Scalar=double` RVV 或未上板点型性能。 |
| path / asm（路径 / 反汇编） | `production-default-fused-abcd-ilp` asm attribution 按 `boundary` 记录实际归因边界；`detail` 或 `estimate-rvv-helper` 边界内确认 `vlse32.v`、`vle32.v`、`vfmsac.vv`、`vfmacc.vv` A/B/C/N partial sums、`vcpop/vmerge` 和显式 `vfredosum.vs`。 | full-cloud production RVV 指令归属于当前默认 helper；`PointNormal -> PointNormal` 行落在 `estimate-rvv-helper`，RVV 指令数为 `357`，另外两类代表点型落在 `detail`，RVV 指令数为 `338`；三行均 `vfadd=0`、`vfmsac=6`，且无 vector spill/reload。source-indexed 路径命中由 `run_test_source_indices` 的 `used_rvv` 和 repeated board speedup 证明。 | source-indexed 尚无单独 asm attribution summary；dual/correspondences 不在 production RVV 范围；不同 `boundary` 的总指令数不能直接横向比较。 |
| performance（性能）         | `production_dispatch_fused_abcd_ilp/summary.md`、`production_source_indices_block_fused_abcd_ilp_probe/summary.md`、`production_source_indices_staged_gather/summary.md` 和 `production_default_fused_abcd_ilp/trace_summary.md`。 | full-cloud 三类代表点型在 262144 点上 repeated std/RVV speedup 均正向；source-indexed block-fused probe 三类代表点型在 65536 和 262144 点上 repeated std/RVV speedup 均正向；staged-gather summary 保留 rollback baseline；默认 RVV trace checksum 序列一致。 | 10-case diagnostic 不替代 production evidence；QEMU timing 不作性能结论；source-indexed probe 还不是 clean adopted。 |
| boundary（边界）            | EvidenceDecision、覆盖范围表和 fallback 矩阵共同记录生产边界。 | 结论覆盖 full-cloud 和 source-indexed、`Scalar=float`、contiguous `weights_`、source xyz f32 AoS、target xyz/normal f32 AoS。 | 未逐类型上板的 gate-allowed 点型没有逐类型性能证明；dual/correspondences 保持标量。 |

## Fallback 矩阵

| 条件                                                           | RVV 行为                   | 标量语义                                                       |
| -------------------------------------------------------------- | -------------------------- | -------------------------------------------------------------- |
| 非`__RVV10__` 构建                                           | 没有 RVV 尝试。            | 原 full-cloud overload 构造 iterator 并进入 protected helper。 |
| `Scalar != float`                                      | RVV helper 返回 false。    | 原模板标量路径；`Scalar=double` 有 fallback test。           |
| `nr_points < 64`                                             | 不进入 block-reduction。   | 小规模保留标量，避免 dispatch 成本。                           |
| `cloud_tgt.size()` 或 `weights_.size()` 不等于 source size | 公开入口原错误路径。       | 保持原错误处理。                                               |
| source 或 target layout gate 失败                              | 不读取 RVV 字段 offset。   | 原标量模板路径；double-normal target fallback 已覆盖。         |
| `vlmax_e32m1() > 64`                                         | 固定 block helper 不授权。 | 标量。                                                         |
| source/target 元素数超过 32-bit byte offset gate               | 不进入 RVV。               | 标量。                                                         |
| source-indexed invalid index gate miss                         | 不进入 block-fused 或 staged-gather RVV。 | 原 iterator 标量路径。                                         |
| dual-indices、correspondences                                  | 无 production RVV 分流。   | 原 iterator / correspondence weight 路径。                     |

## 遗留风险与后续条件

- 更多 gate-allowed 点型：当前 generic gate 允许更多 f32 AoS 点型，但板卡只覆盖三类代表组合。新增点型性能结论需要对应 production direct、asm 和 board 抽样。
- fused formula：当前 production 保留 block-reduction partial-sum `vfmacc`，逐点 `a/b/c/d` 采用 fused-abcd-ilp 公式块。`abc`、D 项和 `abcd` 已在 generic layout-gated test_support 中完成消融；接入后 production default asm 已确认公式收缩进入默认 helper，且无 vector spill/reload。接入前 20-run 的 `PointNormal` 运行态波动作为 accepted risk 保留。
- dual/correspondences：不应直接接 production。下一步先运行 `run_bench_row_sources` 或对应板卡诊断；若某条 row source 稳定正向，再补 production helper、public dispatch、fallback tests、production asm 和 repeated board。
- 负向归因：dual/correspondences 单次板卡负向说明当前诊断路径不适合直接接生产，但不能证明 gather、buffer 或 weight 展开是唯一主因。
