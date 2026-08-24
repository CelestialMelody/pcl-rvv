# Phase 075 Plan: correspondence sorted-copy `Scalar=double` rollback closeout

## 背景

Phase 072 把 correspondence sorted-copy（对应关系排序副本）扩到 `Scalar=double` public production probe（公开生产探针），public Std/RVV board repeated 为 positive：64K / 256K median B/A `5.727x` / `5.408x`，Doctor `0/0/0`。

Phase 073 随后补同一 production detail boundary（生产细节边界）内的 RVV-vs-RVV A/B：baseline 是既有 D64 gather RVV family，candidate 是 sorted-copy double。板卡 repeated 为 negative：64K / 256K median B/A `0.283x` / `0.431x`，Doctor `Errors=2`、`Warnings=1`、`Suggestions=0`。

用户已明确规则：同一边界下选择更好的实现；后续也按同边界更优实现收口。因此本阶段不接受 Phase 072 bounded risk，而是回滚 sorted-copy double 生产分流，保留更快的 D64 gather RVV path。

## 本阶段范围

本阶段只处理 Phase 072 / 073 的 `Scalar=double` correspondence sorted-copy family-selection：

- 移除 production header 中 `estimateRigidTransformationSVDScaleCorrespondencePairSortedCopyRVV(... Eigen::Matrix<double, 4, 4>&)` overload。
- 移除 double correspondence branch 中 sorted-copy helper 尝试；contiguous fast path 不命中后直接使用 D64 gather accumulation。
- 保留 `Scalar=float` sorted-copy helper 和 dispatch，因为 Phase 047 已采纳该分支。
- 更新当前 correctness guard（正确性保护）命名和注释，表达 shuffled correspondence double 当前走 D64 gather production path，并与 selected-cloud double reference 对齐。
- 更新 Phase README、optimization matrix、roadmap、topic-local docs、长期 `doc-rvv` 和 current Handoff，把 Phase 072 状态从 bounded candidate 收口为 rollback / no-production for double sorted-copy。

## 不做范围

- 不删除 Phase 072 / 073 的历史 bench target、summary 或 result；它们是本次决策证据。
- 不改变 Phase 069 / 070 / 071 已采纳的 `Scalar=double` generic / custom layout path。
- 不改变 Phase 047 `Scalar=float` correspondence sorted-copy production behavior。
- 不新开更多 custom layout double 或 sorted-copy 输入分布实验；本阶段只关闭当前决策边界。

## 证据计划

1. `make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare`
   - 证明 rollback 后 Std/RVV correctness 仍通过。
2. `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_correctness_state`
   - 刷新当前 production correctness registry；QEMU 只证明正确性和日志形状，不写性能结论。
3. `make -C test-rvv/registration/transformation_estimation_svd_scale evidence_status`
   - 检查 evidence registry fresh。
4. `python3 -m py_compile test-rvv/registration/transformation_estimation_svd_scale/script/*.py`
   - 检查 topic-local scripts。
5. `git diff --check -- ...`
   - 检查 whitespace。

## EvidenceDecision 预期

若 correctness 和 registry 均通过，本阶段写成：

`rolled_back_no_production_for_scalar_double_sorted_copy`

含义：

- Phase 072 public Std/RVV positive 作为历史事实保留。
- Phase 073 detail A/B negative 是 family-selection 的决定性证据。
- 当前 production behavior 对 `Scalar=double` shuffled correspondence 使用 D64 gather RVV family；不再尝试 sorted-copy double。
- `Scalar=float` sorted-copy production behavior 不受影响。

## 停止 / 继续条件

- 若 rollback 后 correctness 失败：暂停并诊断，不更新 adopted / rollback closeout。
- 若 verification 通过：同步所有当前文档和 Handoff，然后扫描 roadmap 是否还有当前授权内、未阻塞且值得推进的优化路线。
- 若剩余路线都需要新的输入分布定义、board budget 或扩大 topic scope：停止并报告原因。
