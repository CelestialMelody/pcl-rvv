# Benchmark 与证据说明

## 本文职责

本文说明 bench label（性能测试标签）、计时边界、checksum（校验和）、QEMU smoke（QEMU 小型验证）和 board repeated（板卡重复采集）证据。读者用它判断某个 case-filter 测了哪条路径，以及对应 summary / Evidence Doctor 能证明什么。

本文件不承载完整 phase 流水。阶段取舍看 `doc/phases/*/result.zh.md`，当前 production 判断看 `doc/transformation_estimation_svd_scale-evaluation.zh.md`，target 总览看 `doc/testing-overview.zh.md`。

| 证据家族 | 代表 case-filter / target | 当前状态 |
| --- | --- | --- |
| adopted production bench | `public-scale`、`row-source-scale`、`row-source-affine-index-fast-path-production-probe`、`scalar-double-production-probe`、`scalar-double-row-source-production-probe`、`generic-scalar-double-ordered-production-probe`、`row-source-generic-scalar-double-production-probe` | 支撑已采纳生产分支。 |
| evidence expansion bench | `generic-xyz-point-types-public`、`more-generic-xyz-aos-point-types-public`、`row-source-all-more-generic-xyz-aos-matrix`、custom layout / padding / alignment filters | 扩大点型、row-source 或 layout 证据边界，不外推到全集。 |
| diagnostic / profile bench | `ordered-cloud-pair`、`matrix-local-scale`、`row-source-locality-order-profile`、`custom-row-source-large-variance-profile` | 解释候选形态、局部性或 warning，不单独证明 production adoption。 |
| RVV-vs-RVV family selection | sorted-copy、staged-selected-cloud、target-sorted、dual-indexed 256K stability 和 sorted-copy double detail A/B | 用于选择或关闭 mitigation family。 |
| rolled back / historical | `correspondence-sorted-copy-scalar-double-production-probe` | public Std/RVV 为 positive，但 Phase 073 detail A/B negative；Phase 075 已回滚。 |

## Bench 输出格式


`src/bench_tesvd_scale.cpp` 输出 `Dataset:`、`Iterations:`、`Warmup Iterations:`、每个 case 的 `ms/iter`、`Total Time`、checksum 和 `max_reference_error`。分析脚本依赖这些字段生成 summary、manifest 和 Evidence Doctor 输入。

checksum 是日志指纹，不是 bit-identical correctness gate（逐位一致正确性门槛）。RVV reduction tree（规约树）改变后，1e-6 量化 checksum 与标量不同是预期风险；当前 correctness 以 gtest 和 `max_reference_error <= 2e-3` 为准。

## CLI 参数

| 参数 | 默认值 | 含义 |
| --- | ---: | --- |
| `--iterations` | `20` | 每个 case 的计时迭代数。 |
| `--warmup-iterations` | `3` | 计时前 warmup 次数；board repeated target 覆盖为 5。 |
| `--case-filter` | `all` | 可选 `public-scale`、`ordered-cloud-pair`、`scalar-double-diagnostic-scout`、`scalar-double-production-probe`、`scalar-double-row-source-production-probe`、`generic-scalar-double-ordered-production-probe`、`row-source-generic-scalar-double-production-probe`、`custom-layout-scalar-double-diagnostic-scout`、`more-custom-layout-scalar-double-sampling`、`correspondence-sorted-copy-scalar-double-production-probe`、`correspondence-sorted-copy-scalar-double-detail-ab`、`matrix-local-scale`、`generic-xyz-point-types-public`、`more-generic-xyz-aos-point-types-public`、`row-source-scale`、`row-source-generic-xyz-point-types`、`row-source-more-generic-xyz-aos-point-types`、`row-source-all-more-generic-xyz-aos-matrix`、`custom-xyz-aos-layout-sampling`、`custom-row-source-large-variance-profile`、`custom-layout-padding-sensitivity`、`custom-layout-alignment-sensitivity`、`row-source-locality-order-profile`、`row-source-affine-index-fast-path-detail-ab`、`row-source-affine-index-fast-path-production-probe`、`row-source-shuffle-sorted-copy-detail-ab`、`correspondence-sorted-copy-production-probe`、`row-source-shuffle-staged-selected-cloud-detail-ab`、`row-source-shuffle-dual-indexed-target-sorted-detail-ab`、`row-source-shuffle-dual-indexed-256k-sorted-copy-stability` 或 `all`。 |

## Bench Label / case-filter 字典

| case-filter | label | baseline / candidate | 计时边界 |
| --- | --- | --- | --- |
| `public-scale` | `public scale ordered-cloud-pair <size>` | production public scale path | 包含父类 centroid、demean、`getTransformationFromCorrelation` 和输出矩阵。 |
| `ordered-cloud-pair` | `fused scale candidate ordered-cloud-pair <size>` | test-only fused scale candidate | 包含直接点对累加、3x3 SVD、scale 和输出矩阵；不包含 production dispatch。 |
| `scalar-double-diagnostic-scout` | `scalar double diagnostic scout ordered-cloud-pair 64K` | public double fallback / scalar double fallback / RVV f64 widened scout | 只覆盖 ordered `PointXYZ -> PointXYZ` / `Scalar=double` diagnostic；输出 checksum、`max_public_error` 和 path。QEMU timing 不作为性能证据；Phase 064 board repeated 只是 diagnostic performance signal，不是 production dispatch evidence。 |
| `scalar-double-production-probe` | `scalar double production probe ordered-cloud-pair 64K` | Std public double fallback / RVV public double production branch | 真实 public ordered overload；RVV 构建在 exact `PointXYZ -> PointXYZ` / `Scalar=double` / dense / `nr_points >= 16` 命中时走 f64 widened accumulation。Phase 065 完成 PI5 probe，Phase 066 根据用户确认收口为 adopted；不覆盖 row-source double 或 generic double。 |
| `scalar-double-row-source-production-probe` | `scalar double row-source production probe <source-indexed|dual-indexed|correspondence> 64K` | Std public row-source double fallback / RVV public row-source double production branch | 真实 source-indexed、dual-indexed 和 correspondence public overload；RVV 构建在 exact `PointXYZ -> PointXYZ` / `Scalar=double` / dense / `nr_points >= 16` 命中时走 row-source f64 widened accumulation。Phase 067 完成 PI5 probe 并根据用户确认收口为 adopted；不覆盖 generic double、custom layout double、sorted-copy double 或非法 index / correspondence。 |
| `generic-scalar-double-ordered-production-probe` | `generic scalar double ordered production probe <PointSource->PointTarget> 64K` | Std public generic double fallback / RVV public generic double production branch | 真实 public ordered overload；RVV 构建在 common PCL xyz AoS whitelist、`RVVXYZAoSFloatLayout`、`Scalar=double`、dense、`nr_points >= 16` 命中时走 f64 widened accumulation。Phase 068 根据用户确认收口为 adopted；不覆盖 row-source generic double、custom layout double、sorted-copy double 或任意自定义点型全集。 |
| `row-source-generic-scalar-double-production-probe` | `row source generic scalar double production probe <source-indexed|dual-indexed|correspondence> <PointSource->PointTarget> 64K` | Std public generic double row-source fallback / RVV public generic double row-source production probe | 真实 source-indexed、dual-indexed 和 correspondence public overload；RVV 构建在 common PCL xyz AoS whitelist、`RVVXYZAoSFloatLayout`、`Scalar=double`、dense、`nr_points >= 16` 和合法 index / correspondence 命中时走 row-source f64 widened accumulation。Phase 069 已由 Phase 074 根据用户确认收口为 adopted；不覆盖 custom layout double、sorted-copy double 或任意自定义点型全集。 |
| `custom-layout-scalar-double-diagnostic-scout` | `custom layout scalar double scout ordered <LocalPaddedXYZSource->LocalWideXYZTarget> 64K` / `row source generic scalar double production probe <source-indexed|dual-indexed|correspondence> <LocalPaddedXYZSource->LocalWideXYZTarget> 64K` | Std custom layout double public fallback / RVV custom layout double public candidate | 真实 ordered、source-indexed、dual-indexed 和 correspondence public overload；RVV 构建在 `RVVXYZAoSFloatLayout`、`Scalar=double`、dense、`nr_points >= 16` 和合法 index / correspondence 命中时走 f64 widened accumulation。Phase 070 当前为 adopted-by-user；只覆盖这个 custom layout sample，不覆盖全部 custom layout double 或 sorted-copy double。 |
| `more-custom-layout-scalar-double-sampling` | `more custom layout scalar double sampling ordered <LocalCompactXYZSource->LocalCompactXYZTarget|LocalHugePaddingXYZSource->LocalHugePaddingXYZTarget|LocalAligned64XYZSource->LocalAligned32XYZTarget> 64K` / `row source generic scalar double production probe <source-indexed|dual-indexed|correspondence> <LocalCompactXYZSource->LocalCompactXYZTarget|LocalHugePaddingXYZSource->LocalHugePaddingXYZTarget|LocalAligned64XYZSource->LocalAligned32XYZTarget> 64K` | Std more custom layout double public fallback / RVV more custom layout double public candidate | 真实 ordered、source-indexed、dual-indexed 和 correspondence public overload；RVV 构建在 `RVVXYZAoSFloatLayout`、`Scalar=double`、dense、`nr_points >= 16` 和合法 index / correspondence 命中时走 f64 widened accumulation。Phase 071 当前为 adopted-by-user；只覆盖三组测试本地 custom layout sample，不覆盖全部 custom layout double 或 sorted-copy double。 |
| `correspondence-sorted-copy-scalar-double-production-probe` | `row source correspondence sorted-copy scalar double production probe <64K|256K>` | Std public correspondence double fallback / RVV public correspondence sorted-copy double production probe | 真实 public correspondence overload；RVV 构建在 `PointXYZ -> PointXYZ`、`Scalar=double`、dense、合法 shuffled correspondences、size / disorder gate 命中时走 sorted-copy D64 accumulation。Phase 072 public Std/RVV 为 positive；Phase 073 detail A/B 为 negative，因此当前为 bounded public positive but no clean adoption。 |
| `correspondence-sorted-copy-scalar-double-detail-ab` | `row source correspondence sorted-copy scalar double detail <current|sorted-copy> <64K|256K>` | D64 gather RVV baseline / sorted-copy double RVV candidate | 同一 production detail boundary 内比较 D64 gather RVV 与 sorted-copy double RVV；candidate 计时包含 correspondence copy + sort、D64 accumulation、solve 和输出 checksum。Phase 073 只回答 RVV-family-selection。 |
| `matrix-local-scale` | `matrix local scale simplification ordered-cloud-pair <size>` | Std build legacy formula vs RVV build trace formula | 包含父类形态的 centroid / demean / `H` 后段和 scale 公式差异；只表示实现形态 A/B，不表示 RVV 指令收益。 |
| `generic-xyz-point-types-public` | `generic public scale <PointSource->PointTarget> <size>` | production public generic scale path | 包含代表点型 public path；用于 `PointXYZI` / `PointXYZRGB` generic public smoke 和 board repeated。 |
| `more-generic-xyz-aos-point-types-public` | `more generic public scale <PointSource->PointTarget> 64K` | production public generic scale path | 包含更多常见 PCL xyz AoS 点型 public path；用于 Phase 051 QEMU smoke / manifest 和 Phase 052 board repeated。 |
| `row-source-scale` | `row source scale <source-indexed|dual-indexed|correspondence> <size>` | production public row-source scale path | 包含 source-indexed、dual-indexed 和 correspondence public path；用于 Phase 043 row-source smoke 和 board repeated。 |
| `row-source-generic-xyz-point-types` | `row source scale <row-source> <PointSource->PointTarget> <size>` | production public row-source generic scale path | 包含代表泛型点型 source-indexed、dual-indexed 和 correspondence public path；用于 Phase 044 row-source generic smoke 和 board repeated。 |
| `row-source-more-generic-xyz-aos-point-types` | `row source scale <row-source> <PointSource->PointTarget> <size>` | production public row-source more-generic scale path | 包含更多常见 PCL xyz AoS 代表组合 source-indexed、dual-indexed 和 correspondence public path；用于 Phase 053 row-source more-generic smoke 和 board repeated。 |
| `row-source-all-more-generic-xyz-aos-matrix` | `row source scale <row-source> <PointSource->PointTarget> <size>` | production public row-source all-more-generic scale path | 包含 Phase 051 五个常见 PCL xyz AoS 组合 × source-indexed、dual-indexed 和 correspondence public path；用于 Phase 054 row-source all-more-generic smoke 和 board repeated。 |
| `custom-xyz-aos-layout-sampling` | `custom public scale <ordered|row-source> <LocalPaddedXYZSource->LocalWideXYZTarget> <size>` | production public custom xyz AoS layout path | 包含两个测试本地 registered custom layout 样本的 ordered、source-indexed、dual-indexed 和 correspondence public path；用于 Phase 055 custom layout sampling。 |
| `custom-row-source-large-variance-profile` | `custom row source scale <dual-indexed|correspondence> locality-<contiguous|stride|reverse|shuffle> <LocalPaddedXYZSource->LocalWideXYZTarget> 256K` | production public custom row-source profile path | 只覆盖 Phase 055 两个 custom layout 样本的 256K dual-indexed / correspondence order pattern；用于 Phase 056 方差和局部性复核。 |
| `custom-layout-padding-sensitivity` | `custom padding row source scale <row-source> <LocalCompactXYZSource->LocalCompactXYZTarget|LocalHugePaddingXYZSource->LocalHugePaddingXYZTarget> <size>` | production public custom layout padding sensitivity path | 覆盖 compact-ish 和 huge-padding 两组测试本地 registered custom layout 的 source-indexed、dual-indexed 和 correspondence public path；用于 Phase 057 padding / stride 敏感性复核。 |
| `custom-layout-alignment-sensitivity` | `custom alignment row source scale <row-source> <LocalAligned64XYZSource->LocalAligned32XYZTarget> <size>` | production public custom layout alignment sensitivity path | 覆盖一个 alignas 测试本地 registered custom layout 的 source-indexed、dual-indexed 和 correspondence public path；用于 Phase 059 alignment / stride 敏感性采样。 |
| `row-source-locality-order-profile` | `row source scale <row-source> locality-<contiguous|stride|reverse|shuffle> <size>` | production public row-source profile path | 包含 source-indexed、dual-indexed 和 correspondence 在四种 order pattern 下的 public path；用于 Phase 045 locality / order profile。 |
| `row-source-affine-index-fast-path-detail-ab` | `row source affine current <row-source> <size>` / `row source affine fast-path <row-source> <size>` | RVV-current vs affine contiguous fast-path | 只在 RVV 构建内比较 current contiguous gather 与 test-only contiguous offset accumulation；用于 Phase 060 production probe candidate 判断。 |
| `row-source-affine-index-fast-path-production-probe` | `row source affine production probe <source-indexed|dual-indexed|correspondence> <size>` | Std public row-source path vs RVV production fast path | 真实 public overload；RVV 构建在 step=1 contiguous indices / correspondences 命中时走 production contiguous offset fast path，并计入 contiguous 检测成本；Phase 061 完成 PI5 production probe，Phase 062 根据用户确认收口为 adopted production branch。 |
| `row-source-shuffle-sorted-copy-detail-ab` | `row source shuffle current <dual-indexed|correspondence> <size>` / `row source shuffle sorted-copy <dual-indexed|correspondence> <size>` | RVV-current vs sorted-copy mitigation | 只在 RVV 构建内比较 current shuffled gather 与 copy/sort + same public row-source path；sorted-copy 计时包含 copy + sort。 |
| `correspondence-sorted-copy-production-probe` | `row source correspondence sorted-copy production probe <size>` | production public correspondence overload | 真实 public correspondence path；在 size / disorder gate 命中时走 sorted-copy production probe，否则自然回到 current gather / 父类 fallback。 |
| `row-source-shuffle-staged-selected-cloud-detail-ab` | `row source shuffle current <dual-indexed|correspondence> <size>` / `row source shuffle staged-selected-cloud <dual-indexed|correspondence> <size>` | RVV-current vs staged selected-cloud | 只在 RVV 构建内比较 current shuffled gather 与 selected source / target copy + ordered public estimate；staging 计时包含两端点云拷贝。 |
| `row-source-shuffle-dual-indexed-target-sorted-detail-ab` | `row source shuffle current dual-indexed <size>` / `row source shuffle target-sorted dual-indexed <size>` | RVV-current vs target-sorted mitigation | 只在 RVV 构建内比较 current shuffled dual-indexed gather 与 indices copy + target sort + same public dual-indexed estimate；target-sorted 计时包含 copy + sort。 |
| `row-source-shuffle-dual-indexed-256k-sorted-copy-stability` | `row source shuffle current dual-indexed 256K` / `row source shuffle sorted-copy dual-indexed 256K` | RVV-current vs source-sorted-copy stability | 只在 RVV 构建内比较 current shuffled dual-indexed gather 与 copy/sort by source + same public dual-indexed path；用于 Phase 058 10-run stability 复核。 |

## 推荐 Target

| target | 用途 | 输出 |
| --- | --- | --- |
| `run_test_compare` | correctness gate。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| `run_bench_compare` with `ALLOW_QEMU_BENCH_COMPARE=1` | QEMU smoke only，检查 label、checksum 和 summary 可解析。 | `log/qemu/analyze_bench_compare.log` |
| `record_qemu_scalar_double_diagnostic_scout_state` | Phase 063 `Scalar=double` QEMU smoke-only，并登记 raw smoke logs。 | `log/qemu/run_bench_std.log`、`log/qemu/run_bench_rvv.log`、`log/qemu/analyze_bench_compare.log`、`log/evidence_registry.json` |
| `record_qemu_scalar_double_production_probe_state` | Phase 065 / 066 `Scalar=double` production public probe QEMU smoke，并登记 manifest / Doctor / registry；QEMU timing 不作为性能证据。 | `log/qemu/scalar_double_production_probe/analyze_bench_compare.log`、manifest、doctor、registry |
| `record_qemu_row_source_scalar_double_production_probe_state` | Phase 067 row-source `Scalar=double` production public probe QEMU smoke，并登记 manifest / Doctor / registry；QEMU timing 不作为性能证据。 | `log/qemu/row_source_scalar_double_production_probe/analyze_bench_compare.log`、manifest、doctor、registry |
| `record_qemu_generic_scalar_double_ordered_production_probe_state` | Phase 068 ordered generic `Scalar=double` production public probe QEMU smoke，并登记 manifest / Doctor / registry；QEMU timing 不作为性能证据。 | `log/qemu/generic_scalar_double_ordered_production_probe/analyze_bench_compare.log`、manifest、doctor、registry |
| `record_qemu_row_source_generic_scalar_double_production_probe_state` | Phase 069 row-source generic `Scalar=double` production public probe QEMU smoke，并登记 manifest / Doctor / registry；QEMU timing 不作为性能证据。 | `log/qemu/row_source_generic_scalar_double_production_probe/analyze_bench_compare.log`、manifest、doctor、registry |
| `record_qemu_custom_layout_scalar_double_diagnostic_scout_state` | Phase 070 custom layout `Scalar=double` production-public scout QEMU smoke，并登记 manifest / Doctor / registry；QEMU timing 不作为性能证据。 | `log/qemu/custom_layout_scalar_double_diagnostic_scout/analyze_bench_compare.log`、manifest、doctor、registry |
| `record_qemu_more_custom_layout_scalar_double_sampling_state` | Phase 071 more custom layout `Scalar=double` production-public sampling QEMU smoke，并登记 manifest / Doctor / registry；QEMU timing 不作为性能证据。 | `log/qemu/more_custom_layout_scalar_double_sampling/analyze_bench_compare.log`、manifest、doctor、registry |
| `record_qemu_correspondence_sorted_copy_scalar_double_production_probe_state` | Phase 072 correspondence sorted-copy `Scalar=double` production-public QEMU smoke，并登记 manifest / Doctor / registry；QEMU timing 不作为性能证据。 | `log/qemu/correspondence_sorted_copy_scalar_double_production_probe/analyze_bench_compare.log`、manifest、doctor、registry |
| `record_qemu_correspondence_sorted_copy_scalar_double_detail_ab_state` | Phase 073 correspondence sorted-copy `Scalar=double` detail A/B QEMU smoke，并登记 manifest / Doctor / registry；QEMU timing 不作为性能证据。 | `log/qemu/correspondence_sorted_copy_scalar_double_detail_ab/analyze_bench_compare.log`、manifest、doctor、registry |
| `run_qemu_smoke_evidence_doctor` | QEMU smoke manifest / doctor。 | `log/qemu/evidence_manifest.json`、`log/qemu/evidence_doctor.md` |
| `run_board_bench_scalar_double_diagnostic_scout_repeated` | Phase 064 `Scalar=double` board diagnostic repeated；只比较 test-only scalar double fallback 与 RVV f64 widened candidate。 | `log/board/scalar_double_diagnostic_scout_repeated/summary.md`、manifest、doctor |
| `run_board_bench_scalar_double_production_probe_repeated` | Phase 065 / 066 `Scalar=double` board production public probe repeated；比较 Std public double fallback 与 RVV public double branch。 | `log/board/scalar_double_production_probe_repeated/summary.md`、manifest、doctor |
| `run_board_bench_row_source_scalar_double_production_probe_repeated` | Phase 067 row-source `Scalar=double` board production public probe repeated；比较 Std public row-source double fallback 与 RVV public row-source double branch。 | `log/board/row_source_scalar_double_production_probe_repeated/summary.md`、manifest、doctor |
| `run_board_bench_generic_scalar_double_ordered_production_probe_repeated` | Phase 068 ordered generic `Scalar=double` board production public probe repeated；比较 Std public generic double fallback 与 RVV public generic double branch。 | `log/board/generic_scalar_double_ordered_production_probe_repeated/summary.md`、manifest、doctor |
| `run_board_bench_row_source_generic_scalar_double_production_probe_repeated` | Phase 069 row-source generic `Scalar=double` board production public probe repeated；比较 Std public generic double row-source fallback 与 RVV public generic double row-source branch。 | `log/board/row_source_generic_scalar_double_production_probe_repeated/summary.md`、manifest、doctor |
| `run_board_bench_custom_layout_scalar_double_diagnostic_scout_repeated` | Phase 070 custom layout `Scalar=double` board production-public scout repeated；比较 Std custom layout double public fallback 与 RVV custom layout double public candidate。 | `log/board/custom_layout_scalar_double_diagnostic_scout_repeated/summary.md`、manifest、doctor |
| `run_board_bench_more_custom_layout_scalar_double_sampling_repeated` | Phase 071 more custom layout `Scalar=double` board production-public sampling repeated；比较 Std more custom layout double public fallback 与 RVV more custom layout double public candidate。 | `log/board/more_custom_layout_scalar_double_sampling_repeated/summary.md`、manifest、doctor |
| `run_board_bench_correspondence_sorted_copy_scalar_double_production_probe_repeated` | Phase 072 correspondence sorted-copy `Scalar=double` board production-public repeated；比较 Std public correspondence double fallback 与 RVV public correspondence sorted-copy double probe。 | `log/board/correspondence_sorted_copy_scalar_double_production_probe_repeated/summary.md`、manifest、doctor |
| `run_board_bench_correspondence_sorted_copy_scalar_double_detail_ab_repeated` | Phase 073 correspondence sorted-copy `Scalar=double` board production-detail RVV-vs-RVV repeated；比较 D64 gather RVV baseline 与 sorted-copy double RVV candidate。 | `log/board/correspondence_sorted_copy_scalar_double_detail_ab_repeated/summary.md`、manifest、doctor |
| `run_board_test_smoke` | 板卡 correctness smoke。 | `log/board/test_smoke/run_test.log` |
| `run_board_bench_ordered_cloud_pair_repeated` | Phase 000 repeated board diagnostic。 | `log/board/ordered_cloud_pair_repeated/summary.md`、manifest、doctor |
| `run_board_bench_production_public_scale_repeated` | Phase 010 repeated board production direct。 | `log/board/production_public_scale_ordered_cloud_pair_repeated/summary.md`、manifest、doctor |
| `run_board_bench_matrix_local_scale_repeated` | Phase 020 repeated board implementation-shape diagnostic。 | `log/board/matrix_local_scale_repeated/summary.md`、manifest、doctor |
| `run_board_bench_generic_xyz_point_types_public_repeated` | Phase 041 repeated board production public generic。 | `log/board/generic_xyz_point_types_public_repeated/summary.md`、manifest、doctor |
| `record_qemu_more_generic_public_state` | Phase 051 QEMU more-generic public smoke。 | `log/qemu/more_generic_xyz_aos_point_types_public/analyze_bench_compare.log`、manifest、doctor |
| `run_board_bench_more_generic_xyz_aos_point_types_public_repeated` | Phase 052 repeated board production public more-generic。 | `log/board/more_generic_xyz_aos_point_types_public_repeated/summary.md`、manifest、doctor |
| `record_qemu_row_source_state` | Phase 043 QEMU row-source smoke。 | `log/qemu/row_source_scale/analyze_bench_compare.log`、manifest、doctor |
| `run_board_bench_row_source_scale_repeated` | Phase 043 repeated board production public row-source。 | `log/board/row_source_scale_repeated/summary.md`、manifest、doctor |
| `record_qemu_row_source_generic_state` | Phase 044 QEMU row-source generic smoke。 | `log/qemu/row_source_generic_xyz_point_types/analyze_bench_compare.log`、manifest、doctor |
| `run_board_bench_row_source_generic_xyz_point_types_repeated` | Phase 044 repeated board production public row-source generic。 | `log/board/row_source_generic_xyz_point_types_repeated/summary.md`、manifest、doctor |
| `record_qemu_row_source_more_generic_state` | Phase 053 QEMU row-source more-generic smoke。 | `log/qemu/row_source_more_generic_xyz_aos_point_types/analyze_bench_compare.log`、manifest、doctor |
| `run_board_bench_row_source_more_generic_xyz_aos_point_types_repeated` | Phase 053 repeated board production public row-source more-generic。 | `log/board/row_source_more_generic_xyz_aos_point_types_repeated/summary.md`、manifest、doctor |
| `record_qemu_row_source_all_more_generic_state` | Phase 054 QEMU row-source all-more-generic smoke。 | `log/qemu/row_source_all_more_generic_xyz_aos_matrix/analyze_bench_compare.log`、manifest、doctor |
| `run_board_bench_row_source_all_more_generic_xyz_aos_matrix_repeated` | Phase 054 repeated board production public row-source all-more-generic。 | `log/board/row_source_all_more_generic_xyz_aos_matrix_repeated/summary.md`、manifest、doctor |
| `record_qemu_custom_row_source_large_variance_profile_state` | Phase 056 QEMU custom row-source large variance profile smoke。 | `log/qemu/custom_row_source_large_variance_profile/analyze_bench_compare.log`、manifest、doctor |
| `run_board_bench_custom_row_source_large_variance_profile_repeated` | Phase 056 repeated board production public custom row-source profile。 | `log/board/custom_row_source_large_variance_profile_repeated/summary.md`、manifest、doctor |
| `record_qemu_custom_layout_padding_sensitivity_state` | Phase 057 QEMU custom layout padding sensitivity smoke。 | `log/qemu/custom_layout_padding_sensitivity/analyze_bench_compare.log`、manifest、doctor |
| `run_board_bench_custom_layout_padding_sensitivity_repeated` | Phase 057 repeated board production public custom layout padding sensitivity。 | `log/board/custom_layout_padding_sensitivity_repeated/summary.md`、manifest、doctor |
| `record_qemu_custom_layout_alignment_sensitivity_state` | Phase 059 QEMU custom layout alignment sensitivity smoke。 | `log/qemu/custom_layout_alignment_sensitivity/analyze_bench_compare.log`、manifest、doctor |
| `run_board_bench_custom_layout_alignment_sensitivity_repeated` | Phase 059 repeated board production public custom layout alignment sensitivity。 | `log/board/custom_layout_alignment_sensitivity_repeated/summary.md`、manifest、doctor |
| `record_qemu_row_source_locality_order_profile_state` | Phase 045 QEMU row-source locality / order profile smoke。 | `log/qemu/row_source_locality_order_profile/analyze_bench_compare.log`、manifest、doctor |
| `run_board_bench_row_source_locality_order_profile_repeated` | Phase 045 repeated board production public row-source profile。 | `log/board/row_source_locality_order_profile_repeated/summary.md`、manifest、doctor |
| `record_qemu_affine_index_fast_path_detail_ab_state` | Phase 060 QEMU affine index fast-path detail A/B smoke。 | `log/qemu/row_source_affine_index_fast_path_detail_ab/summary.md`、manifest、doctor |
| `run_board_bench_affine_index_fast_path_detail_ab_repeated` | Phase 060 repeated board RVV-vs-RVV affine fast-path detail A/B。 | `log/board/row_source_affine_index_fast_path_detail_ab_repeated/summary.md`、manifest、doctor |
| `record_qemu_affine_index_fast_path_production_probe_state` | Phase 061 QEMU affine index fast-path production probe smoke。 | `log/qemu/row_source_affine_index_fast_path_production_probe/analyze_bench_compare.log`、manifest、doctor |
| `run_board_bench_affine_index_fast_path_production_probe_repeated` | Phase 061/062 repeated board production public affine index fast-path probe，用户确认后已采纳。 | `log/board/row_source_affine_index_fast_path_production_probe_repeated/summary.md`、manifest、doctor |
| `record_qemu_row_source_shuffle_sorted_copy_detail_ab_state` | Phase 046 QEMU row-source shuffle sorted-copy detail A/B smoke。 | `log/qemu/row_source_shuffle_sorted_copy_detail_ab/summary.md`、manifest、doctor |
| `run_board_bench_row_source_shuffle_sorted_copy_detail_ab_repeated` | Phase 046 repeated board RVV-vs-RVV detail A/B。 | `log/board/row_source_shuffle_sorted_copy_detail_ab_repeated/summary.md`、manifest、doctor |
| `record_qemu_correspondence_sorted_copy_production_probe_state` | Phase 047 QEMU correspondence sorted-copy production probe smoke。 | `log/qemu/correspondence_sorted_copy_production_probe/analyze_bench_compare.log`、manifest、doctor |
| `run_board_bench_correspondence_sorted_copy_production_probe_repeated` | Phase 047 repeated board production public correspondence sorted-copy probe。 | `log/board/correspondence_sorted_copy_production_probe_repeated/summary.md`、manifest、doctor |
| `record_qemu_row_source_staged_selected_cloud_detail_ab_state` | Phase 048 QEMU staged-selected-cloud detail A/B smoke。 | `log/qemu/row_source_shuffle_staged_selected_cloud_detail_ab/summary.md`、manifest、doctor |
| `run_board_bench_row_source_shuffle_staged_selected_cloud_detail_ab_repeated` | Phase 048 repeated board RVV-vs-RVV staged-selected-cloud detail A/B。 | `log/board/row_source_shuffle_staged_selected_cloud_detail_ab_repeated/summary.md`、manifest、doctor |
| `record_qemu_row_source_target_sorted_detail_ab_state` | Phase 049 QEMU dual-indexed target-sorted detail A/B smoke。 | `log/qemu/row_source_shuffle_dual_indexed_target_sorted_detail_ab/summary.md`、manifest、doctor |
| `run_board_bench_row_source_shuffle_dual_indexed_target_sorted_detail_ab_repeated` | Phase 049 repeated board RVV-vs-RVV target-sorted detail A/B。 | `log/board/row_source_shuffle_dual_indexed_target_sorted_detail_ab_repeated/summary.md`、manifest、doctor |
| `record_qemu_dual_indexed_256k_sorted_copy_stability_state` | Phase 058 QEMU dual-indexed 256K sorted-copy stability smoke。 | `log/qemu/row_source_shuffle_dual_indexed_256k_sorted_copy_stability/summary.md`、manifest、doctor |
| `run_board_bench_dual_indexed_256k_sorted_copy_stability_repeated` | Phase 058 repeated board RVV-vs-RVV dual-indexed 256K stability 复核。 | `log/board/row_source_shuffle_dual_indexed_256k_sorted_copy_stability_repeated/summary.md`、manifest、doctor |
| `evidence_status` | registry freshness check。 | `log/evidence_registry.json` |

## 计时边界

计时边界按 case-filter 分组记录。表中“production public”表示真实公开入口；“detail A/B”表示同一 RVV 构建内比较两个 RVV family；“diagnostic”表示测试专用或局部实现形态。

| case-filter 组 | 计时边界 | 证据角色 |
| --- | --- | --- |
| `public-scale` | 真实 ordered public scale path，包含父类入口、centroid、demean、3x3 SVD、scale 和输出矩阵。 | Phase 031 adopted ordered production direct。 |
| `ordered-cloud-pair` | test-only fused candidate，直接从原始点对累加统计量，保留 Eigen 3x3 SVD 和输出矩阵。 | Phase 000 diagnostic；不含 production dispatch。 |
| `matrix-local-scale` | legacy formula 与 `trace(R * H)` 简化公式的局部 helper 对比。 | implementation-shape 证据；不是 RVV intrinsic 主线。 |
| generic / more-generic ordered public | 代表点型和更多常见 PCL xyz AoS 的真实 ordered public path。 | Phase 041 / 052 point-type evidence expansion。 |
| row-source public | source-indexed、dual-indexed、correspondence 的真实 public overload，包含 gather、3x3 SVD、scale 和输出矩阵。 | Phase 043 / 044 / 053 / 054 row-source evidence。 |
| custom layout / padding / alignment | 测试本地 custom layout samples 的 ordered 或 row-source public path。 | Phase 055 / 057 / 059 / 070 / 071 layout sampling。 |
| locality / variance profile | contiguous、stride、reverse、shuffle 等 order pattern。 | 解释 row-source warning 和 locality sensitivity。 |
| affine index fast path | detail A/B 不计通用检测成本；production probe 计入真实 contiguous 检测和 fallback。 | Phase 060 candidate；Phase 061 / 062 adopted production branch。 |
| `Scalar=double` production probes | 真实 ordered 或 row-source public double overload，记录 `max_public_error` 和 path。 | Phase 065 到 Phase 071 adopted double branches。 |
| sorted-copy float production probe | correspondence public overload，包含 sorted-copy gate、copy、sort、RVV accumulation 和 scalar solve tail。 | Phase 047 adopted narrow branch。 |
| sorted-copy double public probe | correspondence double public overload，包含 sorted-copy branch。 | Phase 072 historical public-positive；不能回答 RVV-family-selection。 |
| sorted-copy double detail A/B | D64 gather RVV baseline 对 sorted-copy double RVV candidate，candidate 计入 copy + sort。 | Phase 073 negative；Phase 075 rollback。 |
| staged-selected-cloud / target-sorted / stability | detail A/B 或 10-run stability，candidate 计入 copy / sort / selected-cloud 构造。 | Phase 048 / 049 / 058 rejected 或 unstable。 |

## QEMU 与板卡边界

QEMU 只用于 correctness、build、case label、checksum、`max_reference_error` 和 manifest 形状。性能结论只来自 repeated board summary，并且必须结合 Evidence Doctor 的 Errors / Warnings / Suggestions。

| phase / evidence family | QEMU 边界 | board 结论 | 处理方式 |
| --- | --- | --- | --- |
| Phase 000 diagnostic | build / label / `max_reference_error`。 | 4K/64K/256K 全 positive，median B/A `2.783x` / `2.958x` / `2.937x`，Doctor `0/1/0`。 | 历史 diagnostic 对照。 |
| Phase 031 ordered production direct | public smoke / asm 输入。 | production direct median B/A `26.123x` / `33.860x` / `33.066x`，Doctor `0/1/0`。 | adopted ordered production behavior。 |
| Phase 020 / 042 / 050 matrix-local | `matrix-local-scale` smoke。 | median B/A `1.683x` / `1.173x` / `1.173x`，Doctor `0/2/0`。 | adopted helper simplification；不写成 RVV intrinsic 主线。 |
| Phase 041 / 051 / 052 ordered generic | generic 和 more-generic smoke / manifest。 | 代表点型和 5 个 more-generic case 全 positive，Phase 052 Doctor `0/0/0`。 | point-type evidence expansion。 |
| Phase 043 / 044 / 053 / 054 row-source | row-source public / generic / all-more-generic smoke。 | row-source public、代表泛型、更多泛型和 45-case 全交叉均 positive；Doctor warnings 按 row source / point type / size 解释。 | adopted 或 evidence expansion，不能外推到非法 index / correspondence。 |
| Phase 055 / 056 / 057 / 059 custom layout float | custom layout、order profile、padding、alignment smoke。 | 多数或全部 positive；Doctor warnings 记录 variance、padding、alignment 和 group-outlier。 | sampled evidence；不外推到全部 custom layout。 |
| Phase 045 locality profile | 36 label smoke。 | 36/36 positive；shuffle 明显低于 contiguous / stride / reverse，Doctor `0/29/0`。 | 解释 locality sensitivity，不新增 family。 |
| Phase 060 / 061 affine fast path | detail A/B 和 production public smoke。 | Phase 060 detail A/B 6/6 positive；Phase 061 production probe 6/6 positive，median B/A `10.115x` 到 `14.021x`，Doctor `0/0/0`。 | Phase 062 adopted。 |
| Phase 063 / 064 double diagnostic | QEMU smoke-only；timing 不作为性能。 | diagnostic median B/A `2.863x`，Doctor `0/0/0`。 | 支撑进入 production probe，不直接采纳。 |
| Phase 065 / 066 ordered double | production public smoke。 | median B/A `33.792x`，Doctor `0/0/0`。 | adopted ordered exact `PointXYZ` double。 |
| Phase 067 row-source double | production public smoke。 | source-indexed / dual-indexed / correspondence median B/A `20.201x` / `14.800x` / `13.474x`，Doctor `0/1/0`。 | adopted exact row-source double。 |
| Phase 068 ordered generic double | production public smoke。 | 5 个 case positive，median B/A `24.111x` 到 `32.493x`，Doctor `0/1/0`。 | adopted ordered common PCL xyz AoS double。 |
| Phase 069 row-source generic double | production public smoke。 | 9 个 case positive，median B/A `11.309x` 到 `17.433x`，Doctor `0/4/0`。 | Phase 074 adopted-by-user。 |
| Phase 070 / 071 custom layout double | custom layout double public smoke。 | Phase 070 4/4 positive，median B/A `9.966x` 到 `27.497x`；Phase 071 12/12 positive，median B/A `2.766x` 到 `29.087x`。 | Phase 074 adopted-by-user；只覆盖 sampled layouts。 |
| Phase 072 sorted-copy double public probe | public smoke。 | 64K / 256K public Std/RVV positive，median B/A `5.727x` / `5.408x`，Doctor `0/0/0`。 | historical public-positive。 |
| Phase 073 sorted-copy double detail A/B | paired label smoke；QEMU Errors 只说明 QEMU timing 不作性能。 | sorted-copy double 相对 D64 gather negative，median B/A `0.283x` / `0.431x`，Doctor `2/1/0`。 | Phase 075 回滚 sorted-copy double。 |
| Phase 046 / 047 sorted-copy float | detail A/B 和 production probe smoke。 | correspondence 64K/256K detail A/B positive；production probe 4K/64K/256K median B/A `8.128x` / `3.929x` / `3.618x`。 | Phase 047 adopted narrow float branch。 |
| Phase 048 / 049 / 058 negative families | build / label / manifest shape。 | staged-selected-cloud 6/6 negative；target-sorted negative / mixed；dual-indexed 256K stability median `1.010x` 且 5/10 run 低于 1。 | rejected 或 unstable，不进入 production。 |

## Evidence Doctor / Manifest 边界

Evidence Doctor（证据体检）分成 QEMU 和 board 两类。QEMU Doctor 主要检查 manifest 和日志形状；board Doctor 才参与性能证据解释。

| 证据组 | manifest / doctor 路径 | 当前 Doctor 摘要 | 使用规则 |
| --- | --- | --- | --- |
| QEMU generic / row-source / layout smoke | `log/qemu/*/evidence_manifest.json`、`log/qemu/*/evidence_doctor.md` | 大多数 production smoke 为 `0/0/0`；negative detail A/B 的 QEMU Errors 只表示 QEMU timing 小于 1。 | 不能作为性能结论。 |
| QEMU 裸文件 | `log/qemu/evidence_manifest.json`、`log/qemu/evidence_doctor.md` | 会被不同 smoke target 覆盖。 | 以 phase result 和 registry 条目确认归属。 |
| adopted board production | `log/board/*_repeated/evidence_manifest.json`、`evidence_doctor.md` | ordered、row-source、affine fast path、double 和 custom layout double 采样均有 positive summary；warnings 按 case 降级解释。 | 可支撑对应 adopted 边界。 |
| evidence expansion board | generic、more-generic、custom layout、padding、alignment、locality profile summary / Doctor | warnings 常见于 variance、long-tail、group-outlier、padding 或 locality sensitivity。 | 只扩大 sampled evidence boundary。 |
| rejected / rolled-back board | staged-selected-cloud、target-sorted、dual-indexed stability、sorted-copy double detail A/B Doctor | Errors 表示 repeated board 中稳定小于 1 或负向。 | 用于关闭 family，不用于扩大生产分流。 |
| registry | `log/evidence_registry.json` | `evidence_status` 当前用于 freshness check。 | 进入提交前必须保持 fresh。 |

完整路径清单见“当前可提交证据”和各 phase result。文档引用 summary、manifest、Doctor 或小型 correctness log 时，才进入可提交证据候选；raw logs 默认不提交。

## ASM Attribution 口径


Phase 010 反汇编证明 production ordered public overload 符号内可见 `vlsseg3e32.v`、`vfmacc.vv`、`vfredosum.vs`、`vsetvli`，并调用 `solveTransformationEstimationSVDScaleF32`。Phase 000 diagnostic candidate 的 ASM 只能作为历史诊断证据。

Phase 041 generic public smoke 复用 `dump_bench_rvv` 输出，RVV bench full asm 可见 `PointXYZI` / `PointXYZRGB` generic public bench 实例以及 `vlse32.v`、`vfmacc.vv`、`vfredosum.vs` 等生产累加相关指令。该归因只覆盖代表点型 public generic bench，不覆盖全部点型。

## 复现命令

```bash
ALLOW_QEMU_BENCH_COMPARE=1 make -C test-rvv/registration/transformation_estimation_svd_scale run_bench_compare BENCH_ARGS="--case-filter ordered-cloud-pair --iterations 3 --warmup-iterations 1"
ALLOW_QEMU_BENCH_COMPARE=1 make -C test-rvv/registration/transformation_estimation_svd_scale run_bench_compare BENCH_ARGS="--case-filter public-scale --iterations 3 --warmup-iterations 1"
ALLOW_QEMU_BENCH_COMPARE=1 make -C test-rvv/registration/transformation_estimation_svd_scale run_bench_compare BENCH_ARGS="--case-filter scalar-double-diagnostic-scout --iterations 3 --warmup-iterations 1"
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_scalar_double_diagnostic_scout_state
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_scalar_double_diagnostic_scout_repeated
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_scalar_double_production_probe_state
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_scalar_double_production_probe_repeated
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_row_source_scalar_double_production_probe_state
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_row_source_scalar_double_production_probe_repeated
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_generic_scalar_double_ordered_production_probe_state
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_generic_scalar_double_ordered_production_probe_repeated
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_row_source_generic_scalar_double_production_probe_state
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_row_source_generic_scalar_double_production_probe_repeated
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_more_custom_layout_scalar_double_sampling_state
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_more_custom_layout_scalar_double_sampling_repeated
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_correspondence_sorted_copy_scalar_double_production_probe_state
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_correspondence_sorted_copy_scalar_double_production_probe_repeated
ALLOW_QEMU_BENCH_COMPARE=1 make -C test-rvv/registration/transformation_estimation_svd_scale run_bench_compare BENCH_ARGS="--case-filter matrix-local-scale --iterations 3 --warmup-iterations 1"
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_generic_public_state
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_more_generic_public_state
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_row_source_state
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_row_source_generic_state
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_row_source_all_more_generic_state
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_custom_row_source_large_variance_profile_state
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_custom_layout_padding_sensitivity_state
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_custom_layout_alignment_sensitivity_state
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_row_source_locality_order_profile_state
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_affine_index_fast_path_production_probe_state
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_row_source_shuffle_sorted_copy_detail_ab_state
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_correspondence_sorted_copy_production_probe_state
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_row_source_staged_selected_cloud_detail_ab_state
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_row_source_target_sorted_detail_ab_state
make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_dual_indexed_256k_sorted_copy_stability_state
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_ordered_cloud_pair_repeated
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_production_public_scale_repeated
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_matrix_local_scale_repeated
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_generic_xyz_point_types_public_repeated
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_more_generic_xyz_aos_point_types_public_repeated
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_row_source_scale_repeated
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_row_source_generic_xyz_point_types_repeated
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_row_source_all_more_generic_xyz_aos_matrix_repeated
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_custom_row_source_large_variance_profile_repeated
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_custom_layout_padding_sensitivity_repeated
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_custom_layout_alignment_sensitivity_repeated
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_row_source_locality_order_profile_repeated
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_affine_index_fast_path_production_probe_repeated
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_row_source_shuffle_sorted_copy_detail_ab_repeated
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_correspondence_sorted_copy_production_probe_repeated
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_row_source_shuffle_staged_selected_cloud_detail_ab_repeated
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_row_source_shuffle_dual_indexed_target_sorted_detail_ab_repeated
make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_dual_indexed_256k_sorted_copy_stability_repeated
```

## 提交边界

raw logs 默认不提交；summary、manifest、Evidence Doctor report 和小型 correctness log 只有被本目录文档引用并经用户授权后才进入提交候选。
