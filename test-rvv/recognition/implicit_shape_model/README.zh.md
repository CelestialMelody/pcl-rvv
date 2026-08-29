# recognition/implicit_shape_model RVV 主题

本目录保存 `recognition/include/pcl/recognition/impl/implicit_shape_model.hpp`
的 RVV（RISC-V Vector，可变长度向量扩展）专项测试资产。当前 topic token
为 `ism`，完整主题名为 `implicit_shape_model`。

## 当前结论

- 当前 phase：`020-production-integration-findobjects-public-entry`
- 当前证据角色：production direct（真实生产路径证据）
- production 状态：窄范围 adopted production behavior（已采纳生产行为）
- `doc-rvv` 状态：applicable；正式长期文档为
  `doc-rvv/recognition/implicit_shape_model-RVV.zh.md`

生产源码已经在 `findObjects()` 中把 descriptor-to-cluster assignment
（描述子到聚类中心最近邻分配）抽成 `pcl::ism::detail::findNearestClusterIndexStd/RVV`
helper。`__RVV10__` 构建使用 RVV helper，非 RVV 构建使用 Std helper；`153` 只是当前板卡证据里
的代表性 `FeatureSize`，不是生产分流门禁。当前采纳只覆盖 `findObjects()` 公开入口中的 nearest cluster assignment；
`trainISM()`、`calculateSigmas()`、`calculateWeights()`、vote density（投票密度）和泛型点类型扩展
仍保持未验证或标量边界。

Phase 020 的 production direct repeated board（真实生产路径重复板卡测试）结果为
`public_find_objects_descriptor_assignment` 5/5 同向：median `1.060x`，min `1.040x`，
max `1.070x`，`B/A < 1` 为 `0/5`，Evidence Doctor（证据体检）为
`Errors=0 / Warnings=0 / Suggestions=2`。收益属于 weak positive（弱正向），但当前 patch
实现小、fallback 简单、语义风险低，且本轮 prompt 明确允许“板卡有收益即可采纳”，因此进入窄范围生产采纳。

## 阅读顺序

1. `doc-rvv/recognition/implicit_shape_model-RVV.zh.md`：长期 production 行为、fallback、证据链和未覆盖范围。
2. `doc/implicit_shape_model-evaluation.zh.md`：函数级评估、Phase 000-020 取舍和 production decision。
3. `doc/phases/020-production-integration-findobjects-public-entry/result.zh.md`：本阶段执行事实、EvidenceDecision 和 doc-suite closeout。
4. `doc/benchmark-and-evidence.zh.md`：bench（性能测试）、case-filter、board repeated 和 Evidence Doctor 说明。
5. `doc/testing-overview.zh.md`：测试入口、target 粒度和证据边界。
6. `doc/correctness-tests.zh.md`：correctness（正确性）测试说明。
7. `doc/optimization-evidence.zh.md`：候选公式的 adopted / attempted / deferred 状态。
8. `doc/test-support-code-map.zh.md`：测试支撑代码地图。
9. `doc/optimization-roadmap.zh.md` 和 `doc/phases/optimization-matrix.zh.md`：后续候选和阶段矩阵。

## 常用命令

```bash
make -C test-rvv/recognition/implicit_shape_model run_test_compare
make -C test-rvv/recognition/implicit_shape_model run_upstream_test_compare
make -C test-rvv/recognition/implicit_shape_model check_ism_rvv_asm
SSH_AUTH_SOCK=<agent-forwarded-sock> make -C test-rvv/recognition/implicit_shape_model public_entry_board_repeated
make -C test-rvv/recognition/implicit_shape_model record_evidence_state_public_entry
make -C test-rvv/recognition/implicit_shape_model check_public_entry_evidence_freshness
```

QEMU 只用于正确性、构建和日志形状；性能结论只来自 board（板卡）或目标硬件。

## 当前可提交证据白名单

当前可提交候选包括 topic-local 文档、测试支撑源码、summary / manifest /
Evidence Doctor / registry 级摘要证据，以及正式长期文档
`doc-rvv/recognition/implicit_shape_model-RVV.zh.md`。默认不提交：

- `log/board/**/run_*` raw logs（原始日志）。
- `log/qemu/`、`build/`、本机 `config.mk`。
- 私有 board 地址、远端目录和 SSH 信息。

当前 production direct 主证据路径：

- summary：`test-rvv/recognition/implicit_shape_model/log/board/repeated_phase020_public_entry_findobjects_production_direct/summary.md`
- manifest：`test-rvv/recognition/implicit_shape_model/log/board/repeated_phase020_public_entry_findobjects_production_direct/evidence_manifest.json`
- doctor：`test-rvv/recognition/implicit_shape_model/log/board/repeated_phase020_public_entry_findobjects_production_direct/evidence_doctor.md`
- registry：`test-rvv/recognition/implicit_shape_model/log/evidence_registry.json`

## production_topic_doc 适用性

`doc-rvv/recognition/implicit_shape_model-RVV.zh.md` 已适用，因为 Phase 020 完成了 production
patch、公开入口 correctness、反汇编归属、production direct repeated board 和 Evidence Doctor。
该长期文档只说明当前已采纳的 `findObjects()` descriptor assignment RVV 路径；历史 diagnostic
和其它候选的完整解释仍归属 topic-local evaluation、phase result、roadmap 和矩阵。
