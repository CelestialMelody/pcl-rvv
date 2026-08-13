# correspondence_types board repeated summary

## 运行合同

- run root：`log/board/011-production-probe-boundary-check/production-index-extract`
- runs：run-001, run-002, run-003, run-004, run-005
- device：Milkv-Jupiter
- case-filter：`production-index-extract`
- evidence_role：`production_direct_probe`，production direct probe（真实生产路径探针），需要 Evidence Doctor 和反汇编归属共同解释。
- B/A：`Std ms / RVV ms`，大于 1 表示 RVV 更快。
- manifest（本地机器输入，默认不提交）：`log/board/011-production-probe-boundary-check/production-index-extract/evidence_manifest.json`
- Evidence Doctor：`log/board/011-production-probe-boundary-check/production-index-extract/evidence_doctor.md`

## 汇总

| case | runs | speedup values | median | min | max | checksum | bucket |
| --- | ---: | --- | ---: | ---: | ---: | --- | --- |
| match-index production 64K | 5 | `1.003, 0.918, 0.983, 0.907, 1.018` | 0.983 | 0.907 | 1.018 | match | `negative` |
| query+match production 256K | 5 | `0.966, 0.953, 0.972, 0.954, 0.984` | 0.966 | 0.953 | 0.984 | match | `negative` |
| query-index production 4K | 5 | `0.877, 0.729, 0.863, 1.117, 0.895` | 0.877 | 0.729 | 1.117 | match | `negative` |

## 决策

5 轮板卡 `production-index-extract` 中，index extraction（索引抽取）的结论按上表 bucket 判断。
若主 case 没有达到 `weak_positive`，本摘要不支持保留 production probe（生产路径探针）。
若达到 `weak_positive`，仍需结合 correctness、asm attribution 和 Evidence Doctor 再做生产接入判断。
