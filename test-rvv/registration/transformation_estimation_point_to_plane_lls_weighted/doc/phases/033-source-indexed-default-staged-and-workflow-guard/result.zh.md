# Phase 033 Result：source-indexed default staged and workflow guard

## 结论

Phase 033 已把 Phase 032 的 same-boundary production detail A/B 结论落实到当前默认生产路径：

- source-indexed production 默认路径回到 staged-gather / compressed-tail。
- `block-fused-abcd-ilp` production helper 保留为显式 probe / `production-source-indices-detail-ba` helper，不再作为默认优先 dispatch。
- EvidenceDecision 更新为：

```text
production-adopted/full-cloud-and-source-indexed-staged-f32-aos-valid-index-with-block-fused-probe-rejected-for-default
```

这不是撤销 Phase 031 的 public production probe 正向结果。Phase 031 仍证明真实 public source-indexed RVV path 比 public scalar path 快；Phase 032 证明它不能回答 block-fused 是否优于 staged。默认 family selection 因此回到 staged。

## 完成项

| item | 状态 | 结果 |
| --- | --- | --- |
| P1 phase plan | done | 新增本阶段 plan，边界固定为 source-indexed default、agent guard、docs 和 handoff。 |
| P2 production default | done | `buildPointToPlaneLLSWeightedSourceIndicesDefault` 和 `estimatePointToPlaneLLSWeightedSourceIndicesRVV` 改为只尝试 staged-gather RVV；失败后走 std。 |
| P3 correctness guard | done | 新增 RVV-only `ProductionSourceIndexedDefaultUsesStagedGatherRVV`，证明 default normal-equation 与 staged helper 完全一致。 |
| P4 agent assets | done | `rvv-test` 增加 diagnostic negative / public production positive 分叉规则：不能直接拒绝 probe，也不能用 public Std/RVV positive 证明新 RVV family 优于既有 adopted family；family selection 必须补同边界 production RVV-vs-RVV A/B。 |
| P5 docs refresh | done | README、testing overview、correctness、benchmark/evidence、optimization evidence、code map、evaluation、phase index 和 `doc-rvv` 均更新为 staged default / block-fused rejected-for-default。 |
| P6 validation | done | py_compile、QEMU gtest compare、board detail A/B dry-run 和 stale-text 扫描完成。 |
| P7 handoff | done | current handoff Markdown/YAML 已刷新为 Phase 033。 |

## 代码改动

| 文件 | 改动 |
| --- | --- |
| `registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls_weighted.hpp` | source-indexed default selector 和 public RVV wrapper 不再默认调用 block-fused；只尝试 staged-gather RVV，失败后 std。block-fused helper 保留。 |
| `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/src/test_teptplw_production_direct.cpp` | 新增 `ProductionSourceIndexedDefaultUsesStagedGatherRVV` 护栏测试。 |
| `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/Makefile` | `collect_board_production_source_indices_probe_repeated`、`refresh_board_production_source_indices_probe_repeated` 默认报错，避免 Phase 033 后把 staged public 结果误标为 block-fused probe；clean historical probe evidence 需要显式 opt-in。 |

## Agent 资产改动

| 文件 | 改动 |
| --- | --- |
| `.agents/skills/rvv-test/references/performance-and-ablation.zh.md` | 增加 comparison-boundary / baseline mismatch 规则。 |
| `.agents/skills/rvv-test/references/registration-topic-evidence.zh.md` | 增加 row source production probe 分叉后的 family-selection A/B 要求。 |

本轮还保留了已有 structure-parity 相关 agent 资产改动；它们属于当前工作树已有 agent 资产完善，不改变本阶段 source-indexed production 决策。

## 文档改动

| 文件 | 结果 |
| --- | --- |
| `README.zh.md` | 顶部结论、证据表和当前结果改为 staged default。 |
| `doc/testing-overview.zh.md` | 测试类型、board target、提交证据和结论边界改为 staged default；block-fused probe 只保留历史 probe / detail A/B。 |
| `doc/correctness-tests.zh.md` | 记录新护栏测试和最新 gtest 计数。 |
| `doc/benchmark-and-evidence.zh.md` | 当前 production evidence 改为 staged summary；block-fused probe 不再写成当前默认生产结论。 |
| `doc/optimization-evidence.zh.md` | 优化矩阵、代码索引和当前可提交证据改为 staged adopted/default。 |
| `doc/test-support-code-map.zh.md` | production 对照表改为 staged default selector，block-fused 为 explicit probe。 |
| `doc/transformation_estimation_point_to_plane_lls_weighted-evaluation.zh.md` | EvidenceDecision、Traceability Map、EvidenceDecision Audit 和遗留风险同步。 |
| `doc-rvv/registration/transformation_estimation_point_to_plane_lls_weighted-RVV.zh.md` | 长期 production 说明改为 full-cloud adopted + source-indexed staged default，block-fused rejected-for-default。 |
| `doc/phases/README.zh.md` | 默认恢复入口切到 Phase 033。 |

## 验证

| 命令 | 结果 |
| --- | --- |
| `python3 -m py_compile .../analyze_teptplw_rvv_ba.py .../collect_teptplw_board_rvv_ba.py .../collect_teptplw_board_compare_repeated.py .../generate_teptplw_evidence_manifest.py` | pass。 |
| `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted run_test_source_indices_compare run_test_production_direct_compare` | pass；source-indexed std 6 passed，RVV 8 passed；production_direct std 18 passed，RVV 20 passed。 |
| `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted run_test_compare` | pass；std 52 passed + 1 skipped，RVV 55 passed。 |
| `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted -n collect_board_production_source_indices_detail_ba` | pass；dry-run 展示仍调用 `production-source-indices-detail-ba`。 |
| `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted collect_board_production_source_indices_probe_repeated` | expected fail；Phase 031 historical probe guard 生效，防止误采。 |
| stale-text scan | pass；当前文档不再把 block-fused 写成默认优先 dispatch。历史 Phase 031/032 文档保留当时事实。 |

## 证据解释

当前 source-indexed 生产判断使用三层证据：

| 层 | 证据 | 结论 |
| --- | --- | --- |
| staged public Std/RVV | `production_source_indices_staged_gather/summary.md` | 当前默认生产证据；三类代表点型、65536/262144 点均正向。 |
| block-fused public Std/RVV | `production_source_indices_block_fused_abcd_ilp_probe/summary.md` | 历史 probe positive；只能证明 public RVV path 快于 public scalar path。 |
| block-fused vs staged RVV-vs-RVV | `production_source_indices_detail_ba/analyze_rvv_ba.md` | 同边界 family selection mixed / negative；block-fused rejected-for-default。 |

## Dirty Isolation

工作树仍包含非本阶段 / 非本 topic 的 dirty changes，例如：

- `doc-rvv/library-screening/registration/registration-module-second-pass.zh.md`
- `doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md`
- `registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls.hpp`
- `test-rvv/registration/transformation_estimation_point_to_plane_lls/`

这些文件未作为 Phase 033 结论依据，不应在本 topic 分类提交时误 stage 或 revert。

## 下一步

当前阶段可进入 review。若后续重新挑战 source-indexed block-fused 默认路径，应新建 phase，并至少补：

- extended production detail RVV-vs-RVV A/B。
- source-indexed-specific asm attribution。
- binary identity / build label / taskset metadata。
- 对 262144 长尾和 staged-vs-block 反转的解释。
