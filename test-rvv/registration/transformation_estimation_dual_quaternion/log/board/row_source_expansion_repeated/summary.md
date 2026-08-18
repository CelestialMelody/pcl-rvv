# transformation_estimation_dual_quaternion board repeated summary

## 运行合同

- run_label：`row_source_expansion_repeated`
- runs：`run-01, run-02, run-03, run-04, run-05`
- device：`Milkv-Jupiter`
- case-filter：`row-source-expansion`
- evidence_role：`diagnostic`；当前摘要只覆盖 `source-indexed-cloud-pair`，同边界 Std/RVV 对比不是 production direct（真实生产路径证据）。
- iterations：`20`
- warm-up iterations：`5`
- B/A：`Std staged source-indexed-cloud-pair ms / RVV staged source-indexed-cloud-pair ms`，大于 1 表示 RVV candidate 更快。
- manifest（默认本地证据清单）：`log/board/row_source_expansion_repeated/evidence_manifest.json`
- Evidence Doctor：`log/board/row_source_expansion_repeated/evidence_doctor.md`

## source-indexed-cloud-pair staged candidate Std/RVV（同边界）

这一表只比较 source-indexed-cloud-pair 将 row 展开并暂存为 ordered cloud 后复用 C1/C2 RVV candidate 的 Std/RVV 构建。index / correspondence 展开和 staging 成本包含在 bench 计时边界内；它仍是 test-rvv 诊断证据。

| size | values | median | min | max | bucket | checksum |
| --- | --- | ---: | ---: | ---: | --- | --- |
| 4K | `1.618, 1.556, 1.529, 1.443, 1.419` | 1.529 | 1.419 | 1.618 | `positive` | same |
| 64K | `1.419, 1.436, 1.423, 1.402, 1.418` | 1.419 | 1.402 | 1.436 | `positive` | same |
| 256K | `1.409, 1.417, 1.415, 1.404, 1.411` | 1.411 | 1.404 | 1.417 | `positive` | same |

## 决策桶

- overall_decision_bucket：`positive`（默认按 64K / 256K 等非 4K 目标规模判断；4K 保留为小规模诊断）。
- `positive` / `weak_positive` 只能按当前 mode 的证据边界解释；indexed / correspondences 不从 ordered full-cloud 自动继承。
- 当前 positive / weak_positive 只批准该 row source 的诊断候选；其它 policy 不继承本摘要结论，也不能直接进入 production。
