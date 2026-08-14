# transformation_estimation_svd dual-indices-cloud-pair board repeated summary

## 运行合同

- run_label：`dual_indices_cloud_pair_repeated`
- runs：`run-01, run-02, run-03, run-04, run-05`
- device：`RVV board`
- case-filter：`dual-indices-cloud-pair`
- evidence_role：`diagnostic`
- iterations：`20`
- warm-up iterations：`5`
- B/A：`baseline ms / candidate ms`，大于 1 表示 candidate 更快。
- manifest（默认本地证据清单）：`log/board/dual_indices_cloud_pair_repeated/evidence_manifest.json`
- Evidence Doctor：`log/board/dual_indices_cloud_pair_repeated/evidence_doctor.md`

- 诊断模式说明：不是 production direct（真实生产路径证据）。

## public baseline vs fused RVV（混合边界）

这一表用当前 public Umeyama dual-indices-cloud-pair Std 作为 baseline，用 test-only fused RVV 作为 candidate。它只回答该 row source 是否值得进入 production integration loop；不能直接证明 production dispatch。

| size | values | median | min | max | bucket |
| --- | --- | ---: | ---: | ---: | --- |
| 4K | `4.677, 4.666, 4.729, 4.751, 4.731` | 4.729 | 4.666 | 4.751 | `positive` |
| 64K | `4.451, 4.321, 4.283, 4.420, 4.124` | 4.321 | 4.124 | 4.451 | `positive` |
| 256K | `3.932, 4.201, 3.934, 4.277, 3.750` | 3.934 | 3.750 | 4.277 | `positive` |

## fused Std/RVV（同边界）

这一表只比较同一 test-support fused helper 的 Std 和 RVV 构建，用来判断 RVV accumulation 本身是否有收益。

| size | values | median | min | max | bucket |
| --- | --- | ---: | ---: | ---: | --- |
| 4K | `1.787, 1.832, 1.801, 1.817, 1.778` | 1.801 | 1.778 | 1.832 | `positive` |
| 64K | `1.787, 1.684, 1.683, 1.681, 1.609` | 1.683 | 1.609 | 1.787 | `positive` |
| 256K | `1.425, 1.504, 1.418, 1.526, 1.337` | 1.425 | 1.337 | 1.526 | `positive` |

## public build sanity（公开入口构建一致性）

这一表只检查 public Umeyama 在 Std/RVV build 中是否有明显构建漂移，不作为候选收益。

| size | values | median | min | max | bucket |
| --- | --- | ---: | ---: | ---: | --- |
| 4K | `1.015, 0.968, 1.027, 0.992, 1.045` | 1.015 | 0.968 | 1.045 | `negative` |
| 64K | `1.001, 0.990, 1.006, 1.034, 1.052` | 1.006 | 0.990 | 1.052 | `unstable` |
| 256K | `1.005, 1.000, 1.009, 1.022, 0.984` | 1.005 | 0.984 | 1.022 | `neutral` |

## 决策桶

- overall_decision_bucket：`positive`
- `positive` / `weak_positive` 只能支持进入 PI1；仍需 production direct tests、fallback 和 production bench。
- 任一 size 若为 `negative` 或 Evidence Doctor 有 Error，不能把该 size 纳入 production gate。
