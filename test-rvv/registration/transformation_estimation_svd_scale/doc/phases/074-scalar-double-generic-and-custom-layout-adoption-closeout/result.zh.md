# Phase 074 Result: scalar-double generic and custom layout adoption closeout

## 结论

EvidenceDecision：`adopted-by-user` for Phase 069、Phase 070 and Phase 071。

用户确认“板卡上的测试结果如果显示有收益即可采纳；接入后也需要进行测试；正式 `doc-rvv` 使用接入后的板卡测试数据”后，本阶段把三条已完成 production-public board evidence 的 `Scalar=double` candidate 收口为 adopted production behavior：

- Phase 069 `row-source-generic-scalar-double-production-probe`
- Phase 070 `custom-layout-scalar-double-diagnostic-scout`
- Phase 071 `more-custom-layout-scalar-double-sampling`

Phase 072 / 073 不在本阶段采纳范围内：sorted-copy double 的 public Std/RVV probe 为正向，但同边界 RVV-vs-RVV detail A/B 为负向，仍不支持 clean adoption。

## 采纳范围

| adopted boundary | scope | board evidence | Doctor |
| --- | --- | --- | --- |
| Phase 069 row-source generic double | source-indexed、dual-indexed、correspondence / common PCL xyz AoS whitelist / `Scalar=double` / dense / 64K | 9/9 positive，median B/A `11.309x` 到 `17.433x` | `Errors=0`、`Warnings=4`、`Suggestions=0` |
| Phase 070 custom layout double scout | `LocalSVDScalePaddedXYZSource -> LocalSVDScaleWideXYZTarget` / ordered + three row-source overloads / `Scalar=double` / dense / 64K | 4/4 positive，median B/A `9.966x` 到 `27.497x` | `Errors=0`、`Warnings=1`、`Suggestions=0` |
| Phase 071 more custom layout double sampling | compact、huge-padding、aligned registered custom layout samples / ordered + three row-source overloads / `Scalar=double` / dense / 64K | 12/12 positive，median B/A `2.766x` 到 `29.087x` | `Errors=0`、`Warnings=8`、`Suggestions=0` |

## 不采纳 / 不外推范围

- 不把 Phase 069 外推到全部 custom layout double、非法 index / correspondence、non-dense、小规模或任意自定义点型全集。
- 不把 Phase 070 / 071 外推到全部 custom layout double、packed unaligned float、异常 alignment 全集或 sorted-copy double。
- 不把 Phase 072 sorted-copy double 写成 clean adopted；Phase 073 detail A/B 已显示 64K / 256K median B/A `0.283x` / `0.431x`，慢于既有 D64 gather RVV family。

## Evidence Doctor 处理

Phase 069 的 4 个 warning、Phase 070 的 1 个 warning 和 Phase 071 的 8 个 warning 均为 long-tail / variance 或 group-outlier。处理方式：

- 保留 min / median / max，不剔除异常。
- 只按本阶段列出的 row source、layout sample、size 和 `Scalar=double` 边界解释。
- 不改变 positive bucket，也不扩大到未验证 layout 或点型全集。

## 文档同步

本阶段刷新：

- `doc-rvv/registration/transformation_estimation_svd_scale-RVV.zh.md`
- `test-rvv/registration/transformation_estimation_svd_scale/doc/transformation_estimation_svd_scale-evaluation.zh.md`
- `test-rvv/registration/transformation_estimation_svd_scale/doc/optimization-evidence.zh.md`
- `test-rvv/registration/transformation_estimation_svd_scale/doc/benchmark-and-evidence.zh.md`
- `test-rvv/registration/transformation_estimation_svd_scale/doc/correctness-tests.zh.md`
- `test-rvv/registration/transformation_estimation_svd_scale/doc/test-support-code-map.zh.md`
- `test-rvv/registration/transformation_estimation_svd_scale/doc/optimization-roadmap.zh.md`
- `test-rvv/registration/transformation_estimation_svd_scale/doc/phases/optimization-matrix.zh.md`
- `test-rvv/registration/transformation_estimation_svd_scale/doc/phases/README.zh.md`
- `tmp/rvv-work-logs/registration/transformation_estimation_svd_scale/current-handoff/`

## 最终验证

Phase 074 closeout 后已重新运行接入后的 correctness（正确性）和收口门禁：

- `make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare`：Std 38/38 passed，RVV 38/38 passed。
- `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_correctness_state`：重新登记 `log/qemu/run_test_std.log` 和 `log/qemu/run_test_rvv.log`。
- `make -C test-rvv/registration/transformation_estimation_svd_scale evidence_status`：`evidence registry check: fresh`。
- `python3 -m py_compile test-rvv/registration/transformation_estimation_svd_scale/script/*.py`：passed。
- `git diff --check -- registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp registration/include/pcl/registration/transformation_estimation_svd_scale.h test-rvv/registration/transformation_estimation_svd_scale doc-rvv/registration/transformation_estimation_svd_scale-RVV.zh.md tmp/rvv-work-logs/registration/transformation_estimation_svd_scale/current-handoff`：passed。

## Continue / Stop Decision

`continue_stop_decision`: `turn_stop_closeout_verified_phase_072_user_decision_required`。

`next_phase_default`: none unless user authorizes a Phase 072 bounded-risk acceptance / rollback closeout, or requests broader custom layout double / point-type / input-distribution expansion.
