# Phase 004：structure parity doc suite

## 目标

按 `.agents` 的 doc-suite parity closeout gate，把 ICP topic-local 文档对齐到成熟
`transformation_estimation_point_to_plane_lls_weighted` topic 的读者路径和证据分工。只迁移结构经验，不复制算法、
性能结论或文件切分粒度。

## 计划动作

| action | 完成判据 | 状态 |
| --- | --- | --- |
| parity audit | 用 `area / current shape scan / mature sibling quality bar / decision / evidence / next action` 表审计。 | planned |
| README navigation | README 补齐“先读哪份文档、目录分工、常用命令、当前可提交证据、默认不提交生成产物、当前结果”。 | planned |
| topic-local docs | 新增 testing overview、correctness tests、benchmark/evidence、optimization evidence 和 test-support code map。 | planned |
| evaluation split | evaluation 聚焦 EvidenceDecision、Traceability Map、文档分工、风险和质量门禁。 | planned |
| long-term doc split | `doc-rvv` 只保留 adopted production 行为、当前优化方式、dispatch/fallback、证据链和长期边界。 | planned |
| phase / roadmap update | phase index、optimization matrix 和 roadmap 记录文档对齐结果。 | planned |

## 证据边界

本 phase 是 docs-only，不修改 production code、test code、bench harness 或证据 raw logs。不运行 QEMU
`run_bench_compare`；若需要验证，只做 Markdown diff / whitespace 检查。已有性能结论继续只引用 board repeated
summary。

## 停止条件

- 发现 mature sibling 结构要求与当前 `.agents` 指令冲突，先修 agent 指令。
- 发现 ICP 文档需要引用缺失证据，先降级结论或标出 evidence gap。
- dirty isolation 显示待改文档与其它 topic 未提交工作重叠，则停止等待人工拆分。
