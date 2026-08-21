# image_depth phase index

| phase | status | 默认恢复动作 | plan | result |
| --- | --- | --- | --- | --- |
| 000-current-state-and-diagnostic-scaffold | complete | 已形成 production-shaped diagnostic 证据；结论为 partial-production-candidate | `000-current-state-and-diagnostic-scaffold/plan.zh.md` | `000-current-state-and-diagnostic-scaffold/result.zh.md` |
| 010-production-integration-plan | plan_ready / waits_user_authorization_for_PI2 | 用户授权 production integration loop 后，从 PI2 production patch 恢复 | `010-production-integration-plan/plan.zh.md` | `010-production-integration-plan/result.zh.md` |
| 020-stride-load-downsample-diagnostic | complete | `xStep > 1` downsample RVV `vlse16` 候选形成 production-shaped partial-production-candidate；继续生产接入需用户授权 PI2 | `020-stride-load-downsample-diagnostic/plan.zh.md` | `020-stride-load-downsample-diagnostic/result.zh.md` |
| 030-production-scope-refresh-and-stop-audit | complete / waits_user_authorization_for_PI2 | PI1 已刷新为 contiguous + downsample production probe 候选；raw / OpenNI 已审计为 deferred；继续需用户授权 production patch | `030-production-scope-refresh-and-stop-audit/plan.zh.md` | `030-production-scope-refresh-and-stop-audit/result.zh.md` |
| 040-doc-suite-parity | complete / waits_user_authorization_for_PI2 | 补齐 topic-local README、testing overview、correctness、benchmark/evidence、optimization evidence 和 code map；继续需用户授权 production patch | `040-doc-suite-parity/plan.zh.md` | `040-doc-suite-parity/result.zh.md` |
| 050-production-public-probe | complete / adopted_after_user_confirmation | 已接入 `io/src/image_depth.cpp` production patch 并完成 production-public 测试、asm、board repeated、Evidence Doctor；用户已确认采纳有收益的优化，S11 已创建长期 `doc-rvv` 文档 | `050-production-public-probe/plan.zh.md` | `050-production-public-probe/result.zh.md` |

当前 topic 已成为 adopted production behavior（已采用生产行为）：保留 depth contiguous、disparity contiguous 和 disparity downsample RVV；depth downsample 保持标量 fallback。长期文档为 `doc-rvv/io/image_depth-RVV.zh.md`。

当前可复查证据入口：

- `make record_board_repeated_evidence_state`：重建 phase 000 repeated board summary、manifest、Evidence Doctor，并登记 `log/evidence_registry.json`。
- `make record_board_downsample_evidence_state`：重建 phase 020 downsample repeated board summary、manifest、Evidence Doctor，并登记 `log/evidence_registry.json`。
- `make record_board_production_evidence_state`：重建 phase 050 production-public repeated summary、manifest、Evidence Doctor，并登记 `log/evidence_registry.json`。
- `make evidence_status`：检查 repeated board 摘要证据是否仍与 registry 和文档引用一致。

topic-local doc suite 入口：

- `../../README.zh.md`
- `../testing-overview.zh.md`
- `../correctness-tests.zh.md`
- `../benchmark-and-evidence.zh.md`
- `../optimization-evidence.zh.md`
- `../test-support-code-map.zh.md`
