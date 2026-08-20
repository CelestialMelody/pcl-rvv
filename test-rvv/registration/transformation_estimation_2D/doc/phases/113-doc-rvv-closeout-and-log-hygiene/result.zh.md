# Phase 113: doc-rvv closeout and log hygiene result

## 执行范围

本阶段执行 `113-doc-rvv-closeout-and-log-hygiene/plan.zh.md`。实际范围与计划一致：

- 重构 production 长期主题文档 `doc-rvv/registration/transformation_estimation_2D-RVV.zh.md`。
- 同步 phase README、optimization roadmap 和 optimization matrix。
- 将 TE2D 下早先被 Git 跟踪的 generated logs 从索引移除；本地 evidence files 保留。
- 把 Phase 113 plan/result 加入 `evidence_status` 文档输入，避免本阶段文档逃逸 freshness scan。

未修改 production source，未新增 RVV candidate，未改变 Phase 091 / Phase 112 adopted exact gate，也未把 Phase 103/104 guarded probe 或 Phase 106 source-indexed generic widening 写成 adopted。

## Production Doc Closeout Gate 回填

| area | result | evidence |
| --- | --- | --- |
| 当前状态 | completed | 长期文档新增当前 production status 表，列出 ordered generic、source-indexed exact `PointXYZ`、source-indexed exact `PointXYZI`、dual-indexed exact `PointXYZ` adopted / retained，correspondence scalar-only。 |
| 函数语义 | completed | 长期文档新增 public row source、`ConstCloudIterator` 标量路径、centroid / demean / correlation / 2D solve / matrix 输出说明。 |
| 当前采用的优化方式 | completed | 长期文档新增 two-pass centered 2D correlation accumulator 说明，覆盖 dispatch、fallback、VL chunk、load、reduction、FMA 和 scalar tail。 |
| 范围决策表 | completed | 长期文档把 adopted、guarded、rejected、scalar-only 和 unvalidated 范围分表列出。 |
| Traceability Map | completed | 长期文档新增 production entry/helper、test、bench、script、registry、phase、evaluation 和 matrix 的可追踪性地图。 |
| 数值算例 / VL chunk | completed | 长期文档新增 4 点示例，说明 pass 1 centroid 和 pass 2 `H00/H01/H10/H11` 累加。 |
| Bench 与证据 | completed | 长期文档按 evidence role 区分 correctness、QEMU/asm、board repeated、Doctor 和 negative evidence。 |
| 正确性与高效性证据链 | completed | 长期文档新增 correctness / QEMU / board / Doctor / fallback 分层表。 |
| production closeout | completed | 长期文档新增文件、helper、dispatch、adopted scope、rolled back scope、not adopted scope 和 log policy。 |

## Log Hygiene 回填

| 检查 | result |
| --- | --- |
| tracked log list | `git rm --cached` 已移除 15 个 TE2D tracked generated log 文件；`git ls-files test-rvv/registration/transformation_estimation_2D/log` 输出为空。 |
| local file preservation | 抽查 `log/qemu/production_public/asm_attribution.md` 和 `log/board/row_source_fused_repeated/summary.md` 仍在本地。 |
| ignore behavior | `test-rvv/.gitignore` 已有 `registration/transformation_estimation_2D/log/**`；本阶段不需要新增 ignore 规则。 |
| commit boundary | 预期提交只包含文档、Makefile evidence_status 输入、phase 113 和 tracked-log deletion。 |

从版本库删除的 generated logs：

```text
test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_public_repeated/evidence_doctor.md
test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_public_repeated/summary.md
test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_repeated/evidence_doctor.md
test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_repeated/summary.md
test-rvv/registration/transformation_estimation_2D/log/board/row_source_fused_repeated/evidence_doctor.md
test-rvv/registration/transformation_estimation_2D/log/board/row_source_fused_repeated/summary.md
test-rvv/registration/transformation_estimation_2D/log/qemu/asm_attribution.json
test-rvv/registration/transformation_estimation_2D/log/qemu/asm_attribution.md
test-rvv/registration/transformation_estimation_2D/log/qemu/evidence_doctor.md
test-rvv/registration/transformation_estimation_2D/log/qemu/production_public/asm_attribution.json
test-rvv/registration/transformation_estimation_2D/log/qemu/production_public/asm_attribution.md
test-rvv/registration/transformation_estimation_2D/log/qemu/production_public/evidence_doctor.md
test-rvv/registration/transformation_estimation_2D/log/qemu/row_source/asm_attribution.json
test-rvv/registration/transformation_estimation_2D/log/qemu/row_source/asm_attribution.md
test-rvv/registration/transformation_estimation_2D/log/qemu/row_source/evidence_doctor.md
```

## 过程改进结论

本次问题的直接原因不是 `rvv-documentation` 未触发，而是执行时把 Production Doc Closeout Gate 降级成了 small freshness sync。当前 topic 的改进方式是把 closeout gate 写入 Phase 113 的阶段完成条件，并在 result 中逐项回填。后续遇到用户确认采纳 / 准备提交 / production patch 已保留时，长期 `doc-rvv` 必须先通过 closeout gate；如果长期文档只有小 diff，应默认可疑，必须解释为什么不需要大改或标记 `doc_closeout_pending`。

通用 `.agents` 资产当前按 defaults 的 `agent_assets.feedback_mode: report-only` 处理，本阶段不自动修改 `.agents`。建议后续 workflow improvement patch 在 `rvv-documentation` 或 phase loop 中增加一句显式规则：production adoption closeout 不能只由 freshness sync 关闭，必须记录 closeout gate 审计表。

## 验证

| command | result |
| --- | --- |
| `make -C test-rvv/registration/transformation_estimation_2D evidence_status` | pass：`evidence registry check: fresh`，并已把 Phase 113 plan/result 纳入 doc input list。 |
| `git ls-files test-rvv/registration/transformation_estimation_2D/log` | pass：输出为空，TE2D generated logs 不再被 Git 跟踪。 |
| `git ls-files --others --exclude-standard -- test-rvv/registration/transformation_estimation_2D/log` | pass：输出为空，本地 generated logs 被 ignore 规则接管。 |
| `git diff --check -- doc-rvv/registration/transformation_estimation_2D-RVV.zh.md test-rvv/registration/transformation_estimation_2D tmp/rvv-work-logs/registration/transformation_estimation_2D` | pass：无 whitespace error。 |
| `python3 -c 'import yaml; yaml.safe_load(open("tmp/rvv-work-logs/registration/transformation_estimation_2D/current-handoff/current-handoff.yaml")); print("ok")'` | pass：输出 `ok`。 |

## Continue / stop decision

`continue_stop_decision`: continue to verification and follow-up topic-only commit if all checks pass.

`next_phase_default`: after commit, no default Normal / correspondence optimization phase. Future performance exploration requires explicit new phase and independent evidence label.
