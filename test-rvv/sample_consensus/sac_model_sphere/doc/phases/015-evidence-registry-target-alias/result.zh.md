# Phase 015: evidence registry target alias 结果

## 执行摘要

本阶段补齐了当前 topic 的 repeated Evidence Doctor（重复证据体检）和 evidence registry（证据登记表）Make target，没有重新跑板卡，也没有修改 production（生产源码）。Phase 000 的 repeated board summary 现在可以通过正式 target 重建 manifest、生成 Markdown / JSON doctor report，并登记到 `log/evidence_registry.json`。

## 文件和 target 变化

| area | 变化 | 状态 |
| --- | --- | --- |
| Makefile evidence 变量 | 新增 `REPEATED_EVIDENCE_MANIFEST`、`REPEATED_EVIDENCE_DOCTOR_MD`、`REPEATED_EVIDENCE_DOCTOR_JSON`、`EVIDENCE_REGISTRY` 和 doc refs。 | adopted |
| `run_repeated_board_evidence_doctor` | 先调用 topic-local manifest wrapper，再调用全局 Evidence Doctor。 | adopted |
| `record_repeated_board_evidence_state` | 登记 manifest、doctor markdown 和 doctor json。 | adopted |
| `repeated_evidence_status` | 扫描 summary evidence 和 doc refs，要求 registry fresh。 | adopted |

## 验证命令

```bash
make -C test-rvv/sample_consensus/sac_model_sphere record_repeated_board_evidence_state repeated_evidence_status
```

结果：命令 exit 0；输出显示重新写入 `repeated-evidence-manifest.json`、`repeated-evidence-doctor.md`、`repeated-evidence-doctor.json`，登记 3 个 evidence 文件，并返回 `evidence registry check: fresh`。

## Evidence Doctor 状态

当前 doctor 结果仍为 `Errors=3, Warnings=0, Suggestions=1`。这些 Error 已在 Phase 000 result 中解释并用于降级 / 拒绝对应证据边界：public select/getDistances 不写成 RVV 收益，当前 getDistances candidate 为 negative，select candidate 保持 `partial-production-candidate`。

## Artifact tracking

`doc/phases/000-sphere-select-distance-diagnostic/repeated-evidence-doctor.json` 和 `log/evidence_registry.json` 是本阶段新增 summary/registry 产物。`log/evidence_registry.json` 属于 evidence freshness metadata（证据新鲜度元数据）；raw board logs 仍默认 local-only，不进入默认提交边界。

## Continue / stop decision

Phase 015 当前完成，`stop_condition_hit=production_authorization_required`。topic 内测试资产、doc suite、manifest、doctor 和 registry freshness 已闭合到当前可授权范围。下一步若用户明确授权修改 production header，则按 `020-select-production-integration-plan/plan.zh.md` 进入 PI2；否则保持当前 diagnostic / partial-production-candidate 边界。
