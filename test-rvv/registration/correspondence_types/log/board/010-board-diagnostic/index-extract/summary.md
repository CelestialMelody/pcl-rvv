# correspondence_types board repeated summary

## 运行合同

- run root：`log/board/010-board-diagnostic/index-extract`
- runs：run-001, run-002, run-003, run-004, run-005
- device：Milkv-Jupiter
- case-filter：`index-extract`
- evidence_role：`diagnostic`，不是 production direct（真实生产路径证据）。
- B/A：`Std ms / RVV ms`，大于 1 表示 RVV 更快。
- manifest（本地机器输入，默认不提交）：`log/board/010-board-diagnostic/index-extract/evidence_manifest.json`
- Evidence Doctor：`log/board/010-board-diagnostic/index-extract/evidence_doctor.md`

## 汇总

| case | runs | speedup values | median | min | max | checksum | bucket |
| --- | ---: | --- | ---: | ---: | ---: | --- | --- |
| match-index candidate 64K | 5 | `0.983, 0.989, 0.934, 0.933, 0.959` | 0.959 | 0.933 | 0.989 | match | `negative` |
| query+match candidate 256K | 5 | `0.966, 0.967, 0.953, 0.960, 0.948` | 0.960 | 0.948 | 0.967 | match | `negative` |
| query-index candidate 4K | 5 | `0.966, 0.987, 1.048, 0.987, 0.872` | 0.987 | 0.872 | 1.048 | match | `negative` |

## 决策

5 轮板卡诊断中，index extraction（索引抽取）没有达到 `weak_positive`。
综合 64K match 和 256K query+match 主 case，RVV 多数慢于标量，当前 decision bucket 为 `negative`。
因此本摘要不支持进入 production integration loop（生产接入闭环）。
