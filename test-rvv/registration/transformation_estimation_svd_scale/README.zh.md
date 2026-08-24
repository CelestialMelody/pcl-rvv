# transformation_estimation_svd_scale RVV 主题入口

本目录保存 `registration/transformation_estimation_svd_scale` 的 test-rvv（RVV 测试资产）、diagnostic（诊断）和阶段文档。目标 production 源码是：

- `registration/include/pcl/registration/transformation_estimation_svd_scale.h`
- `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp`

`tesvd_scale` 是本 topic 的短标识。

## 当前结论

收尾判断：Phase 075 已关闭同范围优化，当前没有值得继续推进的 positive same-scope route。后续若要继续，只能另起新 scope，重新定义输入分布、family selection 或 board budget；当前 topic 进入文档收口和提交准备。

| 结论项 | 当前状态 | 读者应如何理解 |
| --- | --- | --- |
| 已采纳主线 | ordered direct fused、row-source、affine fast-path、`Scalar=double` ordered / row-source / generic、custom layout double 取样、`matrix-local-scale-simplification` | 这些路线已经进入长期事实，README 只负责给读者定位。 |
| 已回滚分支 | correspondence sorted-copy `Scalar=double` | Phase 072 public-positive 只是历史事实；Phase 073 family-selection negative，Phase 075 已回滚。 |
| 已关闭负向路线 | staged-selected-cloud、target-sorted、dual-indexed 256K source-sorted-copy | 这些路线不再是当前可继续推进方向。 |
| 当前 topic 状态 | stop / closeout | 后续工作仅剩文档收尾、提交边界和必要同步。 |

README 只给恢复导航。完整阶段审计在 `doc/transformation_estimation_svd_scale-evaluation.zh.md`，长期生产事实在 `doc-rvv/registration/transformation_estimation_svd_scale-RVV.zh.md`。

### 当前 adopted 生产范围

| 范围 | 状态 | 关键说明 |
| --- | --- | --- |
| ordered-cloud-pair / `Scalar=float` | adopted | Phase 031 已收口；production direct board 4K/64K/256K median B/A 为 `26.123x` / `33.860x` / `33.066x`。 |
| source-indexed / dual-indexed / correspondence / `Scalar=float` | adopted | Phase 043 已收口；9 个 row-source board case 全 positive。 |
| correspondence sorted-copy / `Scalar=float` | adopted | Phase 047 已收口；只覆盖 correspondence、size >= 64K、shuffle-like disorder。 |
| contiguous affine index fast path | adopted | Phase 061 / 062 已收口；只覆盖 step=1 contiguous indices / correspondences。 |
| `Scalar=double` ordered / row-source / generic | adopted | Phase 066 / 067 / 068 / 069 已收口；边界按 exact `PointXYZ`、common PCL xyz AoS whitelist 和 row-source 分开记录。 |
| custom layout double sampling | adopted sampled production behavior | Phase 070 / 071 已由 Phase 074 收口；只覆盖测试本地 custom layout samples。 |
| `matrix-local-scale-simplification` | adopted helper simplification | Phase 050 已收口；它不是新的 RVV intrinsic family。 |

### 最近 closeout

| phase | 当前结论 | 板卡证据 |
| --- | --- | --- |
| Phase 069 | row-source common PCL xyz AoS `Scalar=double` adopted | 9/9 positive，median B/A `11.309x` 到 `17.433x`，Doctor `0/4/0`。 |
| Phase 070 | 一个 custom layout double sample adopted | 4/4 positive，median B/A `9.966x` 到 `27.497x`，Doctor `0/1/0`。 |
| Phase 071 | 更多 custom layout double sampling adopted | 12/12 positive，median B/A `2.766x` 到 `29.087x`，Doctor `0/8/0`。 |
| Phase 072 | sorted-copy double historical public-positive | public Std/RVV 64K / 256K median B/A `5.727x` / `5.408x`，Doctor `0/0/0`。 |
| Phase 073 | sorted-copy double family selection negative | 同边界 RVV-vs-RVV median B/A `0.283x` / `0.431x`，Doctor `2/1/0`。 |
| Phase 075 | sorted-copy double rollback | 当前 double correspondence 使用 D64 gather，Phase 047 float sorted-copy 不受影响。 |

### 已关闭或只作证据扩展的路线

| 路线 | 结论 | 看哪里 |
| --- | --- | --- |
| more generic point types | 扩大 evidence boundary，不新增 production gate。 | Phase 041 / 044 / 051 / 052 / 053 / 054。 |
| custom layout / padding / alignment sampling | sampled positive with warnings，不外推到全部 custom layout。 | Phase 055 / 056 / 057 / 059。 |
| row-source locality / order profile | 解释 locality sensitivity，不新增 optimization family。 | Phase 045。 |
| staged-selected-cloud | negative，不进入 production。 | Phase 048。 |
| dual-indexed target-sorted | negative / mixed，不进入 production。 | Phase 049。 |
| dual-indexed 256K source-sorted-copy | rejected / unstable，不进入 production probe。 | Phase 058。 |

### 当前未覆盖范围

- 全部自定义点型、全部 custom layout double、packed unaligned float 或异常 alignment 全集。
- 非 dense 输入、小规模输入、退化 source variance。
- 非法 index / correspondence。
- stride / reverse / shuffle affine fast path。
- sorted-copy double family selection 的其它输入分布。

## 先读哪份文档

| 读者问题 | 首选入口 | 说明 |
| --- | --- | --- |
| 为什么这个 topic 值得做 | `doc/transformation_estimation_svd_scale-evaluation.zh.md` | 标量路径、候选边界和生产接入判断主归属。 |
| 当前 phase 状态是什么 | `doc/phases/README.zh.md` | 当前恢复入口、阶段表、phase result 和停止条件。 |
| PI5 历史确认点是什么 | `doc/phases/030-pi5-user-confirmation-packet/result.zh.md` | ordered production diff、证据、Evidence Doctor warning、风险和用户选项的历史记录。 |
| 采纳后怎么收口 | `doc/phases/031-adoption-closeout-plan/result.zh.md` | adoption closeout 事实记录；当前 adopted 状态主归属。 |
| 泛型点型扩展推进到哪里 | `doc/phases/040-generic-point-type-expansion/result.zh.md`、`doc/phases/041-generic-point-type-public-board-asm/result.zh.md`、`doc/phases/051-more-generic-xyz-aos-point-types/result.zh.md`、`doc/phases/052-more-generic-xyz-aos-board/result.zh.md`、`doc/phases/053-row-source-more-generic-xyz-aos-point-types/result.zh.md`、`doc/phases/054-row-source-all-more-generic-xyz-aos-matrix/result.zh.md` | representative generic public board / ASM 已完成；Phase 051 又补更多常见 PCL xyz AoS 点型 correctness / QEMU smoke，Phase 052 已补这些点型的 64K ordered board repeated，Phase 053 已补 3 个 row-source more-generic 代表组合，Phase 054 已补 5 个点型组合 × 3 类 row source 全交叉 evidence。 |
| row-source 扩展推进到哪里 | `doc/phases/043-row-source-expansion/result.zh.md`、`doc/phases/044-row-source-generic-xyz-point-type-expansion/result.zh.md`、`doc/phases/053-row-source-more-generic-xyz-aos-point-types/result.zh.md`、`doc/phases/054-row-source-all-more-generic-xyz-aos-matrix/result.zh.md`、`doc/phases/045-row-source-locality-order-profile/result.zh.md`、`doc/phases/046-row-source-shuffle-mitigation-detail-ab/result.zh.md`、`doc/phases/047-correspondence-sorted-copy-production-probe/result.zh.md`、`doc/phases/048-row-source-shuffle-staged-selected-cloud-detail-ab/result.zh.md`、`doc/phases/049-dual-indexed-target-sorted-detail-ab/result.zh.md`、`doc/phases/058-dual-indexed-256k-sorted-copy-stability/result.zh.md`、`doc/phases/060-affine-index-fast-path-detail-ab/result.zh.md`、`doc/phases/061-affine-index-fast-path-production-probe/result.zh.md`、`doc/phases/062-affine-index-fast-path-adoption-closeout/result.zh.md` | Phase 043 已采纳 `PointXYZ -> PointXYZ` row-source public path；Phase 044 已补代表泛型点型 row-source board 证据；Phase 053 已补更多常见 PCL xyz AoS 的 3 个 row-source 代表组合；Phase 054 已补 Phase 051 五个点型组合的 row-source 全交叉 evidence；Phase 045 已解释 locality / order sensitivity；Phase 046 发现 correspondence 64K/256K sorted-copy 正向子边界；Phase 047 已将其有界接入；Phase 048 排除了 staged-selected-cloud；Phase 049 排除了 dual-indexed target-sorted；Phase 058 排除了 dual-indexed 256K source-sorted-copy production probe；Phase 060 新增 contiguous affine index fast-path positive probe candidate；Phase 061/062 已完成 production public probe 并由用户确认采纳。 |
| custom layout 采样推进到哪里 | `doc/phases/055-custom-xyz-aos-layout-sampling/result.zh.md`、`doc/phases/056-custom-row-source-large-variance-profile/result.zh.md`、`doc/phases/057-custom-layout-padding-sensitivity/result.zh.md`、`doc/phases/059-custom-layout-alignment-sensitivity/result.zh.md` | Phase 055 证明两个 custom layout 样本 correctness / QEMU smoke clean，ordered 和多数 row-source positive，但 256K row-source mixed；Phase 056 用受控 order pattern 复核 256K dual-indexed / correspondence，8 个 board case 全 positive但有 8 个 variance / group-outlier warnings；Phase 057 新增 compact-ish / huge-padding 两组 layout，12 个 board case 全 positive但有 15 个 padding / group-outlier warnings；Phase 059 新增一个 alignas layout 采样，6 个 board case 全 positive但有 3 个 variance warnings。 |
| matrix-local helper 简化如何收口 | `doc/phases/050-matrix-local-adoption-closeout/result.zh.md` | Phase 050 已把 `trace(R * H)` helper 简化收口为 adopted production helper simplification。 |
| 历史回滚草案在哪里 | `doc/phases/032-rollback-no-production-plan/plan.zh.md` | rollback/no-production 草案；Phase 043 已采纳后只作为历史备用计划。 |
| `Scalar=double` 接入推进到哪里 | `doc/phases/063-scalar-double-diagnostic-scout/result.zh.md`、`doc/phases/064-scalar-double-board-scout/result.zh.md`、`doc/phases/065-scalar-double-production-probe/result.zh.md`、`doc/phases/066-scalar-double-adoption-closeout/result.zh.md`、`doc/phases/067-row-source-scalar-double-production-probe/result.zh.md`、`doc/phases/068-generic-scalar-double-ordered-production-probe/result.zh.md`、`doc/phases/069-row-source-generic-scalar-double-production-probe/result.zh.md`、`doc/phases/070-custom-layout-scalar-double-diagnostic-scout/result.zh.md`、`doc/phases/071-more-custom-layout-scalar-double-sampling/result.zh.md`、`doc/phases/072-correspondence-sorted-copy-scalar-double-production-probe/result.zh.md`、`doc/phases/073-correspondence-sorted-copy-scalar-double-detail-ab/result.zh.md`、`doc/phases/075-correspondence-sorted-copy-scalar-double-rollback-closeout/result.zh.md` | Phase 063/064 是 diagnostic；Phase 065 是 ordered exact `PointXYZ` 真实 public production probe；Phase 066 是 ordered exact `PointXYZ` 用户确认后的 adopted closeout；Phase 067 是三类 row-source exact `PointXYZ` double public overload 的 adopted production probe；Phase 068 是 ordered common PCL xyz AoS double public overload 的 adopted production probe；Phase 069 是 row-source common PCL xyz AoS double adopted production probe；Phase 070 / 071 是 custom layout double adopted candidate / sampling reinforcement；Phase 072 是 correspondence sorted-copy double public positive 历史探针，Phase 073 已证明同边界 detail A/B 为 negative，Phase 075 已回滚 sorted-copy double 生产分流并保留 D64 gather。 |
| 后续还能尝试什么 | `doc/optimization-roadmap.zh.md` | 跨 phase 候选和恢复队列。 |
| 证据矩阵状态 | `doc/phases/optimization-matrix.zh.md` | candidate × row source × point type × evidence 状态。 |
| 测试和 bench 如何运行 | `doc/testing-overview.zh.md`、`doc/benchmark-and-evidence.zh.md` | target、case-filter、QEMU / board 边界。 |
| helper 在哪里 | `doc/test-support-code-map.zh.md` | 聚合入口、internal helper、bench、script 和 production 对照。 |

## 目录分工

| 目录 / 文件 | 主职责 | 提交边界 |
| --- | --- | --- |
| `include/tesvd_scale.h` | test support 聚合入口 | topic-local test asset，review-required。 |
| `include/impl/tesvd_scale_support.hpp` | fixture、样本构造、统计结构 | topic-local test asset。 |
| `include/impl/tesvd_scale_candidates.hpp` | 标量 reference 与 RVV candidate | topic-local test asset。 |
| `src/test_tesvd_scale.cpp` | correctness（正确性）gtest | topic-local test asset。 |
| `src/bench_tesvd_scale.cpp` | QEMU smoke 和 board bench wrapper | topic-local test asset；QEMU timing 不作为性能结论。 |
| `script/` | topic-local summary / manifest wrapper | topic-local script。 |
| `doc/` | evaluation、测试说明、bench 证据、代码地图 | topic-local documentation，review-required；命名继续服从 `artifact_layout` 和当前 topic role/path index。 |
| `log/qemu/`、`log/board/`、`build/` | 可再生成输出和构建产物 | 默认 local-only。 |

## 常用命令

| 命令 | 证据角色 |
| --- | --- |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare` | QEMU correctness；当前 Std/RVV 两侧 31 tests 必须通过。 |
| `ALLOW_QEMU_BENCH_COMPARE=1 make -C test-rvv/registration/transformation_estimation_svd_scale run_bench_compare BENCH_ARGS="--case-filter ordered-cloud-pair --iterations 3 --warmup-iterations 1"` | QEMU bench smoke，只看日志形状、数值误差和 manifest。 |
| `ALLOW_QEMU_BENCH_COMPARE=1 make -C test-rvv/registration/transformation_estimation_svd_scale run_bench_compare BENCH_ARGS="--case-filter public-scale --iterations 3 --warmup-iterations 1"` | QEMU production public-scale smoke，只看日志形状和 manifest。 |
| `ALLOW_QEMU_BENCH_COMPARE=1 make -C test-rvv/registration/transformation_estimation_svd_scale run_bench_compare BENCH_ARGS="--case-filter matrix-local-scale --iterations 3 --warmup-iterations 1"` | QEMU matrix-local smoke，只看局部公式 A/B 日志形状和 manifest。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale dump_bench_rvv` | 反汇编归因输入。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_test_smoke` | 板卡 correctness smoke。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_ordered_cloud_pair_repeated` | 板卡 repeated diagnostic，生成 summary / manifest / Evidence Doctor。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_production_public_scale_repeated` | 板卡 repeated production direct，生成 summary / manifest / Evidence Doctor。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_matrix_local_scale_repeated` | 板卡 repeated implementation-shape diagnostic，生成 matrix-local summary / manifest / Evidence Doctor。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_generic_xyz_point_types_public_repeated` | 板卡 repeated production public generic，生成代表点型 summary / manifest / Evidence Doctor。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_more_generic_public_state` | QEMU more-generic public smoke，只看更多常见 PCL xyz AoS 点型 label、manifest 和 Evidence Doctor；QEMU timing 不作为性能结论。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_more_generic_xyz_aos_point_types_public_repeated` | 板卡 repeated production public more-generic，生成 Phase 052 新增点型 summary / manifest / Evidence Doctor。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_row_source_state` | QEMU row-source smoke，只看 case-filter、误差和 manifest。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_row_source_scale_repeated` | 板卡 repeated production public row-source，生成 source-indexed / dual-indexed / correspondence summary / manifest / Evidence Doctor。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_row_source_generic_state` | QEMU row-source generic smoke，只看代表点型 case-filter、误差和 manifest。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_row_source_generic_xyz_point_types_repeated` | 板卡 repeated production public row-source generic，生成代表点型 summary / manifest / Evidence Doctor。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_row_source_more_generic_state` | QEMU row-source more-generic smoke，只看更多常见 PCL xyz AoS 代表组合、误差和 manifest；QEMU timing 不作为性能结论。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_row_source_more_generic_xyz_aos_point_types_repeated` | 板卡 repeated production public row-source more-generic，生成 Phase 053 summary / manifest / Evidence Doctor。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_row_source_all_more_generic_state` | QEMU row-source all-more-generic smoke，只看 Phase 051 五个点型组合 × 三类 row source 的 label、误差和 manifest；QEMU timing 不作为性能结论。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_row_source_all_more_generic_xyz_aos_matrix_repeated` | 板卡 repeated production public row-source all-more-generic，生成 Phase 054 summary / manifest / Evidence Doctor。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_row_source_locality_order_profile_state` | QEMU row-source locality / order profile smoke，只看 36 个 case label、误差和 manifest。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_row_source_locality_order_profile_repeated` | 板卡 repeated production public row-source profile，生成 36 个 order-pattern summary / manifest / Evidence Doctor。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_affine_index_fast_path_detail_ab_state` | QEMU affine index fast-path detail A/B smoke，只看 paired labels、误差和 manifest；QEMU timing 不作为性能结论。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_affine_index_fast_path_detail_ab_repeated` | 板卡 RVV-vs-RVV affine contiguous fast-path detail A/B，当前为 positive production probe candidate。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_affine_index_fast_path_production_probe_state` | QEMU affine index fast-path production probe smoke，只看 public labels、误差和 manifest；QEMU timing 不作为性能结论。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_affine_index_fast_path_production_probe_repeated` | 板卡 repeated production public affine index fast-path probe；用户确认后已采纳。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_scalar_double_production_probe_state` | QEMU scalar-double production probe smoke，只看 public double label、误差、manifest、ASM 输入和 Evidence Doctor；QEMU timing 不作为性能结论。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_scalar_double_production_probe_repeated` | 板卡 repeated production public scalar-double probe；Phase 066 用户确认后已采纳 ordered `PointXYZ -> PointXYZ` / `Scalar=double` 窄分支。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_row_source_scalar_double_production_probe_state` | QEMU row-source scalar-double production probe smoke，只看三类 row-source public double label、误差、manifest、ASM 输入和 Evidence Doctor；QEMU timing 不作为性能结论。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_row_source_scalar_double_production_probe_repeated` | 板卡 repeated production public row-source scalar-double probe；Phase 067 用户确认后已采纳 exact `PointXYZ -> PointXYZ` / `Scalar=double` 的三类 row-source 窄分支。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_generic_scalar_double_ordered_production_probe_state` | QEMU ordered generic scalar-double production probe smoke，只看 common PCL xyz AoS public double label、误差、manifest 和 Evidence Doctor；QEMU timing 不作为性能结论。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_generic_scalar_double_ordered_production_probe_repeated` | 板卡 repeated production public ordered generic scalar-double probe；Phase 068 用户确认后已采纳 ordered common PCL xyz AoS / `Scalar=double` 分支。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_row_source_generic_scalar_double_production_probe_state` | QEMU row-source generic scalar-double production probe smoke，只看 common PCL xyz AoS row-source public double label、误差、manifest 和 Evidence Doctor；QEMU timing 不作为性能结论。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_row_source_generic_scalar_double_production_probe_repeated` | 板卡 repeated production public row-source generic scalar-double probe；Phase 074 已将 Phase 069 收口为 adopted。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_custom_layout_scalar_double_diagnostic_scout_state` | QEMU custom layout scalar-double scout，只看 custom layout public double label、误差、manifest 和 Evidence Doctor；QEMU timing 不作为性能结论。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_custom_layout_scalar_double_diagnostic_scout_repeated` | 板卡 repeated production-public custom layout scalar-double scout；Phase 074 已将 Phase 070 收口为 adopted。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_row_source_shuffle_sorted_copy_detail_ab_state` | QEMU sorted-copy detail A/B smoke，只看 paired labels、误差和 manifest；QEMU timing 不作为性能结论。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_row_source_shuffle_sorted_copy_detail_ab_repeated` | 板卡 RVV-vs-RVV detail A/B，生成 sorted-copy summary / manifest / Evidence Doctor。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_correspondence_sorted_copy_production_probe_state` | QEMU correspondence sorted-copy production probe smoke，只看 case-filter、误差和 manifest。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_correspondence_sorted_copy_production_probe_repeated` | 板卡 repeated production public correspondence sorted-copy probe，生成 summary / manifest / Evidence Doctor。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_row_source_staged_selected_cloud_detail_ab_state` | QEMU staged-selected-cloud detail A/B smoke，只看 paired labels、误差和 manifest。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_row_source_shuffle_staged_selected_cloud_detail_ab_repeated` | 板卡 RVV-vs-RVV staged-selected-cloud detail A/B，当前为 negative 诊断证据。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_row_source_target_sorted_detail_ab_state` | QEMU dual-indexed target-sorted detail A/B smoke，只看 paired labels、误差和 manifest。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_row_source_shuffle_dual_indexed_target_sorted_detail_ab_repeated` | 板卡 RVV-vs-RVV target-sorted detail A/B，当前为 negative / mixed 诊断证据。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_dual_indexed_256k_sorted_copy_stability_state` | QEMU dual-indexed 256K source-sorted-copy stability smoke，只看 paired labels、误差和 manifest。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_dual_indexed_256k_sorted_copy_stability_repeated` | 板卡 10-run RVV-vs-RVV dual-indexed 256K source-sorted-copy stability 复核，当前为 rejected / unstable 诊断证据。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_custom_row_source_large_variance_profile_state` | QEMU custom layout 256K row-source order profile smoke，只看 8 个 case label、误差和 manifest。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_custom_row_source_large_variance_profile_repeated` | 板卡 repeated production public custom row-source profile，生成 Phase 056 summary / manifest / Evidence Doctor。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_custom_layout_padding_sensitivity_state` | QEMU custom layout padding sensitivity smoke，只看 12 个 case label、误差和 manifest。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_custom_layout_padding_sensitivity_repeated` | 板卡 repeated production public custom layout padding sensitivity，生成 Phase 057 summary / manifest / Evidence Doctor。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_custom_layout_alignment_sensitivity_state` | QEMU custom layout alignment sensitivity smoke，只看 6 个 case label、误差和 manifest。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_custom_layout_alignment_sensitivity_repeated` | 板卡 repeated production public custom layout alignment sensitivity，生成 Phase 059 summary / manifest / Evidence Doctor。 |
| `make -C test-rvv/registration/transformation_estimation_svd_scale evidence_status` | evidence registry 新鲜度检查。 |

## 当前可提交证据

当前完成的 summary evidence（摘要证据）：

- `log/qemu/evidence_doctor.md`：当前裸文件是 Phase 020 matrix-local QEMU smoke manifest，`Errors=0`、`Warnings=0`；Phase 010 public-scale smoke 的 `0/0` 结果见 Phase 010 result / registry。
- `log/board/ordered_cloud_pair_repeated/summary.md`：5-run board repeated diagnostic。
- `log/board/ordered_cloud_pair_repeated/evidence_doctor.md`：`Errors=0`、`Warnings=1`，warning 为 4K `long_tail_or_variance`，但 4K 最小 B/A 仍为 `2.513x`。
- `log/board/production_public_scale_ordered_cloud_pair_repeated/summary.md`：5-run board repeated production direct，4K/64K/256K median B/A = `26.123x` / `33.860x` / `33.066x`。
- `log/board/production_public_scale_ordered_cloud_pair_repeated/evidence_doctor.md`：`Errors=0`、`Warnings=1`，warning 为 4K 与组内 median 偏离；4K 自身仍为 positive。
- `log/board/matrix_local_scale_repeated/summary.md`：5-run board repeated implementation-shape diagnostic，4K/64K/256K median B/A = `1.683x` / `1.173x` / `1.173x`。
- `log/board/matrix_local_scale_repeated/evidence_doctor.md`：`Errors=0`、`Warnings=2`，warning 为 4K long-tail 和 group-outlier；结论边界降级为 implementation-shape weak-positive。
- `log/qemu/generic_xyz_point_types_public/evidence_doctor.md`：generic public QEMU smoke manifest，`Errors=0`、`Warnings=0`、`Suggestions=0`。
- `log/qemu/more_generic_xyz_aos_point_types_public/evidence_doctor.md`：more-generic public QEMU smoke manifest，5 comparisons，`Errors=0`、`Warnings=0`、`Suggestions=0`。
- `log/board/generic_xyz_point_types_public_repeated/summary.md`：5-run board repeated production public generic，8 个代表点型 median B/A 均为 positive。
- `log/board/generic_xyz_point_types_public_repeated/evidence_doctor.md`：`Errors=0`、`Warnings=0`、`Suggestions=0`。
- `log/board/more_generic_xyz_aos_point_types_public_repeated/summary.md`：5-run board repeated production public more-generic，5 个 Phase 051 新增点型 median B/A 均为 positive，范围 `20.555x` 到 `26.989x`。
- `log/board/more_generic_xyz_aos_point_types_public_repeated/evidence_doctor.md`：`Errors=0`、`Warnings=0`、`Suggestions=0`。
- `log/qemu/row_source_scale/evidence_doctor.md`：row-source QEMU smoke manifest，`Errors=0`、`Warnings=0`、`Suggestions=0`。
- `log/board/row_source_scale_repeated/summary.md`：5-run board repeated production public row-source，9 个 row-source case 全部 positive。
- `log/board/row_source_scale_repeated/evidence_doctor.md`：`Errors=0`、`Warnings=7`，warning 来自 correspondence 长尾、dual-indexed 256K 长尾和 source-indexed 组内离群；结论按 row source / size 分开报告。
- `log/qemu/row_source_generic_xyz_point_types/evidence_doctor.md`：row-source generic QEMU smoke manifest，`Errors=0`、`Warnings=0`、`Suggestions=0`。
- `log/board/row_source_generic_xyz_point_types_repeated/summary.md`：5-run board repeated production public row-source generic，9 个代表点型 case 全部 positive。
- `log/board/row_source_generic_xyz_point_types_repeated/evidence_doctor.md`：`Errors=0`、`Warnings=12`，warning 来自 long-tail / variance 和 source-indexed 组内离群；结论按 row source / point type / size 分开报告。
- `log/qemu/row_source_more_generic_xyz_aos_point_types/evidence_doctor.md`：row-source more-generic QEMU smoke manifest，9 comparisons，`Errors=0`、`Warnings=0`、`Suggestions=0`。
- `log/board/row_source_more_generic_xyz_aos_point_types_repeated/summary.md`：5-run board repeated production public row-source more-generic，9 个代表组合 case 全部 positive，median B/A 范围 `7.729x` 到 `12.614x`。
- `log/board/row_source_more_generic_xyz_aos_point_types_repeated/evidence_doctor.md`：`Errors=0`、`Warnings=3`，warning 来自 source-indexed `PointXYZRGBA` 组内高收益离群；结论按 row source / point type / size 分开报告。
- `log/qemu/row_source_all_more_generic_xyz_aos_matrix/evidence_doctor.md`：row-source all-more-generic QEMU smoke manifest，45 comparisons，`Errors=0`、`Warnings=0`、`Suggestions=0`。
- `log/board/row_source_all_more_generic_xyz_aos_matrix_repeated/summary.md`：5-run board repeated production public row-source all-more-generic，45 个全交叉 case 全部 positive，overall median B/A 范围 `7.277x` 到 `12.565x`。
- `log/board/row_source_all_more_generic_xyz_aos_matrix_repeated/evidence_doctor.md`：`Errors=0`、`Warnings=18`，warning 来自 4K 长尾和 source-indexed 组内高收益离群；结论按 row source / point type / size 分开报告。
- `log/qemu/row_source_locality_order_profile/evidence_doctor.md`：row-source locality / order profile QEMU smoke manifest，36 comparisons，`Errors=0`、`Warnings=0`、`Suggestions=0`。
- `log/board/row_source_locality_order_profile_repeated/summary.md`：5-run board repeated production public row-source profile，36 个 order-pattern case 全部 positive；shuffle 明显低于 contiguous / stride / reverse。
- `log/board/row_source_locality_order_profile_repeated/evidence_doctor.md`：`Errors=0`、`Warnings=29`，warning 来自 locality / order sensitivity 和组内离群；结论按 row source / order / size 分开报告。
- `log/qemu/row_source_shuffle_sorted_copy_detail_ab/evidence_doctor.md`：sorted-copy detail A/B QEMU smoke，当前 Errors 来自 QEMU 退化信号；只作为日志形状和 correctness smoke。
- `log/board/row_source_shuffle_sorted_copy_detail_ab_repeated/summary.md`：5-run board RVV-vs-RVV detail A/B；correspondence 64K/256K positive，dual-indexed mixed，4K negative。
- `log/board/row_source_shuffle_sorted_copy_detail_ab_repeated/evidence_doctor.md`：`Errors=2`、`Warnings=8`，Errors 只来自 4K 5/5 退化；结论必须按 row source / size 分开报告。
- `log/qemu/correspondence_sorted_copy_production_probe/evidence_doctor.md`：correspondence sorted-copy production probe QEMU smoke，`Errors=0`、`Warnings=0`、`Suggestions=0`。
- `log/board/correspondence_sorted_copy_production_probe_repeated/summary.md`：5-run board production public correspondence sorted-copy probe，4K/64K/256K median B/A = `8.128x` / `3.929x` / `3.618x`。
- `log/board/correspondence_sorted_copy_production_probe_repeated/evidence_doctor.md`：`Errors=0`、`Warnings=1`，warning 是 4K group-outlier，需要按 size 分开解释。
- `log/qemu/row_source_shuffle_staged_selected_cloud_detail_ab/evidence_doctor.md`：staged-selected-cloud detail A/B QEMU smoke；只作为日志形状和 manifest 检查。
- `log/board/row_source_shuffle_staged_selected_cloud_detail_ab_repeated/summary.md`：5-run board RVV-vs-RVV staged-selected-cloud detail A/B，6 个 case 全部 negative。
- `log/board/row_source_shuffle_staged_selected_cloud_detail_ab_repeated/evidence_doctor.md`：`Errors=6`、`Warnings=9`，支撑该路线按 negative 收束。
- `log/qemu/row_source_shuffle_dual_indexed_target_sorted_detail_ab/evidence_doctor.md`：target-sorted detail A/B QEMU smoke；QEMU timing 不作为性能结论。
- `log/board/row_source_shuffle_dual_indexed_target_sorted_detail_ab_repeated/summary.md`：5-run board RVV-vs-RVV target-sorted detail A/B，64K negative、256K weak-positive，overall negative。
- `log/board/row_source_shuffle_dual_indexed_target_sorted_detail_ab_repeated/evidence_doctor.md`：`Errors=1`、`Warnings=1`，支撑该路线不进入 production。
- `log/qemu/row_source_shuffle_dual_indexed_256k_sorted_copy_stability/evidence_doctor.md`：Phase 058 dual-indexed 256K source-sorted-copy QEMU smoke；QEMU timing 不作为性能结论。
- `log/board/row_source_shuffle_dual_indexed_256k_sorted_copy_stability_repeated/summary.md`：10-run board RVV-vs-RVV stability 复核，median B/A `1.010x`，5/10 run 低于 1。
- `log/board/row_source_shuffle_dual_indexed_256k_sorted_copy_stability_repeated/evidence_doctor.md`：`Errors=1`、`Warnings=1`、`Suggestions=1`，支撑 dual-indexed source-sorted-copy 不进入 production probe。
- `log/qemu/custom_row_source_large_variance_profile/evidence_doctor.md`：custom row-source large variance QEMU smoke，8 comparisons，`Errors=0`、`Warnings=0`、`Suggestions=0`。
- `log/board/custom_row_source_large_variance_profile_repeated/summary.md`：5-run board production public custom row-source profile，8 个 256K order-pattern case 全部 positive。
- `log/board/custom_row_source_large_variance_profile_repeated/evidence_doctor.md`：`Errors=0`、`Warnings=8`，warning 来自 stride / reverse / shuffle long-tail 和组内离群；结论按 row source / order pattern 分开报告。
- `log/qemu/custom_layout_padding_sensitivity/evidence_doctor.md`：custom layout padding sensitivity QEMU smoke，12 comparisons，`Errors=0`、`Warnings=0`、`Suggestions=0`。
- `log/board/custom_layout_padding_sensitivity_repeated/summary.md`：5-run board production public custom layout padding sensitivity，12 个 custom layout / row-source / size case 全部 positive。
- `log/board/custom_layout_padding_sensitivity_repeated/evidence_doctor.md`：`Errors=0`、`Warnings=15`，warning 来自 huge-padding long-tail 和 layout / stride group-outlier；结论按 layout / row source / size 分开报告。
- `log/qemu/custom_layout_alignment_sensitivity/evidence_doctor.md`：custom layout alignment sensitivity QEMU smoke，6 comparisons，`Errors=0`、`Warnings=0`、`Suggestions=0`。
- `log/board/custom_layout_alignment_sensitivity_repeated/summary.md`：5-run board production public custom layout alignment sensitivity，6 个 alignas custom layout / row-source / size case 全部 positive，median B/A 范围 `1.636x` 到 `2.309x`。
- `log/board/custom_layout_alignment_sensitivity_repeated/evidence_doctor.md`：`Errors=0`、`Warnings=3`，warning 来自 source-indexed 和 dual-indexed 256K variance；结论按 row source / size 分开报告。
- `log/qemu/row_source_affine_index_fast_path_production_probe/evidence_doctor.md`：affine index fast-path production probe QEMU smoke，6 comparisons，`Errors=0`、`Warnings=0`、`Suggestions=0`。
- `log/board/row_source_affine_index_fast_path_production_probe_repeated/summary.md`：5-run board production public affine index fast-path probe，6 个 contiguous row-source case 全部 positive，median B/A 范围 `10.115x` 到 `14.021x`。
- `log/board/row_source_affine_index_fast_path_production_probe_repeated/evidence_doctor.md`：`Errors=0`、`Warnings=0`、`Suggestions=0`；用户确认后已作为 adopted production branch 的证据。
- `log/qemu/scalar_double_production_probe/evidence_doctor.md`：scalar-double production probe QEMU smoke，`Errors=0`、`Warnings=0`、`Suggestions=0`；QEMU timing 不作为性能结论。
- `log/board/scalar_double_production_probe_repeated/summary.md`：5-run board production public scalar-double probe，B/A `33.955, 33.664, 33.781, 34.077, 33.792`，median `33.792x`，checksum match。
- `log/board/scalar_double_production_probe_repeated/evidence_doctor.md`：`Errors=0`、`Warnings=0`、`Suggestions=0`；Phase 066 用户确认后已作为 adopted ordered double branch 的证据。
- `log/qemu/row_source_scalar_double_production_probe/evidence_doctor.md`：row-source scalar-double production probe QEMU smoke，`Errors=0`、`Warnings=0`、`Suggestions=0`；QEMU timing 不作为性能结论。
- `log/board/row_source_scalar_double_production_probe_repeated/summary.md`：5-run board production public row-source scalar-double probe，source-indexed / dual-indexed / correspondence median B/A 分别为 `20.201x`、`14.800x`、`13.474x`，checksum match。
- `log/board/row_source_scalar_double_production_probe_repeated/evidence_doctor.md`：`Errors=0`、`Warnings=1`、`Suggestions=0`；warning 为 dual-indexed variance，Phase 067 用户确认后已作为 adopted row-source double branch 的证据。
- `log/qemu/generic_scalar_double_ordered_production_probe/evidence_doctor.md`：ordered generic scalar-double production probe QEMU smoke，5 comparisons，`Errors=0`、`Warnings=0`、`Suggestions=0`；QEMU timing 不作为性能结论。
- `log/board/generic_scalar_double_ordered_production_probe_repeated/summary.md`：5-run board production public ordered generic scalar-double probe，5 个代表组合全部 positive，median B/A 范围 `24.111x` 到 `32.493x`，checksum match。
- `log/board/generic_scalar_double_ordered_production_probe_repeated/evidence_doctor.md`：`Errors=0`、`Warnings=1`、`Suggestions=0`；warning 为 `PointNormal->PointXYZRGB` group-outlier，Phase 068 用户确认后已作为 adopted ordered generic double branch 的证据。
- `log/qemu/row_source_generic_scalar_double_production_probe/evidence_doctor.md`：row-source generic scalar-double production probe QEMU smoke，9 comparisons，`Errors=0`、`Warnings=0`、`Suggestions=0`；QEMU timing 不作为性能结论。
- `log/board/row_source_generic_scalar_double_production_probe_repeated/summary.md`：5-run board production public row-source generic scalar-double probe，9 个代表组合全部 positive，median B/A 范围 `11.309x` 到 `17.433x`，checksum match；Phase 074 已按用户确认写入 adopted。
- `log/board/row_source_generic_scalar_double_production_probe_repeated/evidence_doctor.md`：`Errors=0`、`Warnings=4`、`Suggestions=0`；warning 为 long-tail / variance，需保留 min / median / max。
- `log/qemu/custom_layout_scalar_double_diagnostic_scout/evidence_doctor.md`：custom layout scalar-double scout QEMU smoke，4 comparisons，`Errors=0`、`Warnings=0`、`Suggestions=0`；QEMU timing 不作为性能结论。
- `log/board/custom_layout_scalar_double_diagnostic_scout_repeated/summary.md`：5-run board production-public custom layout scalar-double scout，4 个 case 全部 positive，median B/A 范围 `9.966x` 到 `27.497x`，checksum match；Phase 074 已按用户确认写入 adopted。
- `log/board/custom_layout_scalar_double_diagnostic_scout_repeated/evidence_doctor.md`：`Errors=0`、`Warnings=1`、`Suggestions=0`；warning 为 dual-indexed long-tail / variance，需保留 min / median / max。
- `log/qemu/more_custom_layout_scalar_double_sampling/evidence_doctor.md`：more custom layout scalar-double sampling QEMU smoke，12 comparisons，`Errors=0`、`Warnings=0`、`Suggestions=0`；QEMU timing 不作为性能结论。
- `log/board/more_custom_layout_scalar_double_sampling_repeated/summary.md`：5-run board production-public more custom layout scalar-double sampling，12 个 case 全部 positive，median B/A 范围 `2.766x` 到 `29.087x`，checksum match；Phase 074 已按用户确认写入 adopted。
- `log/board/more_custom_layout_scalar_double_sampling_repeated/evidence_doctor.md`：`Errors=0`、`Warnings=8`、`Suggestions=0`；warning 为 long-tail / variance 和 group-outlier，需按 layout / row source 保留 min / median / max。
- `log/qemu/correspondence_sorted_copy_scalar_double_production_probe/evidence_doctor.md`：correspondence sorted-copy scalar-double production probe QEMU smoke，2 comparisons，`Errors=0`、`Warnings=0`、`Suggestions=0`；QEMU timing 不作为性能结论。
- `log/board/correspondence_sorted_copy_scalar_double_production_probe_repeated/summary.md`：5-run board production-public correspondence sorted-copy scalar-double probe，64K / 256K median B/A `5.727x` / `5.408x`，checksum match；这是 Phase 075 回滚前的历史 public-positive 证据。
- `log/board/correspondence_sorted_copy_scalar_double_detail_ab_repeated/summary.md`：5-run board production-detail RVV-vs-RVV sorted-copy double vs D64 gather A/B，64K / 256K median B/A `0.283x` / `0.431x`，checksum match；Doctor `Errors=2`、`Warnings=1`、`Suggestions=0`，支撑 Phase 075 选择 D64 gather 并回滚 sorted-copy double。
- `log/board/correspondence_sorted_copy_scalar_double_production_probe_repeated/evidence_doctor.md`：`Errors=0`、`Warnings=0`、`Suggestions=0`；public Std/RVV positive 不能替代同边界 RVV-vs-RVV family selection。
- `log/evidence_registry.json`：registry 已记录 QEMU correctness、QEMU smoke、board diagnostic、board production direct、matrix-local、generic public 和 row-source evidence；`evidence_status` 为 fresh。

生成日志默认不提交；只有被本目录文档明确引用的 summary、manifest、Evidence Doctor report 或小型 correctness log 才能在用户授权后进入提交候选。

## 默认不提交的生成产物

`build/`、raw board logs、本机 `config.mk`、私有板卡地址、未被文档引用的 `log/qemu/*.log` 和 `log/board/*.log` 默认不提交。

## production_topic_doc 适用性

`artifact_layout.topic_doc_template` 解析出的 production 长期主题文档当前适用，路径为 `doc-rvv/registration/transformation_estimation_svd_scale-RVV.zh.md`。该文档只记录已采纳 production behavior、dispatch / fallback、证据链和未覆盖范围。

| 路线 | 是否进入长期文档 | 说明 |
| --- | --- | --- |
| Phase 047 / 050 / 061 / 066 / 067 / 068 / 069 / 070 / 071 | 是 | 已由用户确认采纳，属于当前 production facts。 |
| Phase 072 | 仅保留历史证据 | public positive 只表示快于 scalar fallback。 |
| Phase 073 / 075 | 否 | sorted-copy double family selection negative，Phase 075 已回滚。 |
| Phase 040 / 041 / 044 / 045 / 046 / 048 / 049 / 058 / 059 / 060 / 063 / 064 / 065 / 073 / 075 | 否 | 仍主要归属 topic-local phase 文档。 |

Phase 047 的 correspondence sorted-copy production probe、Phase 050 的 matrix-local helper simplification、Phase 061 的 contiguous affine index fast path、Phase 066 的 ordered scalar-double branch、Phase 067 的 row-source scalar-double branch、Phase 068 的 ordered generic scalar-double branch、Phase 069 的 row-source generic scalar-double branch，以及 Phase 070 / 071 的 custom layout double 采样边界都应进入长期文档。Phase 072 只作为 public positive 历史探针保留；Phase 073/075 记录 sorted-copy double family-selection negative 和 rollback/no-production。
