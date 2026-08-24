# 阶段索引

## 当前恢复入口

默认恢复入口是 Phase 075 rollback/no-production closeout 后的最终验证：Phase 047 的 `Scalar=float` correspondence sorted-copy production probe（对应关系排序副本生产探针）已由用户确认采纳，matrix-local-scale-simplification（矩阵局部 scale 简化）已收口为 adopted helper simplification，Phase 061 的 contiguous affine index fast path 也已由用户确认采纳。Phase 074 已根据用户确认把 Phase 069、Phase 070 和 Phase 071 收口为 `adopted-by-user`：Phase 069 覆盖三类 row-source common PCL xyz AoS / `Scalar=double` probe，9 个 board case 全 positive，median B/A `11.309x` 到 `17.433x`，Board Doctor `Errors=0`、`Warnings=4`、`Suggestions=0`；Phase 070 覆盖 `LocalSVDScalePaddedXYZSource -> LocalSVDScaleWideXYZTarget` custom xyz AoS layout 的 ordered 和三类 row-source `Scalar=double` public path，4 个 case 全 positive，median B/A `9.966x` 到 `27.497x`，Board Doctor `Errors=0`、`Warnings=1`、`Suggestions=0`；Phase 071 覆盖 compact、huge-padding 和 aligned 三组 custom layout double，12 个 case 全 positive，median B/A `2.766x` 到 `29.087x`，Board Doctor `Errors=0`、`Warnings=8`、`Suggestions=0`。Phase 072 的 correspondence sorted-copy `Scalar=double` public probe 曾取得 64K / 256K board median B/A `5.727x` / `5.408x`、Doctor `0/0/0`；但 Phase 073 的同边界 RVV-vs-RVV detail A/B 显示 sorted-copy double 慢于既有 D64 gather RVV family，64K / 256K median B/A `0.283x` / `0.431x`、Doctor `Errors=2`、`Warnings=1`、`Suggestions=0`。Phase 075 已按用户“同边界选择更快实现”的规则回滚 sorted-copy double 生产分流，当前 `Scalar=double` correspondence 在 contiguous fast path 不命中后使用 D64 gather RVV family。

- current previous result：`doc/phases/075-correspondence-sorted-copy-scalar-double-rollback-closeout/result.zh.md`
- current decision：`rolled_back_no_production / correspondence-sorted-copy-scalar-double-production-probe`；`closed_negative_for_clean_adoption / correspondence-sorted-copy-scalar-double-detail-ab`；`adopted-by-user / more-custom-layout-scalar-double-sampling`；`adopted-by-user / custom-layout-scalar-double-diagnostic-scout`；`adopted-by-user / row-source-generic-scalar-double-production-probe`；`adopted_by_user / generic-scalar-double-ordered-production-probe`；`adopted_by_user / row-source-scalar-double-production-probe`；`adopted_by_user / scalar-double-production-probe`；`adopted_by_user / affine-index-fast-path-production-probe`；`adopted_by_user / correspondence-sorted-copy-production-probe`；`adopted_by_user / matrix-local-scale-simplification`；`attempted_negative_or_mixed / staged-selected-cloud-detail-ab`；`attempted_negative_or_mixed / dual-indexed-target-sorted-detail-ab`；`rejected_or_unstable_with_evidence / sorted-copy-dual-indexed-stability`；`positive_more_generic_board_complete / more-generic-xyz-aos-point-types`；`positive_row_source_more_generic_public_board_complete / row-source-more-generic-xyz-aos-point-types`；`positive_row_source_all_more_generic_public_board_complete / row-source-all-more-generic-xyz-aos-matrix`；`mixed_custom_layout_sampling / production_public_custom_xyz_aos_layout`；`profile_positive_with_variance_warnings / custom-row-source-large-variance-profile`；`sampled_positive_with_padding_sensitivity_warnings / custom-layout-padding-sensitivity`；`sampled_positive_with_alignment_sensitivity_warnings / custom-layout-alignment-sensitivity`
- default next action：完成 Phase 075 verification 后扫描 roadmap；若仍无同范围正向候选，则停在 closeout / submit 准备。当前没有未决的 Phase 072 用户决策边界。

## 阶段表

| phase | 状态 | 默认下一动作 |
| --- | --- | --- |
| `000-current-state-and-gaps` | complete_positive | result 已回填；QEMU correctness/smoke、asm、board repeated 和 Evidence Doctor 已闭合。 |
| `005-doc-suite-role-parity` | complete | topic-local docs 已按 role-based templates（基于职责的模板）补齐当前范围；结果见 `005-doc-suite-role-parity/result.zh.md`。 |
| `010-production-integration-plan` | complete_positive_pending_user_confirmation | PI1-PI5 已完成；production patch 证据支持采纳，且已在 Phase 031 收口为 adopted。 |
| `020-matrix-local-scale-simplification` | complete_implementation_shape_weak_positive | result 已回填；legacy formula 与 trace formula 数值等价，board repeated overall `weak_positive`，但只作为实现形态诊断。 |
| `030-pi5-user-confirmation-packet` | complete_turn_stop_at_user_confirmation | result 保留 PI5 历史确认包；现在作为已采纳主线的前序记录。 |
| `031-adoption-closeout-plan` | complete_adoption_closeout | adoption closeout 已完成；当前 adopted 状态主归属见其 result。 |
| `032-rollback-no-production-plan` | archived_not_applicable_after_adoption | 仍保留为历史备用计划，不再是当前默认路径。 |
| `040-generic-point-type-expansion` | complete_correctness_scout_positive | 已完成代表点型 correctness scout；Phase 041 已补 generic public board / ASM。 |
| `041-generic-point-type-public-board-asm` | complete_positive | 代表点型 generic public QEMU smoke、ASM attribution 和 board repeated 已完成。 |
| `042-matrix-local-production-probe` | complete_deferred_probe | 较小 patch 探针只作为实现形态备选，不改变 adopted 主线。 |
| `043-row-source-expansion` | complete_adopted_by_user | source-indexed、dual-indexed 和 correspondence 公开入口已完成 correctness / QEMU smoke / board repeated / doctor / registry；用户已确认采纳。 |
| `044-row-source-generic-xyz-point-type-expansion` | complete_positive | 代表泛型点型 row-source correctness / QEMU smoke / board repeated / doctor / registry 已完成；9 个 board case 全 positive。 |
| `045-row-source-locality-order-profile` | complete_profile_positive_with_locality_sensitivity | source-indexed、dual-indexed、correspondence x contiguous/stride/reverse/shuffle x 4K/64K/256K 共 36 个 board case 全 positive；shuffle 明显低于 contiguous / stride / reverse。 |
| `046-row-source-shuffle-mitigation-detail-ab` | complete_conditional_positive | sorted-copy 对 correspondence shuffle 64K/256K positive，dual-indexed 256K weak-positive，dual-indexed 64K 和 4K 不适合直接接入。 |
| `047-correspondence-sorted-copy-production-probe` | complete_adopted_by_user | correspondence sorted-copy production probe 已在 size >= 64K 且 shuffle-like disorder gate 下接入并由用户确认采纳；board Doctor `Errors=0`、`Warnings=1`。 |
| `048-row-source-shuffle-staged-selected-cloud-detail-ab` | complete_attempted_negative_or_mixed | staged-selected-cloud detail A/B 覆盖 dual-indexed / correspondence x 4K/64K/256K；board 6/6 case negative，Doctor `Errors=6`、`Warnings=9`。 |
| `049-dual-indexed-target-sorted-detail-ab` | complete_attempted_negative_or_mixed | target-sorted detail A/B 覆盖 dual-indexed shuffle 64K/256K；64K negative，256K weak-positive，Doctor `Errors=1`、`Warnings=1`。 |
| `050-matrix-local-adoption-closeout` | complete_adopted_by_user | matrix-local helper simplification 已收口为 adopted production helper simplification；它不改变 row-source / correspondence adoption 边界。 |
| `051-more-generic-xyz-aos-point-types` | complete_evidence_boundary_expanded | 更多常见 PCL xyz AoS 点型 ordered public correctness / QEMU smoke 已完成；不扩大 production gate 或替代 board evidence。 |
| `052-more-generic-xyz-aos-board` | complete_positive_more_generic_board | Phase 051 新增 5 个常见 PCL xyz AoS 点型 64K board repeated 已完成，全部 positive，Doctor `0/0/0`；只扩大这些具体组合的性能证据边界。 |
| `053-row-source-more-generic-xyz-aos-point-types` | complete_positive_row_source_more_generic_board | 3 个更多常见 PCL xyz AoS row-source 代表组合 correctness / QEMU smoke / board repeated 已完成；9 个 board case 全部 positive，Doctor `0/3/0`；只覆盖这些 row-source / 点型 / size 组合。 |
| `054-row-source-all-more-generic-xyz-aos-matrix` | complete_positive_row_source_all_more_generic_board | Phase 051 五个常见 PCL xyz AoS 点型组合 × source-indexed / dual-indexed / correspondence 全交叉 correctness / QEMU smoke / board repeated 已完成；45 个 board case 全部 positive，Doctor `0/18/0`；只覆盖这些具体 row-source / 点型 / size 组合。 |
| `055-custom-xyz-aos-layout-sampling` | complete_mixed_custom_layout_sampling | 两个测试本地 registered custom xyz AoS layout 的 ordered + row-source correctness / QEMU smoke / board repeated 已完成；ordered 和多数 row-source positive，但 256K dual-indexed / correspondence 因退化频率不能写成 clean positive，Doctor `Errors=1`、`Warnings=15`。 |
| `056-custom-row-source-large-variance-profile` | complete_profile_positive_with_variance_warnings | 两个 custom layout 样本的 256K dual-indexed / correspondence × contiguous / stride / reverse / shuffle 复核已完成；8 个 board case 全 positive，Doctor `Errors=0`、`Warnings=8`；只作为受控 order-pattern profile。 |
| `057-custom-layout-padding-sensitivity` | complete_sampled_positive_with_padding_sensitivity_warnings | compact-ish 与 huge-padding 两组 custom layout × source-indexed / dual-indexed / correspondence × 64K / 256K 已完成；12 个 board case 全 positive，Doctor `Errors=0`、`Warnings=15`；只能按 layout / row source / size 分开解释。 |
| `058-dual-indexed-256k-sorted-copy-stability` | complete_rejected_or_unstable_with_evidence | Phase 046 残留的 dual-indexed 256K source-sorted-copy 线索已用 10-run board 复核；median B/A `1.010x`，5/10 run 低于 1，Doctor `Errors=1`、`Warnings=1`、`Suggestions=1`；不进入 production probe。 |
| `059-custom-layout-alignment-sensitivity` | complete_sampled_positive_with_alignment_sensitivity_warnings | `LocalAligned64XYZSource -> LocalAligned32XYZTarget` alignas custom layout × source-indexed / dual-indexed / correspondence × 64K / 256K 已完成；6 个 board case 全 positive，Doctor `Errors=0`、`Warnings=3`；只能按这个 alignment 采样组合解释。 |
| `060-affine-index-fast-path-detail-ab` | complete_positive_production_probe_candidate | contiguous offset source-indexed / dual-indexed / correspondence × 64K / 256K 已完成 RVV-vs-RVV detail A/B；6 个 board case 全 positive，median B/A `1.676x` 到 `2.750x`，Doctor `Errors=0`、`Warnings=3`；下一步是有界 production probe。 |
| `061-affine-index-fast-path-production-probe` | complete_positive_adopted_by_user | contiguous offset source-indexed / dual-indexed / correspondence 已接入 bounded production probe；Std/RVV 23 tests passed，QEMU smoke 6 comparisons Doctor `0/0/0`，board 6 个 case 全 positive，median B/A `10.115x` 到 `14.021x`，Doctor `0/0/0`；用户已确认采纳。 |
| `062-affine-index-fast-path-adoption-closeout` | complete_adoption_closeout | Phase 061 production probe 已收口为 adopted production behavior；topic-local docs、长期 `doc-rvv`、matrix / roadmap 和 evidence 边界已刷新。 |
| `063-scalar-double-diagnostic-scout` | complete_diagnostic_correctness_scout | `Scalar=double` ordered `PointXYZ -> PointXYZ` test-only scalar double accumulation 和 RVV f64 widened accumulation 已通过 correctness；Std/RVV 各 25 tests passed；QEMU smoke only 显示 checksum 一致和 `max_public_error` 约 `1.615e-08`，但未做 board performance / production probe。 |
| `064-scalar-double-board-scout` | complete_board_diagnostic_positive_no_production | `Scalar=double` RVV f64 widened board diagnostic 已完成；5-run B/A median `2.863x`，Doctor `0/0/0`，registry fresh；下一步只能进入 production probe plan，不能直接采纳。 |
| `065-scalar-double-production-probe` | complete_positive_pending_user_confirmation | ordered `PointXYZ -> PointXYZ` / `Scalar=double` production public probe 已完成；Std/RVV 各 31 tests passed，QEMU Doctor `0/0/0`，board median B/A `33.792x`，Doctor `0/0/0`；PI5 后由 Phase 066 收口。 |
| `066-scalar-double-adoption-closeout` | complete_adoption_closeout | 用户确认当前有收益实现可以接入；ordered `PointXYZ -> PointXYZ` / `Scalar=double` / dense / `nr_points >= 16` 已成为 adopted production behavior，不外推到 row-source double 或泛型 double。 |
| `067-row-source-scalar-double-production-probe` | complete_adopted_by_user | 用户确认当前有收益实现可以接入；source-indexed、dual-indexed 和 correspondence 的 exact `PointXYZ -> PointXYZ` / `Scalar=double` / dense row-source public overload 已成为 adopted production behavior。Board median B/A 分别为 `20.201x`、`14.800x` 和 `13.474x`；Doctor `0/1/0`。 |
| `068-generic-scalar-double-ordered-production-probe` | complete_adopted_by_user | 用户确认当前有收益实现可以接入；ordered common PCL xyz AoS / `Scalar=double` / dense public overload 已成为 adopted production behavior。5 个 board case 全 positive，median B/A `24.111x` 到 `32.493x`；Doctor `0/1/0`，`PointNormal->PointXYZRGB` group-outlier 按点型单独报告。 |
| `069-row-source-generic-scalar-double-production-probe` | complete_adopted_by_user | source-indexed、dual-indexed 和 correspondence 的 common PCL xyz AoS / `Scalar=double` / dense public overload 已完成 production probe；Std/RVV 各 33 tests passed，QEMU Doctor `0/0/0`，board 9 个 case 全 positive，median B/A `11.309x` 到 `17.433x`，Doctor `0/4/0`。Phase 074 已根据用户确认收口为 adopted。 |
| `070-custom-layout-scalar-double-diagnostic-scout` | complete_adopted_by_user | custom layout `Scalar=double` public scout 已完成；Std/RVV 各 35 tests passed，QEMU Doctor `0/0/0`，board ordered/source-indexed/dual-indexed/correspondence 4 个 case 全 positive，median B/A `9.966x` 到 `27.497x`，Doctor `0/1/0`。只覆盖 `LocalSVDScalePaddedXYZSource -> LocalSVDScaleWideXYZTarget`，Phase 074 已根据用户确认收口为 adopted。 |
| `071-more-custom-layout-scalar-double-sampling` | complete_adopted_by_user | more custom layout `Scalar=double` sampling 已完成；Std/RVV 各 37 tests passed，QEMU Doctor `0/0/0`，board 12 个 case 全 positive，median B/A `2.766x` 到 `29.087x`，Doctor `0/8/0`。只作为 Phase 070 的取样增强，Phase 074 已根据用户确认收口为 adopted。 |
| `072-correspondence-sorted-copy-scalar-double-production-probe` | complete_historical_public_positive_rolled_back_by_phase075 | correspondence sorted-copy `Scalar=double` public probe 已完成；Std/RVV 各 38 tests passed，QEMU 2 comparisons Doctor `0/0/0`，board 64K / 256K 两个 case 全 positive，median B/A `5.727x` / `5.408x`，Doctor `0/0/0`。public Std/RVV positive 不能证明 sorted-copy double 优于既有 D64 gather family；Phase 073 同边界 detail A/B 已为 negative，Phase 075 已回滚该生产分流。 |
| `073-correspondence-sorted-copy-scalar-double-detail-ab` | complete_detail_ab_negative_closed_by_phase075 | sorted-copy double vs D64 gather 的 production-detail RVV-vs-RVV A/B 已完成；QEMU smoke paired labels 可解析但 B/A 均小于 1，Doctor `2/0/0`；board 64K / 256K median B/A `0.283x` / `0.431x`，Doctor `2/1/0`。结果不支持 clean adoption；Phase 075 已选择 D64 gather。 |
| `075-correspondence-sorted-copy-scalar-double-rollback-closeout` | complete_rollback_no_production_pending_final_verification | 根据用户同边界选择更快实现的规则，移除 sorted-copy double overload 和 double correspondence branch 中的 sorted-copy 尝试；保留 Phase 047 `Scalar=float` sorted-copy。最终 verification 归属见 Phase 075 result。 |
| `076-documentation-closeout-and-submit-prep` | complete_doc_closeout_ready_for_submit | 文档收尾、doc-suite parity 和 artifact tracking 已整理；phase result 记录 stop reason、doc-suite role inventory 和 submit prep。 |

## 文档归属

phase plan/result 保存阶段事实和 early-stop（过早停止）检查；`doc/optimization-roadmap.zh.md` 保存跨阶段候选；`doc/transformation_estimation_svd_scale-evaluation.zh.md` 保存函数级评估和生产接入判断。当前已采纳 production patch，`artifact_layout.topic_doc_template` 解析出的 production 长期主题文档已生效，后续 generic point type / row-source 扩展仍回到 topic-local phase。

Phase 020 的 `matrix-local-scale-simplification` 结果归属在 `doc/phases/020-matrix-local-scale-simplification/result.zh.md`、`doc/phases/optimization-matrix.zh.md` 和 `doc/optimization-roadmap.zh.md`。它不接管 production 长期主题文档，也不改变 Phase 010 的 PI5 pending 状态。

Phase 030 的确认包归属在 `doc/phases/030-pi5-user-confirmation-packet/result.zh.md`。它保留用户检查点历史，不再是当前恢复入口。

Phase 031 的结果归属在 `doc/phases/031-adoption-closeout-plan/result.zh.md`。它是当前 adopted 状态的主记录。

Phase 032 的计划草案归属在 `doc/phases/032-rollback-no-production-plan/plan.zh.md`。它仍作为历史备选计划保留，但不再是当前默认路径。

Phase 040 的结果归属在 `doc/phases/040-generic-point-type-expansion/result.zh.md`。它记录代表点型 correctness scout，Phase 041 接续补齐 generic public board / ASM。

Phase 041 的结果归属在 `doc/phases/041-generic-point-type-public-board-asm/result.zh.md`。它记录代表点型 generic public board evidence 和 ASM attribution，仍不外推到全部 xyz AoS 或 row source。

Phase 043 的结果归属在 `doc/phases/043-row-source-expansion/result.zh.md`。它记录 row-source production patch、三类 row source 的 public correctness、QEMU smoke、board repeated 和 Evidence Doctor 结果；用户确认后已收口为 adopted。

Phase 044 的结果归属在 `doc/phases/044-row-source-generic-xyz-point-type-expansion/result.zh.md`。它记录代表泛型点型 row-source public evidence，仍不外推到全部自定义点型、`Scalar=double` 或非法 index / correspondence。

Phase 045 的结果归属在 `doc/phases/045-row-source-locality-order-profile/result.zh.md`。它记录 row-source order pattern profile，解释 Phase 043 / 044 warning 的 locality sensitivity；它不是新的 production family，也不扩大 adoption 范围。

Phase 046 的结果归属在 `doc/phases/046-row-source-shuffle-mitigation-detail-ab/result.zh.md`。它记录 sorted-copy mitigation 的 RVV-vs-RVV detail A/B：correspondence 64K/256K 是 production probe candidate，但本阶段没有修改 production，也没有把 sorted-copy 写成 adopted behavior。

Phase 047 的结果归属在 `doc/phases/047-correspondence-sorted-copy-production-probe/result.zh.md`。它记录真实 public correspondence overload 下的 sorted-copy production probe、QEMU / board / Doctor 证据和用户采纳后的 adopted 状态。

Phase 048 的结果归属在 `doc/phases/048-row-source-shuffle-staged-selected-cloud-detail-ab/result.zh.md`。它记录 staged-selected-cloud 路线的 RVV-vs-RVV detail A/B 负向结果；该路线不进入 production。

Phase 049 的结果归属在 `doc/phases/049-dual-indexed-target-sorted-detail-ab/result.zh.md`。它记录 target-sorted 路线的 RVV-vs-RVV detail A/B 负向 / 弱正向混合结果；该路线不进入 production。

Phase 050 的结果归属在 `doc/phases/050-matrix-local-adoption-closeout/result.zh.md`。它记录 matrix-local helper simplification 已由用户确认收口为 adopted helper simplification；该结论不扩大 row-source、点类型或 `Scalar` 范围。

Phase 051 的结果归属在 `doc/phases/051-more-generic-xyz-aos-point-types/result.zh.md`。它记录更多常见 PCL xyz AoS 点型 ordered public correctness / QEMU smoke 已闭合；该结论不扩大 production gate，不外推到全部自定义点型，也不作为板卡性能证据。

Phase 052 的结果归属在 `doc/phases/052-more-generic-xyz-aos-board/result.zh.md`。它记录 Phase 051 新增 5 个常见 PCL xyz AoS 点型的 64K board repeated 已闭合；该结论只覆盖这些具体 ordered public 点型组合，不外推到 row-source 点型扩展、全部自定义 xyz AoS 或 `Scalar=double`。

Phase 053 的结果归属在 `doc/phases/053-row-source-more-generic-xyz-aos-point-types/result.zh.md`。它记录 source-indexed `PointXYZRGBA -> PointXYZRGBA`、dual-indexed `PointNormal -> PointXYZRGB` 和 correspondence `PointWithViewpoint -> PointXYZ` 的 row-source correctness / QEMU smoke / board repeated 已闭合；该结论不外推到 Phase 051 全部点型的 row-source 全交叉、全部自定义 xyz AoS 或 `Scalar=double`。

Phase 054 的结果归属在 `doc/phases/054-row-source-all-more-generic-xyz-aos-matrix/result.zh.md`。它记录 Phase 051 五个常见 PCL xyz AoS 点型组合在 source-indexed、dual-indexed 和 correspondence 三类 row source 下的全交叉 correctness / QEMU smoke / board repeated 已闭合；该结论不外推到全部自定义 xyz AoS、异常 padding / stride、非法 index / correspondence 或 `Scalar=double`。

Phase 055 的结果归属在 `doc/phases/055-custom-xyz-aos-layout-sampling/result.zh.md`。它记录两个测试本地 registered custom xyz AoS layout 样本的 ordered / row-source correctness、QEMU smoke、board repeated 和 Evidence Doctor；该结论只覆盖这两个 layout 样本，并把 256K dual-indexed / correspondence 降级为 negative / unstable slice。

Phase 056 的结果归属在 `doc/phases/056-custom-row-source-large-variance-profile/result.zh.md`。它记录两个 custom layout 样本在 256K dual-indexed / correspondence 下的 order-pattern profile；该结论只解释 Phase 055 mixed slice 的受控输入复核，不扩大 production gate。

Phase 057 的结果归属在 `doc/phases/057-custom-layout-padding-sensitivity/result.zh.md`。它记录 compact-ish 与 huge-padding 两组 custom layout 在三类 row source 和 64K/256K 下的 padding sensitivity 采样；该结论是 sampled positive with padding sensitivity warnings，不扩大 production gate，也不替代更广 layout / alignment 取样。

Phase 058 的结果归属在 `doc/phases/058-dual-indexed-256k-sorted-copy-stability/result.zh.md`。它记录 dual-indexed 256K source-sorted-copy 残留线索的 10-run stability 复核；该结论关闭 dual-indexed sorted-copy production probe 方向，不影响 Phase 047 correspondence sorted-copy adopted branch。

Phase 059 的结果归属在 `doc/phases/059-custom-layout-alignment-sensitivity/result.zh.md`。它记录 `alignas(64)` source 与 `alignas(32)` target 的 custom layout alignment sensitivity 采样；该结论只证明这一个 alignment 组合在三类 row source 和 64K/256K 下 sampled positive，不扩大 production gate，也不替代更广 layout / alignment 取样。

Phase 060 的结果归属在 `doc/phases/060-affine-index-fast-path-detail-ab/result.zh.md`。它记录 contiguous offset row-source 的 affine fast-path detail A/B；该结论只支持进入 production probe，不自动成为 adopted production behavior。

Phase 061 的结果归属在 `doc/phases/061-affine-index-fast-path-production-probe/result.zh.md`。它记录真实 public overload 下的 affine index fast-path production probe、QEMU / board / Doctor 证据和 PI5 用户检查点。

Phase 062 的结果归属在 `doc/phases/062-affine-index-fast-path-adoption-closeout/result.zh.md`。它记录用户确认采纳后的文档同步、长期 `doc-rvv` 刷新和剩余优化方向边界。

Phase 063 的结果归属在 `doc/phases/063-scalar-double-diagnostic-scout/result.zh.md`。它记录 `Scalar=double` ordered `PointXYZ -> PointXYZ` 的 diagnostic correctness scout、RVV f64 widened test-only candidate 和 QEMU smoke-only 边界；该结论不接管 production 长期主题文档。

Phase 064 的结果归属在 `doc/phases/064-scalar-double-board-scout/result.zh.md`。它记录 `Scalar=double` RVV f64 widened 的 board diagnostic positive 结果；该结论只支持进入 production probe plan，不接管 production 长期主题文档。

Phase 065 的结果归属在 `doc/phases/065-scalar-double-production-probe/result.zh.md`。它记录真实 public ordered overload 下的 `Scalar=double` production probe、fallback tests、QEMU smoke、ASM 输入和 board repeated；该阶段停在 PI5 用户检查点。

Phase 066 的结果归属在 `doc/phases/066-scalar-double-adoption-closeout/result.zh.md`。它记录用户确认后的 adoption closeout，并把有界 ordered double branch 同步到长期 production 文档、matrix 和 roadmap。

Phase 067 的结果归属在 `doc/phases/067-row-source-scalar-double-production-probe/result.zh.md`。它记录 row-source `Scalar=double` public production probe、QEMU / board / Doctor 证据和用户确认后的 adopted 状态；该结论只覆盖 exact `PointXYZ -> PointXYZ` 的三类 row-source overload，不外推到 generic double 或 custom layout double。

Phase 068 的结果归属在 `doc/phases/068-generic-scalar-double-ordered-production-probe/result.zh.md`。它记录 ordered common PCL xyz AoS `Scalar=double` public production probe、QEMU / board / Doctor 证据和用户确认后的 adopted 状态；该结论只覆盖 ordered whitelist 点型，不外推到 row-source generic double、custom layout double、sorted-copy double 或任意自定义点型全集。

Phase 069 的结果归属在 `doc/phases/069-row-source-generic-scalar-double-production-probe/result.zh.md`。它记录 source-indexed、dual-indexed 和 correspondence 的 common PCL xyz AoS `Scalar=double` public production probe、fallback boundaries、QEMU / board / Doctor 证据和用户确认点；Phase 074 已根据用户确认收口为 `adopted-by-user`。

Phase 070 的结果归属在 `doc/phases/070-custom-layout-scalar-double-diagnostic-scout/result.zh.md`。它记录 custom xyz AoS layout `Scalar=double` public scout、fallback boundaries、QEMU / board / Doctor 证据和用户确认点；Phase 074 已根据用户确认收口为 `adopted-by-user`，也不能替代全部 custom layout double 或 sorted-copy double 证据。

Phase 071 的结果归属在 `doc/phases/071-more-custom-layout-scalar-double-sampling/result.zh.md`。它记录 compact、huge-padding 和 aligned 三组 custom layout `Scalar=double` public sampling、fallback boundaries、QEMU / board / Doctor 证据和用户确认点；Phase 074 已根据用户确认收口为 `adopted-by-user`，只能增强 Phase 070 的代表性，不能替代全部 custom layout double 或 sorted-copy double 证据。

Phase 072 的结果归属在 `doc/phases/072-correspondence-sorted-copy-scalar-double-production-probe/result.zh.md`。它记录 correspondence sorted-copy `Scalar=double` public probe、QEMU / board / Doctor 证据和 family-selection 缺口；经 Phase 073 同边界 detail A/B 和 Phase 075 rollback closeout 回填后，当前状态是 historical public-positive / no-production。

Phase 073 的结果归属在 `doc/phases/073-correspondence-sorted-copy-scalar-double-detail-ab/result.zh.md`。它记录 sorted-copy double vs D64 gather 的同边界 production-detail RVV-vs-RVV A/B；当前状态已由 Phase 075 收口为 `closed_negative_for_clean_adoption`，并选择 D64 gather。

Phase 075 的结果归属在 `doc/phases/075-correspondence-sorted-copy-scalar-double-rollback-closeout/result.zh.md`。它记录 sorted-copy double 的 production rollback / no-production closeout；历史 Phase 072 public-positive 证据保留，但当前生产路径不再尝试 sorted-copy double。

## 当前早停规则

只完成 scaffold、单个 helper、一次 QEMU 测试或一次 board smoke 都不能收口。当前已把 Phase 046 的 correspondence 正向子边界经 Phase 047 接入并采纳，把 Phase 048 的 staged-selected-cloud 路线、Phase 049 的 target-sorted 路线和 Phase 058 的 dual-indexed source-sorted-copy 残留路线按负向 / 不稳定证据收束，把 Phase 050 的 matrix-local helper 简化收口为 adopted helper simplification，并在 Phase 051 / 052 / 053 / 054 / 055 / 056 / 057 / 059 / 060 / 061 补齐各自计划内证据。Phase 062 已把 contiguous affine index fast-path production probe 收口为 adopted；Phase 066 已把 ordered `Scalar=double` production probe 收口为 adopted；Phase 067 已把 row-source exact `PointXYZ -> PointXYZ` / `Scalar=double` production probe 收口为 adopted；Phase 068 已把 ordered common PCL xyz AoS / `Scalar=double` production probe 收口为 adopted；Phase 074 已把 Phase 069 row-source common PCL xyz AoS `Scalar=double` production probe、Phase 070 custom layout double sample 和 Phase 071 more custom layout double sampling 收口为 adopted-by-user；Phase 072 已把 correspondence sorted-copy double public probe 推到 historical public positive；Phase 073 已证明该 sorted-copy double family 在同边界 detail A/B 下慢于 D64 gather；Phase 075 已回滚 sorted-copy double 生产分流。Phase 069、Phase 070、Phase 071 已采纳但仍不能外推到全部 custom layout double、sorted-copy double family selection 或任意自定义点型全集。
