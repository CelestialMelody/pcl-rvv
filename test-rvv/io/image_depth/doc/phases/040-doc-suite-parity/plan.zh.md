# Phase 040 计划：doc-suite parity

## 阶段意图和边界

本阶段补齐 topic-local doc suite（主题本地文档套件）中缺失的读者路径、测试总览、正确性说明、bench/evidence、优化证据索引和代码地图。只修改 `test-rvv/io/image_depth` 文档，不修改 production 或 C++ 测试行为。

## 当前状态清单

| area | 当前状态 | 本阶段动作 |
| --- | --- | --- |
| topic navigation | 缺少 topic 根 README | 新增 `README.zh.md`。 |
| testing overview | 缺少独立 target 粒度审计 | 新增 `doc/testing-overview.zh.md`。 |
| correctness tests | evaluation 中只有摘要 | 新增 `doc/correctness-tests.zh.md`。 |
| benchmark and evidence | summary / doctor 存在，但缺少 case 字典文档 | 新增 `doc/benchmark-and-evidence.zh.md`。 |
| optimization evidence | matrix 存在，但缺少候选证据索引 | 新增 `doc/optimization-evidence.zh.md`。 |
| test-support code map | evaluation 有 Traceability Map，但缺少 test support 代码地图 | 新增 `doc/test-support-code-map.zh.md`。 |

## 执行步骤

1. 新增 role 文档，保持简短，避免复制 raw logs。
2. 更新 phase index、evaluation、roadmap 和 matrix，说明 040 的 doc-suite adopted 状态。
3. 运行 `make evidence_status`、`git diff --check`、尾随空白扫描和路径限定 `git status`。

## 完成条件

所有新增文档存在，并由 README / phase suite / evaluation 可定位。Evidence registry 仍 fresh，且不产生 production 源码 diff。
