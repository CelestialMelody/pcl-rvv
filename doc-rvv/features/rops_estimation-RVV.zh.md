# ROPS RVV 优化说明

## 当前状态

`features/include/pcl/features/impl/rops_estimation.hpp` 当前保留一份已采纳的有界 RVV（RISC-V Vector，可伸缩向量）production patch（生产补丁）。补丁不改变公开 API（应用程序接口）。`ROPSEstimation::rotateCloud()` 和 `ROPSEstimation::getDistributionMatrix()` 在 `__RVV10__` 构建、dense cloud、足够规模、有效 bins / projection / AABB extent，并且 `PointInT` 满足 `pcl::rvv::RVVXYZAoSFloatLayout` 的 xyz float AoS（结构数组）布局时进入 RVV helper；其它情况继续走原标量路径。

采纳依据是接入后的 production-detail（生产私有 helper 直连）板卡 repeated benchmark（重复板卡性能测试）。`PointXYZ`、`PointXYZI` 和 `PointNormal` 三组代表点型均为正向，checksum 一致，Evidence Doctor（证据体检）没有 Error 或 Warning。

## 函数语义与标量路径

公开入口 `pcl::Feature::compute()` 会调用 `ROPSEstimation::computeFeature()`。每个 keypoint 的 RoPS（Rotational Projection Statistics，旋转投影统计）描述子生成流程如下：

1. `buildListOfPointsTriangles()` 为输入 surface 建立点到三角面的反向索引。
2. `getLocalSurface()` 通过 KdTree radius search（半径搜索）取得 keypoint 周围局部点和关联三角面。
3. `computeLRF()` 基于局部三角面计算 LRF（Local Reference Frame，局部参考坐标系），其中包含 scatter matrix 和 Eigen eigenvectors。
4. `transformCloud()` 把局部点平移到 keypoint 原点并乘以 LRF。
5. `rotateCloud()` 对 transformed local cloud 按 x/y/z 三个轴和多次角度旋转，并计算 rotated cloud 的 AABB（轴对齐包围盒）。
6. `getDistributionMatrix()` 对 XY、XZ、YZ 三个 projection（投影）生成 2D distribution matrix（分布矩阵）。
7. `computeCentralMoments()` 从每个 distribution matrix 生成 4 个中心矩和 1 个 entropy（熵）。
8. `computeFeature()` 拼接 135 维 histogram，并按 L1 norm（绝对值和）归一化。

RVV 当前只接管第 5 步的 xyz 旋转和 AABB min/max reduction（最小 / 最大规约），以及第 6 步的 row/col bin index staging（行列 bin 索引暂存）。KdTree search、LRF、matrix scatter 累加、central moments 和 descriptor normalization 仍保持标量。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| dispatch 与 fallback | adopted | 生产私有 helper 在 `__RVV10__` 下先尝试 RVV；helper 返回 `false` 时自然执行原标量主体 | `run_test_compare` 覆盖 helper trace、小规模、production public non-dense 和非 f32 layout fallback；RVV helper 直接拒绝无效 bins / projection / extent | 非 RVV 构建、小规模、non-dense、layout 不满足时回退标量；production 调用方只传有效 projection，setter 拒绝 0 bins |
| 点类型 gate | adopted | `RVVXYZAoSFloatLayout<PointInT>` 使用 PCL traits（点类型字段特征）和 AoS layout 前提证明 xyz 可按 float stride load/store 访问 | `PointXYZI` 与 `PointNormal` typed correctness 和 board repeated 均为正向 | 不逐个声明所有自定义点型性能；额外字段不参与当前 RVV stage |
| `rotateCloudRVV()` | adopted | 每个 VL chunk（可变向量长度分块）跨步加载 xyz，执行 3x3 rotation matrix（旋转矩阵）乘法，写回 rotated cloud，并用向量规约计算 AABB | Phase 020 diagnostic、Phase 040/050 production-detail summary、asm | axis 的 sin/cos 预计算仍为标量；输入标记为 non-dense 时回退 |
| `getDistributionMatrixRVV()` | adopted | RVV 批量计算 projection 坐标到 row/col bin index，并将 row/col staging 写入临时数组；bin scatter 累加保持标量 | Phase 010 diagnostic、Phase 030 combined、Phase 040/050 production-detail summary、asm | scatter 冲突、顺序语义和矩阵更新不做 RVV |
| central moments | attempted | correctness scaffold 成立，但默认 5x5 matrix 太小，且 entropy `std::log` 保持标量 | Phase 000 QEMU correctness 和 asm | 不单独接 production |
| descriptor normalization | not_now | 135 个 float 的 reduction + scale 规模固定，当前没有 public profile 证明它是主成本 | roadmap | 只有 public workload/profile 指向该尾段时再开 phase |

## VL chunk 示例

一个 `rotateCloudRVV()` chunk 的输入是 `cloud.points[i ... i + vl)`。helper 从每个点的 `x/y/z` offset 做三路 stride load，得到 `vx/vy/vz`。随后按标量路径同一组旋转矩阵系数计算：

```text
rx = r00 * x + r01 * y + r02 * z
ry = r10 * x + r11 * y + r12 * z
rz = r20 * x + r21 * y + r22 * z
```

同一 chunk 先完成所有读取，再写回 rotated cloud 的 `x/y/z`，避免输入输出重叠时同一 chunk 内读取被写回污染。AABB 使用向量 `vfmin/vfmax` 和最终 `vfredmin/vfredmax` 归约到 `min/max`。

`getDistributionMatrixRVV()` 的一个 chunk 会把 selected projection 的两个坐标转换成 bin 坐标。它用向零转换得到 row/col，再对最大 bin 做 clamp（边界夹取）。后续 `matrix(row, col) += 1.0f` 保持标量，因为多个 lane（向量通道）可能写同一个 bin。

## 范围决策表

| 范围 | 当前状态 | 证据 | 说明 |
| --- | --- | --- | --- |
| `PointXYZ` / float / xyz AoS / production private helper | adopted | `phase040_production_direct_repeated/summary.md`：median `1.700x`，min `1.690x`，0/5 退化 | 当前生产路径保留 |
| `PointXYZI` / float / xyz AoS / production private helper | adopted representative scope | `phase050_pointxyzi_production_detail_repeated/summary.md`：median `1.520x`，min `1.470x`，0/5 退化 | 额外字段不参与当前 RVV stage |
| `PointNormal` / float / xyz AoS / production private helper | adopted representative scope | `phase050_pointnormal_production_detail_repeated/summary.md`：median `1.590x`，min `1.570x`，0/5 退化 | normal 字段不参与当前 RVV stage |
| 其它满足 `RVVXYZAoSFloatLayout<PointInT>` 的点型 | adopted by traits gate, performance not individually enumerated | traits gate + 代表点型证据 | 若用户需要特定自定义点型性能，需要另开 point-type evidence phase |
| 完整 public `computeFeature()` | deferred | 当前 summary 只覆盖 production private helper detail path | 需要真实 mesh workload 和 profile |
| `Scalar=double` | scalar-only | 当前生产 helper 使用 float xyz 和 `Eigen::MatrixXf` | 不进入 RVV path |
| non-dense 或小规模输入 | scalar fallback | correctness fallback tests；public `compute()` non-dense surface 回归测试 | 小规模阈值为 16 个点；`transformCloud()` 保留 surface dense 标志，使公开链路不会误入 RVV |
| 非 f32 xyz AoS layout | scalar fallback | `RopsXYZDouble` layout mismatch 测试 | 已注册但 xyz 为 double 的点型不会命中 RVV helper |
| 无效 bins / projection / AABB extent | RVV helper reject；production caller 保持有效输入 | helper gate correctness tests | 避免除零、非法 projection 和 degenerate extent；这些 private helper 非法调用不作为 public scalar fallback 证据 |

## Fallback 矩阵

| 条件 | 路径 | 语义 |
| --- | --- | --- |
| 未定义 `__RVV10__` | 原标量主体 | 生产源码中 RVV helper 不编译 |
| `RVVXYZAoSFloatLayout<PointInT>::value == false` | helper 返回 `false`，执行标量主体 | 非 xyz float AoS 点型保持原模板语义 |
| `cloud.size() < 16` | 标量主体 | 避免小输入的 RVV 分流开销 |
| `cloud.is_dense == false` | 标量主体 | 保留原路径对 non-dense 输入的行为 |
| `number_of_bins == 0` 或 `projection >= 3` | 标量主体 | RVV helper 不处理非法配置 |
| projection 方向上的 AABB extent 接近 0 | 标量主体 | 避免 bin 长度除零或不稳定转换 |

## Bench 与证据

性能结论只引用板卡或目标硬件结果。QEMU（仿真器）只用于 correctness（正确性）、路径命中和日志形状。

| 证据 | 命令 / 路径 | 结果 | 证明范围 |
| --- | --- | --- | --- |
| correctness compare | `make -C test-rvv/features/rops_estimation run_test_compare` | Std `7/7`，RVV `16/16` | test-only components、production helper trace、typed scope、public non-dense 和 helper gate fallback |
| asm attribution | `make -C test-rvv/features/rops_estimation dump_bench_rvv` | bench asm 中出现 `vlse32`、`vsse32`、`vfmacc`、`vfredmin`、`vfredmax`、`vfcvt`、`vminu`、`vmerge`、`vse32` | RVV 指令可归属到 ROPS production helper 实例 |
| `PointXYZ` board summary | `test-rvv/features/rops_estimation/log/board/phase040_production_direct_repeated/summary.md` | 5-run median `1.700x`，min `1.690x`，max `1.700x`，0/5 退化，checksum match | production private helper detail path |
| `PointXYZI` board summary | `test-rvv/features/rops_estimation/log/board/phase050_pointxyzi_production_detail_repeated/summary.md` | 5-run median `1.520x`，min `1.470x`，max `1.570x`，0/5 退化，checksum match | representative typed production detail path |
| `PointNormal` board summary | `test-rvv/features/rops_estimation/log/board/phase050_pointnormal_production_detail_repeated/summary.md` | 5-run median `1.590x`，min `1.570x`，max `1.610x`，0/5 退化，checksum match | representative typed production detail path |
| Evidence Doctor | `doctor_phase040_board_summary`、`doctor_phase050_pointxyzi_board_summary`、`doctor_phase050_pointnormal_board_summary` | 三组均 `0 Error / 0 Warning / 2 Suggestion` | 没有 checksum、A/B 边界或方向性 Error；建议补环境 metadata 和 binary hash |

## 正确性与高效性证据链

correctness：Std/RVV compare 覆盖 central moments、distribution matrix、rotateCloud、combined pipeline、production helper trace、小规模 fallback、helper-level invalid input rejection、非 f32 layout fallback、public `compute()` non-dense surface fallback、`PointXYZI` 和 `PointNormal` typed scope。RVV path 命中时，rotated xyz、AABB 和 distribution matrices 与标量参考一致。

path / asm：反汇编显示 production helper 实例包含 RVV load/store、FMA、min/max reduction、float-to-uint conversion、clamp 和 row/col staging 指令。QEMU 只证明构建、执行路径和日志形状，不参与性能判断。

performance：接入后的板卡 repeated summary 是当前性能结论来源。`PointXYZ` median `1.700x`，`PointXYZI` median `1.520x`，`PointNormal` median `1.590x`，三组均没有 B/A < 1 且 checksum 一致。

boundary：当前 EvidenceDecision 只覆盖 `rotateCloud()` + `getDistributionMatrix()` 的 production private helper detail path，以及满足 xyz float AoS gate 的 dense 输入。完整 `computeFeature()` public path、真实 mesh local surface、LRF、central moments、descriptor normalization、自定义点型逐个性能和 `Scalar=double` 未被这些性能数据证明。

risk：Evidence Doctor 建议补 taskset、governor、freq、temperature 和 binary hash。当前 5-run 数据没有方向反转、checksum 错误或退化，因此这些建议不阻塞采纳；如果后续出现长尾或方向反转，应先补齐这些 metadata 后再判断。

## Traceability Map（可追踪性地图）

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `ROPSEstimation::computeFeature()` | production public entry | 完整 RoPS descriptor 生成 | `pcl::Feature::compute()` | local surface、LRF、rotation、distribution matrix、moments、normalization | public boundary；当前性能不外推到完整 public path | `features/include/pcl/features/impl/rops_estimation.hpp` |
| `ROPSEstimation::rotateCloud()` | production dispatch / fallback | 尝试 RVV rotate + AABB，失败后执行标量主体 | `computeFeature()` | `pcl::detail::rops::rotateCloudRVV()` 或标量 loop | adopted production-detail dispatch | `features/include/pcl/features/impl/rops_estimation.hpp` |
| `ROPSEstimation::getDistributionMatrix()` | production dispatch / fallback | 尝试 RVV row/col staging，失败后执行标量主体 | `computeFeature()` | `pcl::detail::rops::getDistributionMatrixRVV()` 或标量 loop | adopted production-detail dispatch | `features/include/pcl/features/impl/rops_estimation.hpp` |
| `rotateCloudRVV()` | production RVV helper | xyz stride load/store、rotation、min/max reduction | `rotateCloud()` | rotated cloud 和 AABB | correctness、asm、board performance | `features/include/pcl/features/impl/rops_estimation.hpp` |
| `getDistributionMatrixRVV()` | production RVV helper | projection row/col staging 和 scalar scatter 前置 | `getDistributionMatrix()` | distribution matrix | correctness、asm、board performance | `features/include/pcl/features/impl/rops_estimation.hpp` |
| `test_rops_estimation.cpp` | correctness test | 对 components、production helper、fallback 和 typed scope 对拍 | `make run_test_compare` | QEMU logs | correctness gate（正确性验收） | `test-rvv/features/rops_estimation/src/test_rops_estimation.cpp` |
| `bench_rops_estimation.cpp` | bench wrapper | 同一 wrapper 下比较 Std fallback 与 RVV production detail | board targets | summary / Evidence Doctor | production-detail performance | `test-rvv/features/rops_estimation/src/bench_rops_estimation.cpp` |
| `generate_rops_repeated_summary.py` | analysis script | 从 repeated board logs 生成 summary 和 manifest | Makefile summary targets | Evidence Doctor | evidence manifest | `test-rvv/features/rops_estimation/script/generate_rops_repeated_summary.py` |
| Phase 040 / 050 summaries | evidence output summary | 保存接入后板卡重复采集数据 | Doctor targets | evaluation / production doc | adopted board evidence | `test-rvv/features/rops_estimation/log/board/phase040_production_direct_repeated/summary.md`、`test-rvv/features/rops_estimation/log/board/phase050_pointxyzi_production_detail_repeated/summary.md`、`test-rvv/features/rops_estimation/log/board/phase050_pointnormal_production_detail_repeated/summary.md` |
| topic-local evaluation | documentation section | 保存候选取舍、阶段证据和未覆盖范围 | phase docs | production doc / reviewer | decision audit | `test-rvv/features/rops_estimation/doc/rops_estimation-evaluation.zh.md` |

## Production Closeout

| 项 | 最终状态 | 证据 |
| --- | --- | --- |
| 生产补丁范围 | 修改 `features/include/pcl/features/impl/rops_estimation.hpp`，新增 RVV helper 和两个 private helper dispatch；公开 API 不变 | production diff |
| 覆盖范围 | `PointInT` 满足 `RVVXYZAoSFloatLayout`、`PointOutT=pcl::Histogram<135>`、dense transformed local cloud、production private helper detail path | Phase 040/050 tests 和 summaries |
| 不覆盖范围 | 完整 public `computeFeature()` 性能、真实 mesh local surface、LRF、central moments、descriptor normalization、自定义点型逐个性能、`Scalar=double` | evaluation 和 roadmap |
| fallback | 非 RVV 构建、小规模、non-dense 和 layout 不满足均回到标量；无效 bins / projection / extent 被 RVV helper 拒绝，production caller 不产生非法 projection | `run_test_compare` |
| board evidence | `PointXYZ` median `1.700x`，`PointXYZI` median `1.520x`，`PointNormal` median `1.590x` | Phase 040/050 board summaries |
| Evidence Doctor | 三组均 `0E/0W/2S` | Phase 040/050 Evidence Doctor reports |
| 后续方向 | 当前 topic 内没有建议默认继续推进的高优先级算法优化 phase | roadmap |

## 后续方向

当前不建议在本 topic 内继续默认推进算法优化。原因如下：

- 完整 public `computeFeature()` 需要真实 mesh workload 和 profile。没有这类证据时，KdTree search、LRF、central moments、normalization 和当前 RVV 覆盖段的成本占比无法分开。
- descriptor normalization 只处理 135 个 float，单独 RVV 收益可能很小。需要 public profile 指向该尾段后再开 phase。
- distribution matrix 的 scatter 去标量化存在冲突 bin 和浮点顺序语义风险。当前已采纳的 row/col staging + 标量 scatter 在接入后板卡上保持正向。
- evidence metadata hardening 只增强归档质量，不改变当前算法收益判断。需要严格归档或后续出现长尾时再补。

因此当前默认动作是暂停在 S11 closeout 后，等待 reviewer 审查或用户进入 commit phase（提交阶段）。若要继续探索，应以新的 public workload/profile、scatter rewrite、descriptor normalization 或 evidence metadata hardening phase 重新冻结计划和证据边界。
