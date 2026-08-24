# transformation_estimation_svd_scale 函数级评估

## 范围和目标源码

本评估覆盖 `pcl::registration::TransformationEstimationSVDScale<PointSource, PointTarget, Scalar>` 的 similarity transformation estimation（相似变换估计，包含旋转、平移和尺度）路径。目标源码是：

- `registration/include/pcl/registration/transformation_estimation_svd_scale.h`
- `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp`

## 当前收口结论

当前 topic 可以结束。现有证据已经把三类路线分开：已采纳的 production 行为、已回滚的 sorted-copy double 候选、以及负向或不稳定的 mitigation family。当前文档保留决策审计和恢复条件，不再指定默认下一优化 phase。

| 分类 | 当前状态 | 关键证据 | 后续动作 |
| --- | --- | --- | --- |
| adopted production behavior（已采纳生产行为） | ordered direct fused、row-source、correspondence sorted-copy `Scalar=float`、contiguous affine fast path、`Scalar=double` ordered / row-source / generic、custom layout double 取样、matrix-local helper simplification | production correctness、QEMU smoke / ASM attribution、board repeated summary 和 Evidence Doctor 均已分层记录。 | 进入提交准备；后续只维护 adopted 边界。 |
| rolled back / no-production | correspondence sorted-copy `Scalar=double` | Phase 072 public Std/RVV positive；Phase 073 同边界 RVV-vs-RVV detail A/B negative；Phase 075 已回滚生产分流。 | 当前 double correspondence 在 contiguous fast path 不命中后使用 D64 gather。 |
| rejected / unstable | staged-selected-cloud、dual-indexed target-sorted、dual-indexed 256K source-sorted-copy | Phase 048 / 049 / 058 的 board detail A/B 或 stability 复核为 negative、mixed 或 unstable。 | 不作为当前 topic 的恢复动作。 |
| out of current scope（超出当前范围） | broader custom layout double、任意自定义点型全集、非法 index / correspondence、新 locality mitigation family、更广 sorted-copy double family selection | 当前证据只支持已列边界；这些方向需要新输入分布、scope 和 board budget。 | 另开 topic 或新授权后重新进入 phase loop。 |

接入后的板卡收益已经足够支撑当前采用边界：Phase 069 的 row-source common PCL xyz AoS `Scalar=double` 为 9/9 positive，median B/A `11.309x` 到 `17.433x`，Doctor `0/4/0`；Phase 070 的 custom layout double sample 为 4/4 positive，median B/A `9.966x` 到 `27.497x`，Doctor `0/1/0`；Phase 071 的更多 custom layout double sampling 为 12/12 positive，median B/A `2.766x` 到 `29.087x`，Doctor `0/8/0`。这些结果只支撑对应 production gate 和采样边界，不外推到全部 custom layout 或全部自定义点型。

## 历史阶段审计摘要

当前 EvidenceDecision 只需记住三件事：已采纳的 production 行为、已回滚的 sorted-copy double、以及已关闭的负向路线。更细的 phase 审计放在下表，不在首段重复展开。

| 类别 | 当前状态 | 说明 |
| --- | --- | --- |
| adopted production behavior | ordered direct fused、row-source、correspondence sorted-copy `Scalar=float`、contiguous affine fast path、`Scalar=double` ordered / row-source / generic、custom layout double 取样、matrix-local helper simplification | 这些是长期生产事实。 |
| rolled back / no-production | correspondence sorted-copy `Scalar=double` | Phase 072 public positive；Phase 073 negative；Phase 075 回滚。 |
| rejected / unstable | staged-selected-cloud、dual-indexed target-sorted、dual-indexed 256K source-sorted-copy | 这些路线不再作为默认恢复动作。 |
| out of current scope | broader custom layout double、任意自定义点型全集、非法 index / correspondence、新 locality mitigation family、更广 sorted-copy double family selection | 需要新 scope 或新 board budget。 |

收尾判断：这些证据已经把当前授权范围内的正向路线、负向路线和 rollback/no-production 边界分开。当前 evaluation 只保留决策审计，不再列默认下一优化 phase；若未来继续 broader custom layout double、任意自定义点型、非法 index / correspondence 或新的 locality mitigation，需要重新进入 phase loop。

## S0 偏好冻结

| 字段 | 当前值 |
| --- | --- |
| `preferences_loaded` | defaults loaded；local override absent；prompt override 指定 worker、新 topic 和板卡可用。 |
| `comment_policy_frozen` | test-rvv / diagnostic / prototype 详细中文注释；production 注释克制，只说明 gate / fallback / layout 等维护边界。 |
| `documentation_policy_frozen` | closeout current-state-first；复杂数值路径需要数值算例；长期文档不保留对话流程话术。 |
| `evidence_policy_frozen` | summary-only；raw logs 默认 local-only；QEMU 只证明 correctness / log shape，性能结论来自 board / target hardware。 |
| `commit_policy_frozen` | 不自动 commit；如进入提交阶段，topic、evidence logs 和 agent assets 分开。 |

## 函数族作用速览

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | 与主流程关系 | RVV 判断 |
| --- | --- | --- | --- | --- |
| `TransformationEstimationSVDScale` constructor | 继承普通 SVD，但传入 `false` 禁用 Umeyama no-scale 快路径。 | 设置 `use_umeyama_ == false`。 | ordered public overload 已由 scale 子类 override；Phase 043 又新增 row-source override。 | 普通 SVD 的 Umeyama RVV production path 不会命中本子类。 |
| 父类 `estimateRigidTransformation` overloads | 处理 fallback ordered、fallback indices 和 fallback correspondence 入口。 | 构造 `ConstCloudIterator` 后进入 shared helper。 | scale 子类通过 `using` 保持父类 overload set 可见；Phase 043 row-source float gate、Phase 067 row-source double gate、Phase 068 ordered generic double gate、Phase 069 row-source generic double gate 和 Phase 070/071 custom layout double gate 失败时显式回父类或回既有 D64 gather。 | 非 dense、小规模、非法 index/correspondence 和未验证 layout 仍不接 scale RVV；ordered exact `PointXYZ -> PointXYZ` / `Scalar=double` 已由 Phase 066 单独采纳，三类 row-source exact `PointXYZ -> PointXYZ` / `Scalar=double` 已由 Phase 067 单独采纳，ordered common PCL xyz AoS / `Scalar=double` 已由 Phase 068 单独采纳，row-source common PCL xyz AoS / `Scalar=double` 与 custom layout double samples 已由 Phase 069/070/071 收口为 adopted；Phase 075 已回滚 sorted-copy double，当前 double correspondence 使用 D64 gather。 |
| 父类 iterator helper / false branch | 计算 source/target centroid，构造 demean 动态矩阵。 | 输出 `cloud_src_demean`、`cloud_tgt_demean` 和 centroids。 | scale 子类的主成本前段。 | 直接 fused candidate 可绕过这段 materialization，但 production 需要新 dispatch。 |
| `getTransformationFromCorrelation` | 从 demean 矩阵构造 `H`、求 3x3 SVD、计算 scale 和 translation。 | 输出 4x4 similarity matrix。 | 本 topic 目标 helper。 | 可局部减少 `R4` 动态矩阵，也可用原始点对 fused accumulation 重建同一数学量。 |

## 标量流程与 RVV 流程对照

标量 public path（公开入口路径）在 scale 子类中固定走 `use_umeyama_ == false`：

```text
public overload
  -> ConstCloudIterator
  -> compute3DCentroid(source/target)
  -> demeanPointCloud(source/target)
  -> getTransformationFromCorrelation
       -> H = src_demean * tgt_demean^T
       -> Eigen 3x3 JacobiSVD
       -> R = V * U^T with determinant sign fix
       -> src_ = R4 * cloud_src_demean
       -> sum_ss = Σ ||source_demean||^2
       -> sum_tt = Σ target_demean · (R * source_demean)
       -> scale = float(sum_tt / sum_ss)
       -> transform = [scale * R, centroid_tgt - scale * R * centroid_src]
```

Phase 000 的 RVV diagnostic path（RVV 诊断路径）直接从 ordered point pairs 累加：

```text
source/target xyz
  -> source_sum, target_sum, source_target_cross_sum, source_square_sum
  -> H = Σ(s t^T) - n * mean_s * mean_t^T
  -> Eigen 3x3 JacobiSVD
  -> sum_ss = Σ||source||^2 - n||mean_s||^2
  -> sum_tt = trace(R * H)
  -> scale 和 translation
```

数值算例：若只有一个 source demean 向量 `s=(2,0,0)`，target demean 向量 `t=(4,0,0)`，且 `R=I`，则 `sum_ss=4`、`H(0,0)=8`、`sum_tt=trace(R*H)=8`，scale 为 `2`。真实测试使用多点非退化样本，避免单点分母为 0。

## 实现方式审计

| 实现维度 | 当前状态 | 证据 | 边界 / 恢复条件 |
| --- | --- | --- | --- |
| production dispatch / fallback | adopted-by-user | `registration/include/pcl/registration/transformation_estimation_svd_scale.h`、`registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp`、Phase 010 / 031 result。 | 已采纳 ordered / dense / `Scalar=float` / layout-gated xyz AoS；board direct evidence 仍以 `PointXYZ -> PointXYZ` 为代表。 |
| ordered direct fused accumulation | production_direct_positive | `run_test_compare`、QEMU public smoke Doctor、production ASM、board production repeated summary。 | 直接证据覆盖 ordered `PointXYZ -> PointXYZ` / `float` / dense。 |
| generic point type public evidence | positive_generic_public_board_complete + positive_more_generic_board_complete | Phase 040 / 041 result；`GenericXYZPointTypesMatchReference`；generic public QEMU smoke；generic public board summary；Phase 051 / 052 more-generic correctness / board summary。 | 覆盖 `PointXYZI` / `PointXYZRGB` 代表点型 correctness 和 board performance；Phase 052 又覆盖 5 个 common PCL xyz AoS 组合的 64K board performance；不外推到全部泛型点型或 row source。 |
| matrix-local simplification | adopted-by-user / production-helper-simplification | Phase 020 / 042 / 050 result；`MatrixLocalScaleSimplificationMatchesLegacyPath`；matrix-local board summary。 | 已收口为 production helper simplification；它不是新的 RVV intrinsic family，不扩大 dispatch、row-source、点型或 `Scalar` 范围。 |
| row source policy | adopted-by-user | Phase 043 已补 source-indexed、dual-indexed 和 correspondence public patch，QEMU correctness / smoke / board repeated / doctor / registry 已完成，且用户已确认采纳。Phase 044 又补代表泛型点型 row-source evidence。Phase 053 补了 source-indexed `PointXYZRGBA -> PointXYZRGBA`、dual-indexed `PointNormal -> PointXYZRGB` 和 correspondence `PointWithViewpoint -> PointXYZ` 的 more-generic row-source correctness / QEMU / board evidence。Phase 054 又补 Phase 051 五个点型组合 × 三类 row source 的全交叉 evidence。Phase 055 / 056 补两个 custom layout 样本和 256K custom row-source order profile；Phase 057 补 compact-ish / huge-padding padding sensitivity sampling；Phase 059 补 alignas custom layout alignment sensitivity sampling。Phase 045 解释了 order pattern 对性能的影响。Phase 046 继续把 shuffle mitigation detail A/B 分成 correspondence positive、dual-indexed mixed 和 4K rejected。Phase 047 已把 correspondence sorted-copy 子边界接入并采纳，Phase 048 排除了 staged-selected-cloud，Phase 049 排除了 target-sorted，Phase 058 排除了 dual-indexed 256K source-sorted-copy。 | 当前采纳直接证据覆盖 `PointXYZ -> PointXYZ` / `float` / dense；代表点型扩展覆盖 `PointXYZI` / `PointXYZRGB` 组合；Phase 054 覆盖 5 个更多常见点型组合的 row-source 全交叉；Phase 056 只覆盖两个 custom layout 样本的 256K controlled profile；Phase 057 只覆盖 compact-ish / huge-padding 两组 layout 的 64K/256K 取样；Phase 059 只覆盖一个 alignas layout 采样组合；locality profile 覆盖 `PointXYZ -> PointXYZ` 的 contiguous / stride / reverse / shuffle；sorted-copy 只覆盖 correspondence 中大规模 shuffle-like disorder，不能外推到 dual-indexed、全部自定义点型、异常 alignment 全集或全部 row source mitigation。 |
| `Scalar=double` ordered branch | adopted-by-user in Phase 066 | Phase 063/064 已证明 ordered `PointXYZ -> PointXYZ` double diagnostic correctness 和 board diagnostic；Phase 065/066 已证明真实 public ordered overload 下 f64 widened RVV branch 有正向板卡收益。 | 只覆盖 exact `PointXYZ -> PointXYZ` / ordered / dense / `nr_points >= 16`；generic double 和 custom layout double 仍需独立 phase。 |
| `Scalar=double` row-source branch | adopted-by-user in Phase 067 | Phase 067 已证明真实 source-indexed、dual-indexed 和 correspondence public overload 下 f64 widened RVV branch 有正向板卡收益。 | 只覆盖 exact `PointXYZ -> PointXYZ` / 三类 row-source / dense / `nr_points >= 16`；generic double、custom layout double、sorted-copy double 和非法 index / correspondence 仍需独立 phase。 |
| `Scalar=double` ordered generic branch | adopted-by-user in Phase 068 | Phase 068 已证明真实 ordered public overload 下 common PCL xyz AoS whitelist 的 f64 widened RVV branch 有正向板卡收益。 | 只覆盖 ordered common PCL xyz AoS whitelist / dense / `nr_points >= 16`；row-source generic double、custom layout double、sorted-copy double 和任意自定义点型全集仍需独立 phase。 |
| `Scalar=double` row-source generic branch | adopted-by-user in Phase 074 | Phase 069 已证明真实三类 row-source public overload 下 common PCL xyz AoS whitelist 的 f64 widened RVV branch 有正向板卡收益。 | Phase 074 已根据用户确认收口为 adopted；只覆盖 row-source common PCL xyz AoS whitelist / dense / 64K / 合法 index 或 correspondence。 |
| `Scalar=double` custom layout sample branch | adopted-by-user in Phase 074 | Phase 070 已证明 `LocalSVDScalePaddedXYZSource -> LocalSVDScaleWideXYZTarget` 的 ordered 与三类 row-source public double path 有正向板卡收益。 | Phase 074 已根据用户确认收口为 adopted；只覆盖该测试本地 registered custom layout sample，不外推到全部 custom layout double、packed unaligned float、异常 alignment 全集或 sorted-copy double。 |
| `Scalar=double` more custom layout sampling branch | adopted-by-user in Phase 074 | Phase 071 已证明 compact、huge-padding 和 aligned 三组 custom layout 的 ordered 与三类 row-source public double path 有正向板卡收益。 | Phase 074 已根据用户确认收口为 adopted；只覆盖这三组测试本地 registered custom layout sample，并且只作为 Phase 070 取样增强，不外推到全部 custom layout double、packed unaligned float、异常 alignment 全集或 sorted-copy double。 |
| `Scalar=double` correspondence sorted-copy branch | rolled_back_no_production in Phase 075 | Phase 072 已证明真实 public correspondence overload 下 sorted-copy double branch 对 public scalar fallback 有正向板卡收益，64K / 256K median B/A `5.727x` / `5.408x`；Phase 073 又证明同边界 RVV-vs-RVV detail A/B 的 family selection 为 negative，64K / 256K median B/A `0.283x` / `0.431x`；Phase 075 已回滚该分流。 | 当前 production 不再尝试 sorted-copy double；`PointXYZ -> PointXYZ` / dense shuffled correspondence / 64K + 256K 使用既有 D64 gather RVV family。Phase 047 `Scalar=float` sorted-copy 不受影响。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- |
| `TransformationEstimationSVDScale::getTransformationFromCorrelation` | production target helper | 当前 scale 标量后段。 | 父类 false branch 虚调用。 | scalar truth / production boundary | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| `TransformationEstimationSVDScale::estimateRigidTransformation` ordered overload | production patch | 尝试 RVV scale fused accumulation，失败则回父类 ordered overload。 | public ordered cloud-pair caller。 | production direct | `registration/include/pcl/registration/transformation_estimation_svd_scale.h`、`registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| `TransformationEstimationSVDScale::estimateRigidTransformation` row-source overloads | adopted production patch | source-indexed、dual-indexed 和 correspondence 尝试 RVV scale fused accumulation，失败则回父类对应 overload。 | public row-source caller。 | production public row-source adopted | `registration/include/pcl/registration/transformation_estimation_svd_scale.h`、`registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| correspondence sorted-copy branch | adopted production branch | correspondence size / disorder gate 命中时复制并排序 correspondence，再复用 correspondence RVV accumulation。 | public correspondence overload。 | production public correspondence sorted-copy adopted | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| correspondence sorted-copy double branch | bounded production candidate | `Scalar=double` correspondence size / disorder gate 命中时复制并排序 correspondence，再复用 D64 correspondence RVV accumulation。 | public correspondence overload。 | Phase 072 public positive + Phase 073 detail A/B negative；不支持 clean adoption。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| `include/tesvd_scale.h` | test support aggregator | 稳定 include 入口。 | test / bench 源码。 | reviewer navigation | `test-rvv/registration/transformation_estimation_svd_scale/include/tesvd_scale.h` |
| `include/impl/tesvd_scale_support.hpp` | fixture / samples | 构造 scale transform 和点云。 | candidate / test / bench。 | correctness input | `test-rvv/registration/transformation_estimation_svd_scale/include/impl/tesvd_scale_support.hpp` |
| `include/impl/tesvd_scale_candidates.hpp` | diagnostic candidate | 标量 reference、RVV candidate、checksum。 | gtest / bench wrapper。 | diagnostic correctness / bench input | `test-rvv/registration/transformation_estimation_svd_scale/include/impl/tesvd_scale_candidates.hpp` |
| `estimateScaleStdDouble` / `estimateScaleRVVDouble` | diagnostic candidate | `Scalar=double` scalar accumulation 和 RVV f64 widened accumulation。 | Phase 063 gtest / `scalar-double-diagnostic-scout` bench。 | double diagnostic correctness scout；非 production dispatch | `test-rvv/registration/transformation_estimation_svd_scale/include/impl/tesvd_scale_candidates.hpp` |
| `accumulateTransformationEstimationSVDScaleOrderedPointXYZD64RVV` / `solveTransformationEstimationSVDScaleD64` | production helper | ordered exact `PointXYZ -> PointXYZ` / `Scalar=double` 的 f64 widened accumulation 和 double solve。 | production ordered overload。 | Phase 065/066 adopted scalar-double production branch。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| `accumulateTransformationEstimationSVDScaleOrderedCloudPairD64RVV` / `solveTransformationEstimationSVDScaleD64` | production helper | ordered common PCL xyz AoS whitelist / `Scalar=double` 的 f64 widened accumulation 和 double solve。 | production ordered overload。 | Phase 068 adopted ordered generic scalar-double production branch。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| `accumulateTransformationEstimationSVDScaleSourceIndexedPointXYZD64RVV` / `accumulateTransformationEstimationSVDScaleDualIndicesPointXYZD64RVV` / `accumulateTransformationEstimationSVDScaleCorrespondencePointXYZD64RVV` | production helper | row-source exact `PointXYZ -> PointXYZ` / `Scalar=double` 的 f64 widened accumulation；contiguous slice 优先走 offset fast path，非 contiguous 合法输入走 widened gather。 | production row-source overloads。 | Phase 067 adopted row-source scalar-double production branch。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| `accumulateTransformationEstimationSVDScaleSourceIndexedCloudPairD64RVV` / `accumulateTransformationEstimationSVDScaleDualIndicesCloudPairD64RVV` / `accumulateTransformationEstimationSVDScaleCorrespondencePairD64RVV` | production helper | row-source generic xyz AoS / `Scalar=double` 的 f64 widened accumulation；Phase 070 / 071 当前 patch 也允许测试本地 custom layout sample 在同一 layout gate 下命中；Phase 075 后 correspondence double 使用该 D64 gather family。 | production row-source overloads。 | Phase 069 / 070 / 071 adopted-by-user；Phase 075 rollback 后保留 D64 gather。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| `src/test_tesvd_scale.cpp` | correctness test | 对拍 public scale、同构标量和 RVV candidate。 | QEMU / board test target。 | correctness gate | `test-rvv/registration/transformation_estimation_svd_scale/src/test_tesvd_scale.cpp` |
| `src/bench_tesvd_scale.cpp` | bench wrapper | 输出 public scale baseline、fused candidate timing、matrix-local formula shape timing、generic public timing、more-generic public timing、row-source timing 和 locality profile timing。 | QEMU smoke / board repeated。 | log shape / board performance | `test-rvv/registration/transformation_estimation_svd_scale/src/bench_tesvd_scale.cpp` |
| `script/generate_tesvd_scale_board_repeated_summary.py` | analysis script | 生成 board summary、manifest 和 Evidence Doctor 输入。 | board repeated target。 | evidence summary | `test-rvv/registration/transformation_estimation_svd_scale/script/generate_tesvd_scale_board_repeated_summary.py` |

## Phase 000 证据摘要

| evidence | result | path / notes |
| --- | --- | --- |
| QEMU correctness | Phase 000 完成时 Std/RVV 各 4 个 gtest 全通过；当前 `log/qemu/run_test_std.log` / `log/qemu/run_test_rvv.log` 已更新为 21-test rerun。 | 历史结论见 `doc/phases/000-current-state-and-gaps/result.zh.md`；当前 correctness 见 `run_test_compare` 21-test rerun。 |
| QEMU smoke Doctor | historical `ordered-cloud-pair` smoke 为 `Errors=0`、`Warnings=0`；QEMU timing 不作为性能证据。当前裸 `log/qemu/evidence_doctor.md` 是 Phase 020 matrix-local smoke。 | 历史结论见 `doc/phases/000-current-state-and-gaps/result.zh.md`；当前裸文件见 `log/qemu/evidence_doctor.md`。 |
| board correctness smoke | Phase 000 historical：RVV test binary 板卡 4 个 gtest 全通过；当前 Phase 010/020 未把 board smoke 作为 7-test 主证据，而是使用 QEMU 7-test correctness 和 board repeated evidence。 | `log/board/test_smoke/run_test.log` |
| board repeated diagnostic | 4K/64K/256K median B/A = `2.783x` / `2.958x` / `2.937x`，全部 `positive`。 | `log/board/ordered_cloud_pair_repeated/summary.md` |
| board Evidence Doctor | `Errors=0`、`Warnings=1`；4K max/min = `1.15` long-tail warning，4K 最小仍为 `2.513x`。 | `log/board/ordered_cloud_pair_repeated/evidence_doctor.md` |
| asm shape | RVV bench asm 可见 `vlseg4e32.v` / `vlsseg3e32.v`、`vfmacc.vv`、`vfredosum.vs`、`vsetvli`。 | `build/asm/riscv/bench_transformation_estimation_svd_scale_rvv.asm` |
| registry | Phase 000 board / smoke evidence 保持登记；QEMU correctness 当前登记为 21-test rerun。 | `make -C test-rvv/registration/transformation_estimation_svd_scale evidence_status` |

## Phase 010 生产接入证据摘要

| evidence | result | path / notes |
| --- | --- | --- |
| production patch | ordered public overload override + RVV scale helper。 | `registration/include/pcl/registration/transformation_estimation_svd_scale.h`、`registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| QEMU correctness | Phase 010 historical：Std/RVV 各 6 个 gtest 全通过；当前可覆盖日志已更新为 21-test rerun。 | 历史结论见 `doc/phases/010-production-integration-plan/result.zh.md`；当前日志见 `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| QEMU public-scale smoke | `Errors=0`、`Warnings=0`；QEMU timing 只作 smoke。 | 历史结论见 `doc/phases/010-production-integration-plan/result.zh.md`；当前裸 `log/qemu/evidence_doctor.md` 已被 Phase 020 覆盖。 |
| production ASM attribution | ordered public overload 符号内可见 `vlsseg3e32.v`、`vfmacc.vv`、`vfredosum.vs`、`vsetvli`，并调用 `solveTransformationEstimationSVDScaleF32`。 | `build/asm/riscv/bench_transformation_estimation_svd_scale_rvv.full.asm` |
| board production repeated | 4K/64K/256K median B/A = `26.123x` / `33.860x` / `33.066x`，全部 `positive`。 | `log/board/production_public_scale_ordered_cloud_pair_repeated/summary.md` |
| board production Evidence Doctor | `Errors=0`、`Warnings=1`；warning 为 4K 与组内 median 偏离，4K 自身仍 positive。 | `log/board/production_public_scale_ordered_cloud_pair_repeated/evidence_doctor.md` |
| registry | fresh。 | `make -C test-rvv/registration/transformation_estimation_svd_scale evidence_status` |

## Phase 020 实现形态诊断证据摘要

| evidence | result | path / notes |
| --- | --- | --- |
| QEMU correctness | Std/RVV 各 7 个 gtest 全通过。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| QEMU matrix-local smoke | `Errors=0`、`Warnings=0`；QEMU timing 只作 log-shape smoke。 | `log/qemu/evidence_doctor.md` |
| board matrix-local repeated | 4K/64K/256K median B/A = `1.683x` / `1.173x` / `1.173x`，overall `weak_positive`。 | `log/board/matrix_local_scale_repeated/summary.md` |
| board matrix-local Evidence Doctor | `Errors=0`、`Warnings=2`；4K 有 long-tail 和 group-outlier，处理方式是按 size 分开报告并降级为实现形态 weak-positive。 | `log/board/matrix_local_scale_repeated/evidence_doctor.md` |
| registry | fresh。 | `make -C test-rvv/registration/transformation_estimation_svd_scale evidence_status` |

## Phase 040/041 泛型点型 public 摘要

| evidence | result | path / notes |
| --- | --- | --- |
| QEMU correctness | Phase 041 当时 Std/RVV 各 8 个 gtest 全通过；后续 Phase 043 扩展为 11 个 gtest。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| representative point types | `PointXYZI -> PointXYZI`、`PointXYZRGB -> PointXYZRGB`、`PointXYZI -> PointXYZRGB`、`PointXYZRGB -> PointXYZ` 与同构标量 reference 对齐。 | `src/test_tesvd_scale.cpp` 的 `GenericXYZPointTypesMatchReference`。 |
| QEMU generic public smoke | `Errors=0`、`Warnings=0`、`Suggestions=0`；QEMU timing 只作 log-shape smoke。 | `log/qemu/generic_xyz_point_types_public/evidence_doctor.md` |
| board generic public repeated | 8 个代表点型 case median B/A 均为 positive，范围为 `23.679x` 到 `28.268x`。 | `log/board/generic_xyz_point_types_public_repeated/summary.md` |
| board generic public Evidence Doctor | `Errors=0`、`Warnings=0`、`Suggestions=0`。 | `log/board/generic_xyz_point_types_public_repeated/evidence_doctor.md` |
| boundary | representative generic public only。 | 不外推到全部 xyz AoS、source-indexed、dual-indexed 或 correspondence。 |

## Phase 043 row-source public 摘要

| evidence | result | path / notes |
| --- | --- | --- |
| QEMU correctness | Phase 043 时 Std/RVV 各 11 个 gtest 全通过；后续 Phase 044 扩展后 Std/RVV 各 12 个 gtest 全通过。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| row-source correctness | `SourceIndexedScaleMatchesReference`、`DualIndexedScaleMatchesReference`、`CorrespondenceScaleMatchesReference` 均通过。 | `src/test_tesvd_scale.cpp` |
| QEMU row-source smoke | `Errors=0`、`Warnings=0`、`Suggestions=0`；QEMU timing 只作 log-shape smoke。 | `log/qemu/row_source_scale/evidence_doctor.md` |
| board row-source repeated | 9 个 source-indexed / dual-indexed / correspondence case 全部 `positive`；median B/A 范围 `6.171x` 到 `16.309x`。 | `log/board/row_source_scale_repeated/summary.md` |
| board row-source Evidence Doctor | `Errors=0`、`Warnings=7`、`Suggestions=0`；warning 来自 correspondence 长尾、dual-indexed 256K 长尾和 source-indexed 组内离群。 | `log/board/row_source_scale_repeated/evidence_doctor.md` |
| decision | `adopted-by-user`。 | Phase 043 result；用户确认后已收口为 adopted。 |

## Phase 044 row-source 泛型代表点型摘要

| evidence | result | path / notes |
| --- | --- | --- |
| QEMU correctness | Std/RVV 各 12 个 gtest 全通过。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| row-source generic correctness | `RowSourceGenericXYZPointTypesMatchReference` 覆盖 source-indexed `PointXYZI -> PointXYZI`、dual-indexed `PointXYZRGB -> PointXYZRGB`、correspondence `PointXYZI -> PointXYZRGB`。 | `src/test_tesvd_scale.cpp` |
| shuffle-sorted-copy correctness | `RowSourceSortedCopyMatchesShuffledPublicPath` 覆盖 shuffled dual-indexed / correspondence sorted-copy 对拍。 | `src/test_tesvd_scale.cpp` |
| QEMU row-source generic smoke | `Errors=0`、`Warnings=0`、`Suggestions=0`；QEMU timing 只作 log-shape smoke。 | `log/qemu/row_source_generic_xyz_point_types/evidence_doctor.md` |
| board row-source generic repeated | 9 个代表点型 row-source case 全部 `positive`；median B/A 范围 `6.325x` 到 `10.371x`。 | `log/board/row_source_generic_xyz_point_types_repeated/summary.md` |
| board row-source generic Evidence Doctor | `Errors=0`、`Warnings=12`、`Suggestions=0`；warning 来自 long-tail / variance 和 source-indexed 组内离群。 | `log/board/row_source_generic_xyz_point_types_repeated/evidence_doctor.md` |
| decision | `positive_row_source_generic_public_board_complete`。 | 只覆盖代表点型，不外推到全部自定义 xyz AoS、`Scalar=double` 或非法 index / correspondence。 |

## Phase 045 row-source locality / order profile 摘要

| evidence | result | path / notes |
| --- | --- | --- |
| QEMU correctness | Std/RVV 各 12 个 gtest 全通过。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| QEMU row-source locality smoke | 36 comparisons；`Errors=0`、`Warnings=0`、`Suggestions=0`；QEMU timing 只作 log-shape smoke。 | `log/qemu/row_source_locality_order_profile/evidence_doctor.md` |
| board row-source locality repeated | 36 个 row source / order / size case 全部 `positive`；contiguous / stride / reverse 强正向，shuffle 仍 positive 但明显掉速。 | `log/board/row_source_locality_order_profile_repeated/summary.md` |
| board row-source locality Evidence Doctor | `Errors=0`、`Warnings=29`、`Suggestions=0`；warning 来自 long-tail / variance 和组内离群。 | `log/board/row_source_locality_order_profile_repeated/evidence_doctor.md` |
| decision | `profile_positive_with_locality_sensitivity`。 | 解释 Phase 043 / 044 warning；不新增 RVV family，也不扩大到全部点型、非法 index / correspondence 或 `Scalar=double`。 |

## Phase 046 row-source shuffle mitigation detail A/B 摘要

| evidence | result | path / notes |
| --- | --- | --- |
| QEMU correctness | Std/RVV 各 13 个 gtest 全通过；新增 `RowSourceSortedCopyMatchesShuffledPublicPath`。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| QEMU sorted-copy detail smoke | 6 个 paired case 可解析；QEMU 全部 negative，仅作为 build / label / manifest shape。 | `log/qemu/row_source_shuffle_sorted_copy_detail_ab/summary.md`、`evidence_doctor.md` |
| board sorted-copy detail A/B | correspondence 64K/256K positive，median B/A `2.344x` / `2.130x`；dual-indexed 256K weak-positive，median `1.185x`；4K negative。 | `log/board/row_source_shuffle_sorted_copy_detail_ab_repeated/summary.md` |
| board sorted-copy Evidence Doctor | `Errors=2`、`Warnings=8`、`Suggestions=0`；Errors 来自 4K 5/5 退化，Warnings 包含 dual-indexed 64K 退化和 long-tail。 | `log/board/row_source_shuffle_sorted_copy_detail_ab_repeated/evidence_doctor.md` |
| decision | `conditional_positive`。 | 只把 correspondence 64K/256K 写成 production probe candidate；不直接修改 production，不写成 adopted。 |

## Phase 047 correspondence sorted-copy production probe 摘要

| evidence | result | path / notes |
| --- | --- | --- |
| QEMU correctness | Phase 047 当时 Std/RVV 各 14 个 gtest 全通过；当前 Phase 049 后 Std/RVV 各 15 个 gtest 全通过。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| QEMU production probe smoke | `Errors=0`、`Warnings=0`、`Suggestions=0`；QEMU timing 只作 log-shape smoke。 | `log/qemu/correspondence_sorted_copy_production_probe/evidence_doctor.md` |
| board production probe | 4K/64K/256K median B/A = `8.128x` / `3.929x` / `3.618x`，全部 positive。 | `log/board/correspondence_sorted_copy_production_probe_repeated/summary.md` |
| board production probe Evidence Doctor | `Errors=0`、`Warnings=1`、`Suggestions=0`；warning 是 4K group-outlier，按 size 分开报告。 | `log/board/correspondence_sorted_copy_production_probe_repeated/evidence_doctor.md` |
| decision | `adopted-by-user`。 | 只覆盖 correspondence、size >= 64K、shuffle-like disorder、`Scalar=float`、dense xyz AoS。 |

## Phase 048 staged-selected-cloud detail A/B 摘要

| evidence | result | path / notes |
| --- | --- | --- |
| QEMU staged-selected-cloud smoke | paired labels / manifest 可解析；QEMU timing 不作为性能结论。 | `log/qemu/row_source_shuffle_staged_selected_cloud_detail_ab/summary.md`、`evidence_doctor.md` |
| board staged-selected-cloud detail A/B | 6 个 dual-indexed / correspondence shuffled case 全部 negative；correspondence 64K/256K median B/A = `0.482x` / `0.563x`。 | `log/board/row_source_shuffle_staged_selected_cloud_detail_ab_repeated/summary.md` |
| board staged-selected-cloud Evidence Doctor | `Errors=6`、`Warnings=9`、`Suggestions=0`。 | `log/board/row_source_shuffle_staged_selected_cloud_detail_ab_repeated/evidence_doctor.md` |
| decision | `attempted_negative_or_mixed`。 | copy + staging 成本吞掉收益；不进入 production。 |

## Phase 049 dual-indexed target-sorted detail A/B 摘要

| evidence | result | path / notes |
| --- | --- | --- |
| QEMU correctness | Std/RVV 各 15 个 gtest 全通过；新增 `RowSourceTargetSortedMatchesShuffledPublicPath`。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| QEMU target-sorted smoke | paired labels / manifest 可解析；QEMU timing 不作为性能结论。 | `log/qemu/row_source_shuffle_dual_indexed_target_sorted_detail_ab/summary.md`、`evidence_doctor.md` |
| board target-sorted detail A/B | dual-indexed 64K median B/A `0.897x` negative；256K median B/A `1.170x` weak-positive。 | `log/board/row_source_shuffle_dual_indexed_target_sorted_detail_ab_repeated/summary.md` |
| board target-sorted Evidence Doctor | `Errors=1`、`Warnings=1`。 | `log/board/row_source_shuffle_dual_indexed_target_sorted_detail_ab_repeated/evidence_doctor.md` |
| decision | `attempted_negative_or_mixed`。 | 64K 退化且 256K 只有 weak-positive；不进入 production。 |

## Phase 058 dual-indexed 256K sorted-copy stability 摘要

| evidence | result | path / notes |
| --- | --- | --- |
| QEMU correctness | Std/RVV 各 20 个 gtest 全通过。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| QEMU stability smoke | dual-indexed 256K paired label 可解析；Doctor `Errors=1`、`Warnings=0`、`Suggestions=0`，QEMU timing 不作为性能结论。 | `log/qemu/row_source_shuffle_dual_indexed_256k_sorted_copy_stability/summary.md`、`evidence_doctor.md` |
| board stability repeated | 10-run B/A 为 `0.979, 1.087, 1.119, 1.167, 0.957, 0.804, 1.231, 0.902, 0.946, 1.041`；median `1.010x`，5/10 run 低于 1。 | `log/board/row_source_shuffle_dual_indexed_256k_sorted_copy_stability_repeated/summary.md` |
| board stability Evidence Doctor | `Errors=1`、`Warnings=1`、`Suggestions=1`。 | `log/board/row_source_shuffle_dual_indexed_256k_sorted_copy_stability_repeated/evidence_doctor.md` |
| decision | `rejected_or_unstable_with_evidence / sorted-copy-dual-indexed-256k-stability`。 | 关闭 Phase 046 dual-indexed 256K weak-positive residual；不进入 production probe，不影响 Phase 047 correspondence sorted-copy adopted branch。 |

## Phase 050 matrix-local adoption closeout 摘要

| evidence | result | path / notes |
| --- | --- | --- |
| production helper | `trace(R * H)` 已在 `getTransformationFromCorrelation` 中使用。 | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` |
| correctness | Phase 050 当时 Std/RVV 各 15 个 gtest 全通过；Phase 051 后更新为 16 tests；Phase 053 后更新为 17 tests；Phase 054 后更新为 18 tests；Phase 055 后更新为 19 tests；Phase 057 后更新为 20 tests；Phase 059 后更新为 21 tests；Phase 060 后更新为 22 tests；Phase 061 后当前 correctness 已更新为 25 tests。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| board matrix-local repeated | 4K/64K/256K median B/A = `1.683x` / `1.173x` / `1.173x`，overall `weak_positive`。 | `log/board/matrix_local_scale_repeated/summary.md` |
| decision | `adopted-by-user / production-helper-simplification`。 | 只收口 helper 简化，不新增 RVV family。 |

## Phase 051/052 more-generic xyz AoS 点型摘要

| evidence | result | path / notes |
| --- | --- | --- |
| QEMU correctness | Std/RVV 各 16 个 gtest 全通过；新增 `MoreGenericXYZAoSPointTypesMatchReference`。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| more-generic correctness | 覆盖 `PointXYZRGBA -> PointXYZRGBA`、`PointXYZL -> PointXYZ`、`PointNormal -> PointXYZRGB`、`PointWithRange -> PointWithRange` 和 `PointWithViewpoint -> PointXYZ`。 | `src/test_tesvd_scale.cpp` |
| QEMU more-generic public smoke | 5 个 label 可解析；Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`；QEMU timing 不作为性能结论。 | `log/qemu/more_generic_xyz_aos_point_types_public/evidence_doctor.md` |
| board more-generic public repeated | 5 个 case 全部 positive；median B/A 范围为 `20.555x` 到 `26.989x`；Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`。 | `log/board/more_generic_xyz_aos_point_types_public_repeated/summary.md`、`evidence_doctor.md` |
| decision | `positive_more_generic_board_complete / more-generic-xyz-aos-point-types`。 | common PCL xyz AoS 点型 correctness / QEMU smoke / board repeated 已闭合；不外推到全部自定义点型、row-source 点型扩展或 `Scalar=double`。 |

## Phase 053 row-source more-generic xyz AoS 摘要

| evidence | result | path / notes |
| --- | --- | --- |
| QEMU correctness | Phase 053 时 Std/RVV 各 17 个 gtest 全通过；Phase 054 后更新为 18 tests；Phase 055 后更新为 19 tests；Phase 057 后更新为 20 tests；Phase 059 后更新为 21 tests；Phase 060 后更新为 22 tests；Phase 061 后当前 correctness 已更新为 25 tests。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| row-source more-generic correctness | 覆盖 source-indexed `PointXYZRGBA -> PointXYZRGBA`、dual-indexed `PointNormal -> PointXYZRGB` 和 correspondence `PointWithViewpoint -> PointXYZ`。 | `src/test_tesvd_scale.cpp` |
| QEMU row-source more-generic smoke | 9 个 label 可解析；Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`；QEMU timing 不作为性能结论。 | `log/qemu/row_source_more_generic_xyz_aos_point_types/evidence_doctor.md` |
| board row-source more-generic repeated | 9 个 case 全部 positive；median B/A 范围为 `7.729x` 到 `12.614x`；Doctor `Errors=0`、`Warnings=3`、`Suggestions=0`。 | `log/board/row_source_more_generic_xyz_aos_point_types_repeated/summary.md`、`evidence_doctor.md` |
| decision | `positive_row_source_more_generic_public_board_complete / row-source-more-generic-xyz-aos-point-types`。 | 只覆盖 3 个更多常见 PCL xyz AoS row-source 代表组合；不外推到 Phase 051 全部点型 row-source 全交叉、全部自定义点型或 `Scalar=double`。 |

## Phase 054 row-source all-more-generic xyz AoS 摘要

| evidence | result | path / notes |
| --- | --- | --- |
| QEMU correctness | Std/RVV 各 18 个 gtest 全通过；新增 `RowSourceAllMoreGenericXYZAoSMatrixMatchesReference`。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| row-source all-more-generic correctness | 覆盖 Phase 051 五个常见 PCL xyz AoS 点型组合 × source-indexed / dual-indexed / correspondence。 | `src/test_tesvd_scale.cpp` |
| QEMU row-source all-more-generic smoke | 45 个 label 可解析；Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`；QEMU timing 不作为性能结论。 | `log/qemu/row_source_all_more_generic_xyz_aos_matrix/evidence_doctor.md` |
| board row-source all-more-generic repeated | 45 个 case 全部 positive；overall median B/A 范围为 `7.277x` 到 `12.565x`；Doctor `Errors=0`、`Warnings=18`、`Suggestions=0`。 | `log/board/row_source_all_more_generic_xyz_aos_matrix_repeated/summary.md`、`evidence_doctor.md` |
| decision | `positive_row_source_all_more_generic_public_board_complete / row-source-all-more-generic-xyz-aos-matrix`。 | 关闭 Phase 051 五个点型组合的 row-source 全交叉缺口；不外推到全部自定义点型、异常 layout / padding、非法 index/correspondence 或 `Scalar=double`。 |

## Phase 055 custom xyz AoS layout sampling 摘要

| evidence | result | path / notes |
| --- | --- | --- |
| QEMU correctness | Std/RVV 各 19 个 gtest 全通过；新增 `CustomXYZAoSLayoutSamplingMatchesReference`。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| custom layout correctness | 覆盖 `LocalSVDScalePaddedXYZSource -> LocalSVDScaleWideXYZTarget` 的 ordered、source-indexed、dual-indexed 和 correspondence reference。 | `src/test_tesvd_scale.cpp` |
| QEMU custom layout smoke | 12 个 label 可解析；Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`；QEMU timing 不作为性能结论。 | `log/qemu/custom_xyz_aos_layout_sampling/evidence_doctor.md` |
| board custom layout repeated | ordered 和多数 row-source slice positive；256K dual-indexed 有 2/5 run 低于 1，correspondence 256K 有 1/5 run 低于 1。 | `log/board/custom_xyz_aos_layout_sampling_repeated/summary.md`、`evidence_doctor.md` |
| decision | `mixed_custom_layout_sampling`。 | 只覆盖两个测试本地 custom layout 样本；256K dual-indexed / correspondence 保留 mixed / unstable 历史 slice。 |

## Phase 056 custom row-source large variance profile 摘要

| evidence | result | path / notes |
| --- | --- | --- |
| QEMU correctness | 当前 Std/RVV 各 20 个 gtest 全通过。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| QEMU custom row-source profile smoke | 8 个 label 可解析；Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`；最大 reference error `5.722e-05`。 | `log/qemu/custom_row_source_large_variance_profile/evidence_doctor.md` |
| board custom row-source profile repeated | 8 个 256K controlled order-pattern case 全部 positive；median B/A 范围 `2.129x` 到 `9.109x`。 | `log/board/custom_row_source_large_variance_profile_repeated/summary.md` |
| board custom row-source Evidence Doctor | `Errors=0`、`Warnings=8`、`Suggestions=0`；warning 来自 stride / reverse / shuffle long-tail 和组内离群。 | `log/board/custom_row_source_large_variance_profile_repeated/evidence_doctor.md` |
| decision | `profile_positive_with_variance_warnings`。 | 受控 profile 没有复现 Phase 055 的 `<1x` 退化，但不能外推到全部 custom layout、异常 padding / alignment 或 `Scalar=double`。 |

## Phase 057 custom layout padding sensitivity 摘要

| evidence | result | path / notes |
| --- | --- | --- |
| QEMU correctness | Std/RVV 各 20 个 gtest 全通过；新增 `CustomLayoutPaddingSensitivityMatchesReference`。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| custom layout padding correctness | 覆盖 compact-ish 与 huge-padding 两组 custom layout 的 ordered、source-indexed、dual-indexed 和 correspondence reference。 | `src/test_tesvd_scale.cpp` |
| QEMU custom layout padding smoke | 12 个 label 可解析；Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`；最大 reference error `3.039837e-05`；QEMU timing 不作为性能结论。 | `log/qemu/custom_layout_padding_sensitivity/evidence_doctor.md` |
| board custom layout padding repeated | 12 个 case 全部 positive；compact-ish median B/A `9.248x` 到 `14.521x`，huge-padding median B/A `2.518x` 到 `4.002x`。 | `log/board/custom_layout_padding_sensitivity_repeated/summary.md` |
| board custom layout padding Evidence Doctor | `Errors=0`、`Warnings=15`、`Suggestions=0`；warning 来自 huge-padding long-tail / variance 和 layout / stride group-outlier。 | `log/board/custom_layout_padding_sensitivity_repeated/evidence_doctor.md` |
| decision | `sampled_positive_with_padding_sensitivity_warnings`。 | 只能按 layout / row source / size 分开解释；不能外推到全部 custom layout、异常 alignment 或 `Scalar=double`。 |

## Phase 059 custom layout alignment sensitivity 摘要

| evidence | result | path / notes |
| --- | --- | --- |
| QEMU correctness | Std/RVV 各 21 个 gtest 全通过；新增 `CustomLayoutAlignmentSensitivityMatchesReference`。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| custom layout alignment correctness | 覆盖 `LocalAligned64XYZSource -> LocalAligned32XYZTarget` alignas custom layout 的 source-indexed、dual-indexed 和 correspondence reference。 | `src/test_tesvd_scale.cpp` |
| QEMU custom layout alignment smoke | 6 个 label 可解析；Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`；最大 reference error `3.039837e-05`；QEMU timing 不作为性能结论。 | `log/qemu/custom_layout_alignment_sensitivity/evidence_doctor.md` |
| board custom layout alignment repeated | 6 个 case 全部 positive；median B/A 范围 `1.636x` 到 `2.309x`。 | `log/board/custom_layout_alignment_sensitivity_repeated/summary.md` |
| board custom layout alignment Evidence Doctor | `Errors=0`、`Warnings=3`、`Suggestions=0`；warning 来自 source-indexed 和 dual-indexed 256K variance。 | `log/board/custom_layout_alignment_sensitivity_repeated/evidence_doctor.md` |
| decision | `sampled_positive_with_alignment_sensitivity_warnings`。 | 只能按这个 alignment-sensitive 采样组合、row source 和 size 分开解释；不能外推到全部 custom layout、packed unaligned float、异常 alignment 全集或 `Scalar=double`。 |

## Phase 060/061/062 affine index fast path 摘要

| evidence | result | path / notes |
| --- | --- | --- |
| Phase 060 detail A/B | `AffineIndexFastPathCandidateMatchesReference` 通过；board 6 个 RVV-vs-RVV case 全 positive，median B/A `1.676x` 到 `2.750x`；Doctor `Errors=0`、`Warnings=3`。 | `doc/phases/060-affine-index-fast-path-detail-ab/result.zh.md`、`log/board/row_source_affine_index_fast_path_detail_ab_repeated/summary.md` |
| Phase 061 correctness | Std/RVV 各 23 tests passed；新增 `AffineIndexFastPathPublicProbeMatchesReference` 覆盖 source-indexed、dual-indexed 和 correspondence public path。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| Phase 061 QEMU smoke | 6 comparisons，Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`；QEMU timing 不作为性能结论。 | `log/qemu/row_source_affine_index_fast_path_production_probe/evidence_doctor.md` |
| Phase 061 board repeated | 6/6 case positive；median B/A `10.115x` 到 `14.021x`；Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`。 | `log/board/row_source_affine_index_fast_path_production_probe_repeated/summary.md`、`evidence_doctor.md` |
| decision | `adopted-by-user / affine-index-fast-path-production-probe`。 | 用户确认当前有收益实现可以接入；Phase 062 已刷新长期文档、matrix 和 roadmap。只覆盖 step=1 contiguous indices / correspondences，不外推到 stride / reverse / shuffle、非法 index / correspondence、custom layout 或 `Scalar=double`。 |

## Phase 063/064/065/066/067/068/069/070/071 scalar-double 摘要

| evidence | result | path / notes |
| --- | --- | --- |
| Phase 063 correctness | ordered `PointXYZ -> PointXYZ` 的 scalar double accumulation 和 RVV f64 widened scout 对齐公开 double fallback。 | `doc/phases/063-scalar-double-diagnostic-scout/result.zh.md` |
| Phase 064 board diagnostic | test-only scalar double fallback vs RVV f64 widened diagnostic，5-run B/A `2.806, 2.851, 2.867, 2.868, 2.863`，median `2.863x`，Doctor `0/0/0`。 | `doc/phases/064-scalar-double-board-scout/result.zh.md`、`log/board/scalar_double_diagnostic_scout_repeated/summary.md` |
| Phase 065 correctness | Std/RVV 各 31 tests passed；新增 `ScalarDoubleProductionProbeMatchesReference` 和 `ScalarDoubleProductionProbeFallbackBoundaries`。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| Phase 065 QEMU / ASM | QEMU production probe Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`；asm 输入可见 f64 widened accumulation 特征。 | `log/qemu/scalar_double_production_probe/evidence_doctor.md` |
| Phase 065 board repeated | 5-run B/A `33.955, 33.664, 33.781, 34.077, 33.792`，median `33.792x`，Doctor `0/0/0`，`max_public_error=9.858780e-14`。 | `log/board/scalar_double_production_probe_repeated/summary.md`、`evidence_doctor.md` |
| Phase 067 correctness | Std/RVV 各 31 tests passed；新增 `ScalarDoubleRowSourceProductionProbeMatchesReference` 和 `ScalarDoubleRowSourceProductionProbeFallbackBoundaries`。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| Phase 067 QEMU / ASM | QEMU row-source production probe Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`；asm 输入覆盖三类 public row-source double label。 | `log/qemu/row_source_scalar_double_production_probe/evidence_doctor.md` |
| Phase 067 board repeated | source-indexed median `20.201x`，dual-indexed median `14.800x`，correspondence median `13.474x`；Doctor `0/1/0`，warning 为 dual-indexed variance。 | `log/board/row_source_scalar_double_production_probe_repeated/summary.md`、`evidence_doctor.md` |
| Phase 068 correctness | Std/RVV 各 31 tests passed；新增 `GenericScalarDoubleOrderedProductionProbeMatchesReference` 和 `GenericScalarDoubleOrderedProductionProbeFallbackBoundaries`。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| Phase 068 QEMU / ASM | QEMU generic ordered double probe Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`；5 个 label 命中 `public-generic-double-rvv-f64-widened-probe`。 | `log/qemu/generic_scalar_double_ordered_production_probe/evidence_doctor.md` |
| Phase 068 board repeated | 5 个 ordered common PCL xyz AoS double case 全 positive，median B/A `24.111x` 到 `32.493x`；Doctor `0/1/0`，warning 为 `PointNormal->PointXYZRGB` group-outlier。 | `log/board/generic_scalar_double_ordered_production_probe_repeated/summary.md`、`evidence_doctor.md` |
| Phase 069 correctness | Std/RVV 各 33 tests passed；新增 `RowSourceGenericScalarDoubleProductionProbeMatchesReference` 和 `RowSourceGenericScalarDoubleProductionProbeFallbackBoundaries`。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| Phase 069 QEMU / ASM | QEMU row-source generic double probe Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`；9 个 label 命中 row-source generic double RVV path。 | `log/qemu/row_source_generic_scalar_double_production_probe/evidence_doctor.md` |
| Phase 069 board repeated | 9 个 row-source common PCL xyz AoS double case 全 positive，median B/A `11.309x` 到 `17.433x`；Doctor `0/4/0`，warnings 为 long-tail / variance。 | `log/board/row_source_generic_scalar_double_production_probe_repeated/summary.md`、`evidence_doctor.md` |
| Phase 070 correctness | Std/RVV 各 35 tests passed；新增 `CustomLayoutScalarDoubleProductionScoutMatchesReference` 和 `CustomLayoutScalarDoubleProductionScoutFallbackBoundaries`。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| Phase 070 QEMU / ASM | QEMU custom layout double scout Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`；4 个 label 命中 custom layout double RVV path。 | `log/qemu/custom_layout_scalar_double_diagnostic_scout/evidence_doctor.md` |
| Phase 070 board repeated | ordered median `27.497x`，source-indexed median `15.948x`，dual-indexed median `11.017x`，correspondence median `9.966x`；Doctor `0/1/0`，warning 为 dual-indexed long-tail / variance。 | `log/board/custom_layout_scalar_double_diagnostic_scout_repeated/summary.md`、`evidence_doctor.md` |
| Phase 071 correctness | Std/RVV 各 37 tests passed；新增 `MoreCustomLayoutScalarDoubleSamplingMatchesReference` 和 `MoreCustomLayoutScalarDoubleSamplingFallbackBoundaries`。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| Phase 071 QEMU / ASM | QEMU more custom layout double sampling Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`；12 个 label 命中 more custom layout double RVV path。 | `log/qemu/more_custom_layout_scalar_double_sampling/evidence_doctor.md` |
| Phase 071 board repeated | 12 个 board case 全 positive，median B/A `2.766x` 到 `29.087x`；Doctor `0/8/0`，warnings 为 long-tail / variance 和 group-outlier。 | `log/board/more_custom_layout_scalar_double_sampling_repeated/summary.md`、`evidence_doctor.md` |
| Phase 075 rollback correctness | Std/RVV 各 38 tests passed；`CorrespondenceScalarDoubleGatherProductionPathMatchesReference` 覆盖 shuffled correspondence double 当前 D64 gather path。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| Phase 072 QEMU / ASM | QEMU correspondence sorted-copy scalar-double probe Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`；2 个 label 命中 public double correspondence sorted-copy RVV path。 | `log/qemu/correspondence_sorted_copy_scalar_double_production_probe/evidence_doctor.md` |
| Phase 072 board repeated | 64K / 256K 两个 board case 全 positive，median B/A `5.727x` / `5.408x`；Doctor `0/0/0`。 | `log/board/correspondence_sorted_copy_scalar_double_production_probe_repeated/summary.md`、`evidence_doctor.md` |
| Phase 073 QEMU detail A/B | QEMU smoke paired labels 可解析，64K / 256K B/A `0.736x` / `0.809x`，checksum match；Doctor `2/0/0`，仅作 build / path / log-shape 证据。 | `log/qemu/correspondence_sorted_copy_scalar_double_detail_ab/summary.md`、`evidence_doctor.md` |
| Phase 073 board detail A/B | 同一 production detail boundary 内，sorted-copy double 相对 D64 gather RVV baseline 为 negative：64K / 256K median B/A `0.283x` / `0.431x`；Doctor `2/1/0`。 | `log/board/correspondence_sorted_copy_scalar_double_detail_ab_repeated/summary.md`、`evidence_doctor.md` |
| decision | `adopted-by-user / scalar-double-production-probe`；`adopted-by-user / row-source-scalar-double-production-probe`；`adopted-by-user / generic-scalar-double-ordered-production-probe`；Phase 069 / Phase 070 / Phase 071 均为 `adopted-by-user`；Phase 072 为 historical public-positive，Phase 073 为 detail A/B negative，Phase 075 为 `rolled_back_no_production_for_scalar_double_sorted_copy`。 | Phase 074 已根据用户确认把 Phase 069 / 070 / 071 收口为 adopted。Phase 072 只证明 public RVV 快于 public scalar fallback，Phase 073 已证明 sorted-copy double 慢于既有 D64 gather RVV family，Phase 075 已回滚该分支。当前 double 采纳覆盖 exact `PointXYZ -> PointXYZ` 的 ordered / row-source public overload、ordered common PCL xyz AoS whitelist、row-source common PCL xyz AoS whitelist、一个 custom layout sample 和更多 custom layout sampling。更广 custom layout double、sorted-copy double family selection 和任意自定义点型全集仍未覆盖。 |

## 测试计划和 bench 计划

| 测试 / target | 层级 | 作用 |
| --- | --- | --- |
| `run_test_compare` | QEMU correctness | Std/RVV 构建都通过；当前 38 tests 覆盖 public production path、fallback boundaries、ordered / row-source / ordered generic / row-source generic / custom layout / more custom layout scalar-double production probe、correspondence sorted-copy scalar-double production probe、代表泛型点型、更多常见 PCL xyz AoS 点型、custom layout sampling、custom layout padding / alignment sensitivity、affine contiguous diagnostic / production branch、row-source public path、correspondence sorted-copy production probe、row-source generic representative path、row-source more-generic representative path、row-source all-more-generic full matrix、shuffle-sorted-copy / target-sorted correctness guard 和 test-only candidate。 |
| `run_bench_compare` with `--case-filter public-scale` | QEMU smoke | 检查 production public case label、checksum 和 summary 可解析，不作为性能结论。 |
| `run_bench_compare` with `--case-filter matrix-local-scale` | QEMU smoke | 检查 matrix-local formula shape case label、checksum 和 summary 可解析，不作为性能结论。 |
| `run_bench_generic_xyz_point_types_public_smoke` | QEMU smoke | 检查 generic public case label、checksum 和 summary 可解析，不作为性能结论。 |
| `record_qemu_more_generic_public_state` | QEMU smoke | 检查更多常见 PCL xyz AoS 点型 public case label、checksum、manifest 和 Evidence Doctor；不作为性能结论。 |
| `run_board_bench_more_generic_xyz_aos_point_types_public_repeated` | board performance production public more-generic | repeated board summary 支撑 Phase 052 新增点型 board evidence。 |
| `dump_bench_rvv` | asm attribution | production ordered overload 路径可见 RVV load/FMA/reduction 指令。 |
| `run_board_bench_production_public_scale_repeated` | board performance production direct | repeated board summary 支撑 PI5 用户采纳判断。 |
| `run_board_bench_matrix_local_scale_repeated` | board implementation-shape diagnostic | repeated board summary 只支撑较小 patch 备选线索，不支撑当前 production adoption。 |
| `run_board_bench_generic_xyz_point_types_public_repeated` | board performance production public generic | repeated board summary 支撑代表泛型点型 public path 结论。 |
| `record_qemu_row_source_state` | QEMU smoke row-source public | row-source case label、checksum、max_reference_error 和 doctor 输入。 |
| `run_board_bench_row_source_scale_repeated` | board performance production public row-source | repeated board summary 支撑 Phase 043 用户确认判断。 |
| `record_qemu_row_source_generic_state` | QEMU smoke row-source generic public | 代表点型 row-source case label、checksum、max_reference_error 和 doctor 输入。 |
| `run_board_bench_row_source_generic_xyz_point_types_repeated` | board performance production public row-source generic | repeated board summary 支撑 Phase 044 代表点型同边界结论。 |
| `record_qemu_row_source_more_generic_state` | QEMU smoke row-source more-generic public | 更多常见 PCL xyz AoS row-source 代表组合 case label、checksum、max_reference_error 和 doctor 输入。 |
| `run_board_bench_row_source_more_generic_xyz_aos_point_types_repeated` | board performance production public row-source more-generic | repeated board summary 支撑 Phase 053 代表组合同边界结论。 |
| `record_qemu_row_source_all_more_generic_state` | QEMU smoke row-source all-more-generic public | Phase 051 五个点型组合 × 三类 row source 的 case label、checksum、max_reference_error 和 doctor 输入。 |
| `run_board_bench_row_source_all_more_generic_xyz_aos_matrix_repeated` | board performance production public row-source all-more-generic | repeated board summary 支撑 Phase 054 全交叉同边界结论。 |
| `record_qemu_custom_row_source_large_variance_profile_state` | QEMU smoke custom row-source profile | 两个 custom layout 样本的 256K dual-indexed / correspondence × order pattern case label、max_reference_error 和 doctor 输入。 |
| `run_board_bench_custom_row_source_large_variance_profile_repeated` | board performance production public custom row-source profile | repeated board summary 支撑 Phase 056 controlled profile 结论。 |
| `record_qemu_custom_layout_padding_sensitivity_state` | QEMU smoke custom layout padding sensitivity | compact-ish / huge-padding 两组 custom layout × row source × 64K/256K 的 case label、max_reference_error 和 doctor 输入。 |
| `run_board_bench_custom_layout_padding_sensitivity_repeated` | board performance production public custom layout padding sensitivity | repeated board summary 支撑 Phase 057 padding sensitivity 结论。 |
| `record_qemu_custom_layout_alignment_sensitivity_state` | QEMU smoke custom layout alignment sensitivity | alignas custom layout × row source × 64K/256K 的 case label、max_reference_error 和 doctor 输入。 |
| `run_board_bench_custom_layout_alignment_sensitivity_repeated` | board performance production public custom layout alignment sensitivity | repeated board summary 支撑 Phase 059 alignment sensitivity 结论。 |
| `record_qemu_row_source_locality_order_profile_state` | QEMU smoke row-source locality profile | 36 个 row-source order-pattern case label、checksum、max_reference_error 和 doctor 输入。 |
| `run_board_bench_row_source_locality_order_profile_repeated` | board performance production public row-source profile | repeated board summary 支撑 Phase 045 locality / order sensitivity 解释。 |
| `record_qemu_affine_index_fast_path_production_probe_state` | QEMU smoke affine index fast-path production probe | 6 个 public probe case label、checksum、max_reference_error 和 Doctor 输入；QEMU timing 不作为性能结论。 |
| `run_board_bench_affine_index_fast_path_production_probe_repeated` | board performance production public affine index fast path | repeated board summary 支撑 Phase 061/062 adopted 判断。 |
| `record_qemu_scalar_double_production_probe_state` | QEMU smoke scalar-double production probe | 1 个 ordered public double probe label、checksum、max_public_error、ASM 输入和 Doctor；QEMU timing 不作为性能结论。 |
| `run_board_bench_scalar_double_production_probe_repeated` | board performance production public scalar-double | repeated board summary 支撑 Phase 065/066 adopted 判断。 |
| `record_qemu_row_source_scalar_double_production_probe_state` | QEMU smoke row-source scalar-double production probe | 3 个 row-source public double probe label、checksum、max_public_error、ASM 输入和 Doctor；QEMU timing 不作为性能结论。 |
| `run_board_bench_row_source_scalar_double_production_probe_repeated` | board performance production public row-source scalar-double | repeated board summary 支撑 Phase 067 adopted 判断。 |
| `record_qemu_generic_scalar_double_ordered_production_probe_state` | QEMU smoke ordered generic scalar-double production probe | 5 个 ordered public generic double probe label、checksum、max_public_error、ASM 输入和 Doctor；QEMU timing 不作为性能结论。 |
| `run_board_bench_generic_scalar_double_ordered_production_probe_repeated` | board performance production public ordered generic scalar-double | repeated board summary 支撑 Phase 068 adopted 判断。 |
| `record_qemu_row_source_generic_scalar_double_production_probe_state` | QEMU smoke row-source generic scalar-double production probe | 9 个 row-source public generic double probe label、checksum、max_public_error、ASM 输入和 Doctor；QEMU timing 不作为性能结论。 |
| `run_board_bench_row_source_generic_scalar_double_production_probe_repeated` | board performance production public row-source generic scalar-double | repeated board summary 支撑 Phase 069 adopted-by-user。 |
| `record_qemu_custom_layout_scalar_double_diagnostic_scout_state` | QEMU smoke custom layout scalar-double public scout | 4 个 custom layout double public label、checksum、max_public_error、ASM 输入和 Doctor；QEMU timing 不作为性能结论。 |
| `run_board_bench_custom_layout_scalar_double_diagnostic_scout_repeated` | board performance production public custom layout scalar-double scout | repeated board summary 支撑 Phase 070 adopted-by-user。 |
| `record_qemu_more_custom_layout_scalar_double_sampling_state` | QEMU smoke more custom layout scalar-double public sampling | 12 个 more custom layout double public label、checksum、max_public_error、ASM 输入和 Doctor；QEMU timing 不作为性能结论。 |
| `run_board_bench_more_custom_layout_scalar_double_sampling_repeated` | board performance production public more custom layout scalar-double sampling | repeated board summary 支撑 Phase 071 adopted-by-user。 |
| `record_qemu_correspondence_sorted_copy_scalar_double_production_probe_state` | QEMU smoke correspondence sorted-copy scalar-double production probe | production-public probe case label、checksum、max_reference_error 和 doctor 输入；QEMU timing 不作为性能证据。 |
| `run_board_bench_correspondence_sorted_copy_scalar_double_production_probe_repeated` | board performance production public correspondence sorted-copy scalar-double probe | repeated board summary 支撑 Phase 072 bounded public positive；Phase 073 detail A/B 已证明它不支持 clean adoption。 |
| `record_qemu_correspondence_sorted_copy_scalar_double_detail_ab_state` | QEMU smoke correspondence sorted-copy scalar-double detail A/B | paired D64 gather / sorted-copy double labels、checksum、max_reference_error 和 doctor 输入；QEMU timing 不作为性能证据。 |
| `run_board_bench_correspondence_sorted_copy_scalar_double_detail_ab_repeated` | board performance production-detail RVV-vs-RVV correspondence sorted-copy scalar-double A/B | repeated board summary 支撑 Phase 073 negative family-selection 判断。 |
| `record_qemu_row_source_shuffle_sorted_copy_detail_ab_state` | QEMU smoke row-source shuffle detail A/B | paired current/sorted-copy label、checksum、max_reference_error 和 doctor 输入。 |
| `run_board_bench_row_source_shuffle_sorted_copy_detail_ab_repeated` | board performance shuffle detail A/B | repeated board summary 支撑 Phase 046 correspondence positive / dual-indexed mixed / 4K rejected。 |
| `record_qemu_correspondence_sorted_copy_production_probe_state` | QEMU smoke correspondence sorted-copy production probe | production probe case label、checksum、max_reference_error 和 doctor 输入。 |
| `run_board_bench_correspondence_sorted_copy_production_probe_repeated` | board performance production public correspondence sorted-copy probe | repeated board summary 支撑 Phase 047 adopted 判断。 |
| `record_qemu_row_source_staged_selected_cloud_detail_ab_state` | QEMU smoke staged-selected-cloud detail A/B | paired current/staged-selected-cloud label、checksum、max_reference_error 和 doctor 输入。 |
| `run_board_bench_row_source_shuffle_staged_selected_cloud_detail_ab_repeated` | board performance staged-selected-cloud detail A/B | repeated board summary 支撑 Phase 048 negative 结论。 |
| `record_qemu_row_source_target_sorted_detail_ab_state` | QEMU smoke target-sorted detail A/B | paired current/target-sorted label、checksum、max_reference_error 和 doctor 输入。 |
| `run_board_bench_row_source_shuffle_dual_indexed_target_sorted_detail_ab_repeated` | board performance target-sorted detail A/B | repeated board summary 支撑 Phase 049 negative / mixed 结论。 |
| `record_qemu_dual_indexed_256k_sorted_copy_stability_state` | QEMU smoke dual-indexed 256K sorted-copy stability | paired current/source-sorted-copy label、checksum、max_reference_error 和 doctor 输入。 |
| `run_board_bench_dual_indexed_256k_sorted_copy_stability_repeated` | board performance dual-indexed 256K sorted-copy stability | 10-run repeated board summary 支撑 Phase 058 rejected / unstable 结论。 |

## 生产接入判断

当前 ordered 判断是 adopted，但它分成几组各自独立的 production facts。

| 路线 | 当前状态 | 说明 |
| --- | --- | --- |
| ordered `Scalar=float` | adopted | 已同步 production 长期主题文档。 |
| ordered exact `PointXYZ -> PointXYZ` / `Scalar=double` | adopted | Phase 066 收口。 |
| ordered common PCL xyz AoS whitelist / `Scalar=double` | adopted | Phase 068 收口。 |
| row-source exact `PointXYZ -> PointXYZ` / `Scalar=double` | adopted | Phase 067 收口。 |
| row-source common PCL xyz AoS whitelist / `Scalar=double` | adopted-by-user | Phase 074 收口。 |
| contiguous affine fast path | adopted | Phase 061 / 062 收口。 |
| custom layout double sampling | adopted-by-user | Phase 070 / 071 / 074 收口。 |
| matrix-local helper simplification | adopted | Phase 050 收口。 |
| correspondence sorted-copy `Scalar=double` | rolled_back / no-production | Phase 072 public positive，但 Phase 073 family selection negative，Phase 075 回滚。 |
| staged-selected-cloud / target-sorted / dual-indexed 256K source-sorted-copy | rejected / unstable | Phase 048 / 049 / 058 关闭。 |

Phase 041 已完成代表泛型点型 board / ASM 闭环，Phase 051 / 052 又补更多常见 PCL xyz AoS 点型 correctness / QEMU smoke / board repeated。

Phase 044、053 和 054 继续扩大 row-source 泛型边界。Phase 055 / 056 / 057 / 059 只补 custom layout 取样、256K row-source profile、padding sensitivity 和 alignment sensitivity，不改变 production gate。

Phase 045 进一步说明 row-source public path 对 locality / order pattern 敏感。

后续不能把任何一条路线外推到这些范围：

- 4K 或 dual-indexed sorted-copy。
- 非法 correspondence。
- 全部自定义泛型点型或异常 alignment 全集。
- stride / reverse / shuffle affine fast path。
- broader custom layout double 或 sorted-copy double family selection。
