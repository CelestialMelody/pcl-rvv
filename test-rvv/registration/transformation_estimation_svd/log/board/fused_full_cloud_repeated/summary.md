# transformation_estimation_svd board repeated summary

## 运行合同

- run_label：`fused_full_cloud_repeated`
- runs：`run-01, run-02, run-03, run-04, run-05`
- device：`RVV board`
- case-filter：`all`
- evidence_role：`diagnostic`
- iterations：`20`
- warm-up iterations：`5`
- B/A：`baseline ms / candidate ms`，大于 1 表示 candidate 更快。
- manifest（默认本地证据清单）：`log/board/fused_full_cloud_repeated/evidence_manifest.json`
- Evidence Doctor：`log/board/fused_full_cloud_repeated/evidence_doctor.md`

- 诊断模式说明：不是 production direct（真实生产路径证据）。

## public baseline vs fused RVV（混合边界）

这一表用当前 public Umeyama Std 作为 baseline，用 test-only fused RVV 作为 candidate。它只回答是否值得进入 PI1；不能直接证明 production dispatch。

| size | values | median | min | max | bucket |
| --- | --- | ---: | ---: | ---: | --- |
| 4K | `15.396, 17.023, 16.174, 16.297, 15.686` | 16.174 | 15.396 | 17.023 | `positive` |
| 64K | `26.059, 25.397, 26.782, 26.839, 26.631` | 26.631 | 25.397 | 26.839 | `positive` |
| 256K | `25.989, 26.460, 26.294, 26.575, 26.362` | 26.362 | 25.989 | 26.575 | `positive` |

## fused Std/RVV（同边界）

这一表只比较同一 test-support fused helper 的 Std 和 RVV 构建，用来判断 RVV accumulation 本身是否有收益。

| size | values | median | min | max | bucket |
| --- | --- | ---: | ---: | ---: | --- |
| 4K | `3.063, 3.094, 3.008, 3.109, 3.081` | 3.081 | 3.008 | 3.109 | `positive` |
| 64K | `3.087, 2.992, 3.179, 3.206, 3.186` | 3.179 | 2.992 | 3.206 | `positive` |
| 256K | `3.115, 3.166, 3.145, 3.157, 3.174` | 3.157 | 3.115 | 3.174 | `positive` |

## public build sanity（公开入口构建一致性）

这一表只检查 public Umeyama 在 Std/RVV build 中是否有明显构建漂移，不作为候选收益。

| size | values | median | min | max | bucket |
| --- | --- | ---: | ---: | ---: | --- |
| 4K | `13.888, 13.674, 14.623, 14.727, 14.114` | 14.114 | 13.674 | 14.727 | `positive` |
| 64K | `24.564, 24.819, 24.543, 24.232, 24.433` | 24.543 | 24.232 | 24.819 | `positive` |
| 256K | `23.639, 24.213, 24.047, 23.977, 23.982` | 23.982 | 23.639 | 24.213 | `positive` |

## 决策桶

- overall_decision_bucket：`positive`
- `positive` / `weak_positive` 只能支持进入 PI1；仍需 production direct tests、fallback 和 production bench。
- 任一 size 若为 `negative` 或 Evidence Doctor 有 Error，不能把该 size 纳入 production gate。
