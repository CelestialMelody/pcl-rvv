# transformation_estimation_dual_quaternion board repeated summary

## 运行合同

- run_label：`row_source_expansion_dual_indexed_repeated`
- runs：`run-01, run-02, run-03, run-04, run-05`
- device：`Milkv-Jupiter`
- case-filter：`row-source-expansion`
- evidence_role：`diagnostic`；当前摘要只覆盖 `dual-indexed-cloud-pair`，同边界 Std/RVV 对比不是 production direct（真实生产路径证据）。
- iterations：`20`
- warm-up iterations：`5`
- B/A：`Std staged dual-indexed-cloud-pair ms / RVV staged dual-indexed-cloud-pair ms`，大于 1 表示 RVV candidate 更快。
- manifest（默认本地证据清单）：`log/board/row_source_expansion_dual_indexed_repeated/evidence_manifest.json`
- Evidence Doctor：`log/board/row_source_expansion_dual_indexed_repeated/evidence_doctor.md`

## dual-indexed-cloud-pair staged candidate Std/RVV（同边界）

这一表只比较 dual-indexed-cloud-pair 将 row 展开并暂存为 ordered cloud 后复用 C1/C2 RVV candidate 的 Std/RVV 构建。index / correspondence 展开和 staging 成本包含在 bench 计时边界内；它仍是 test-rvv 诊断证据。

| size | values | median | min | max | bucket | checksum |
| --- | --- | ---: | ---: | ---: | --- | --- |
| 4K | `1.160, 1.136, 1.257, 1.180, 1.208` | 1.180 | 1.136 | 1.257 | `weak_positive` | same |
| 64K | `1.263, 1.328, 1.270, 1.312, 1.286` | 1.286 | 1.263 | 1.328 | `positive` | same |
| 256K | `1.151, 1.175, 1.163, 1.149, 1.160` | 1.160 | 1.149 | 1.175 | `weak_positive` | same |

## 决策桶

- overall_decision_bucket：`weak_positive`（默认按 64K / 256K 等非 4K 目标规模判断；4K 保留为小规模诊断）。
- `positive` / `weak_positive` 只能按当前 mode 的证据边界解释；indexed / correspondences 不从 ordered full-cloud 自动继承。
- 当前 positive / weak_positive 只批准该 row source 的诊断候选；其它 policy 不继承本摘要结论，也不能直接进入 production。
