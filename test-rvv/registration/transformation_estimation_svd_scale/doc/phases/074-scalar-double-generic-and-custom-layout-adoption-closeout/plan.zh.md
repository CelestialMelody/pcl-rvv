# Phase 074 Plan: scalar-double generic and custom layout adoption closeout

## 阶段意图和边界

本阶段根据用户确认，把已经完成接入后 production-public board evidence（真实公开入口板卡证据）的 Phase 069、Phase 070 和 Phase 071 收口为 `adopted-by-user`。用户确认口径是：板卡测试显示接入后的真实公开入口有收益即可采纳，正式 `doc-rvv` 使用接入后的板卡测试数据。

本阶段只采纳以下边界：

- Phase 069：source-indexed、dual-indexed 和 correspondence / common PCL xyz AoS whitelist / `Scalar=double` / dense / 64K / 合法 index 或 correspondence。
- Phase 070：`LocalSVDScalePaddedXYZSource -> LocalSVDScaleWideXYZTarget` / ordered + source-indexed + dual-indexed + correspondence / `Scalar=double` / dense / 64K。
- Phase 071：compact、huge-padding 和 aligned 三组测试本地 registered custom layout sample / ordered + source-indexed + dual-indexed + correspondence / `Scalar=double` / dense / 64K。

本阶段不采纳 Phase 072 sorted-copy double。Phase 072 虽然 public Std/RVV board positive，但 Phase 073 同边界 RVV-vs-RVV detail A/B 已证明它慢于既有 D64 gather RVV family，因此仍保持 `bounded_public_positive_detail_ab_negative`。

## 当前状态清单

| item | current state | evidence |
| --- | --- | --- |
| Phase 069 | `positive_pending_user_confirmation` | correctness Std/RVV 33/33；QEMU Doctor `0/0/0`；board 9/9 positive，median B/A `11.309x` 到 `17.433x`，Doctor `0/4/0` |
| Phase 070 | `positive_pending_user_confirmation` | correctness Std/RVV 35/35；QEMU Doctor `0/0/0`；board 4/4 positive，median B/A `9.966x` 到 `27.497x`，Doctor `0/1/0` |
| Phase 071 | `positive_pending_user_confirmation` | correctness Std/RVV 37/37；QEMU Doctor `0/0/0`；board 12/12 positive，median B/A `2.766x` 到 `29.087x`，Doctor `0/8/0` |
| Phase 072/073 | bounded public positive + detail A/B negative | public board `5.727x` / `5.408x`；detail A/B `0.283x` / `0.431x` |

## 优化矩阵

| candidate family | row source / layout | evidence role | board evidence | decision after this phase |
| --- | --- | --- | --- | --- |
| `row-source-generic-scalar-double-production-probe` | common PCL xyz AoS whitelist / three row-source overloads | production-public | 9/9 positive, median `11.309x` to `17.433x` | `adopted-by-user` |
| `custom-layout-scalar-double-diagnostic-scout` | one registered custom layout sample / ordered + row-source | production-public scout | 4/4 positive, median `9.966x` to `27.497x` | `adopted-by-user` for this sample boundary |
| `more-custom-layout-scalar-double-sampling` | compact / huge-padding / aligned custom layout samples / ordered + row-source | production-public sampling | 12/12 positive, median `2.766x` to `29.087x` | `adopted-by-user` for sampled boundaries |
| `correspondence-sorted-copy-scalar-double-production-probe` | correspondence sorted-copy double | production-public + production-detail | public positive but detail A/B negative | unchanged: `bounded_public_positive_detail_ab_negative` |

## 实现和测试动作

1. 新建 Phase 074 plan / result。
2. 刷新 phase README、optimization matrix、optimization roadmap、evaluation、optimization evidence、benchmark/evidence、correctness tests、test support code map、长期 `doc-rvv` 和 Handoff，把 Phase 069 / 070 / 071 从 pending 收口为 adopted。
3. 保留 Phase 072 / 073 的负向 family-selection 边界，不写入 clean adopted。
4. 运行 freshness 和格式验证：`run_test_compare`、`evidence_status`、`py_compile`、`git diff --check`。

## Evidence Doctor 和 registry 规则

本阶段不生成新的 raw board run；采用 Phase 069 / 070 / 071 已登记的接入后 production-public board summary 和 Doctor。Warnings 处理方式保持原结论：保留 min / median / max，不剔除异常，不扩大到全部 custom layout double 或任意自定义点型全集。

## 阶段完成条件

- Phase 069 / 070 / 071 在 matrix、roadmap、evaluation、topic-local docs、长期 `doc-rvv` 和 Handoff 中均为 `adopted-by-user`。
- Phase 072 / 073 仍为 `bounded_public_positive_detail_ab_negative` / `rejected_for_clean_adoption_with_detail_ab_negative`。
- fresh verification 通过或明确记录失败。

## 继续 / 停止条件

若验证通过且没有新的 `phase_deferred + unblocked` 性能路线，本轮停在 adoption closeout；后续只有用户要求接受 / 回滚 Phase 072 sorted-copy double、扩大 custom layout double 取样空间，或新增其它输入边界时再开新 phase。
