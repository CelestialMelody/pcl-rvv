# transformation_estimation_svd correspondence-pair board repeated summary

## 运行合同

- run_label：`correspondence_pair_repeated`
- runs：`run-01, run-02, run-03, run-04, run-05`
- device：`RVV board`
- case-filter：`correspondence-pair`
- evidence_role：`diagnostic`
- iterations：`20`
- warm-up iterations：`5`
- B/A：`baseline ms / candidate ms`，大于 1 表示 candidate 更快。
- manifest（默认本地证据清单）：`log/board/correspondence_pair_repeated/evidence_manifest.json`
- Evidence Doctor：`log/board/correspondence_pair_repeated/evidence_doctor.md`

- 诊断模式说明：不是 production direct（真实生产路径证据）。

## public baseline vs fused RVV（混合边界）

这一表用当前 public Umeyama correspondence-pair Std 作为 baseline，用 test-only fused RVV 作为 candidate。它只回答该 row source 是否值得进入 production integration loop；不能直接证明 production dispatch。

| size | values | median | min | max | bucket |
| --- | --- | ---: | ---: | ---: | --- |
| 4K | `8.021, 8.712, 7.583, 8.659, 9.212` | 8.659 | 7.583 | 9.212 | `positive` |
| 64K | `7.891, 9.228, 6.851, 8.644, 8.180` | 8.180 | 6.851 | 9.228 | `positive` |
| 256K | `7.777, 7.562, 7.898, 8.053, 7.824` | 7.824 | 7.562 | 8.053 | `positive` |

## fused Std/RVV（同边界）

这一表只比较同一 test-support fused helper 的 Std 和 RVV 构建，用来判断 RVV accumulation 本身是否有收益。

| size | values | median | min | max | bucket |
| --- | --- | ---: | ---: | ---: | --- |
| 4K | `2.112, 2.322, 2.014, 2.174, 2.394` | 2.174 | 2.014 | 2.394 | `positive` |
| 64K | `1.905, 2.141, 1.521, 1.990, 1.788` | 1.905 | 1.521 | 2.141 | `positive` |
| 256K | `1.727, 1.734, 1.790, 1.772, 1.793` | 1.772 | 1.727 | 1.793 | `positive` |

## public build sanity（公开入口构建一致性）

这一表只检查 public Umeyama 在 Std/RVV build 中是否有明显构建漂移，不作为候选收益。

| size | values | median | min | max | bucket |
| --- | --- | ---: | ---: | ---: | --- |
| 4K | `0.965, 0.949, 0.938, 1.008, 1.009` | 0.965 | 0.938 | 1.009 | `negative` |
| 64K | `0.997, 0.982, 0.942, 1.012, 0.996` | 0.996 | 0.942 | 1.012 | `negative` |
| 256K | `0.993, 0.977, 0.989, 1.022, 1.010` | 0.993 | 0.977 | 1.022 | `neutral` |

## 决策桶

- overall_decision_bucket：`positive`
- `positive` / `weak_positive` 只能支持进入 PI1；仍需 production direct tests、fallback 和 production bench。
- 任一 size 若为 `negative` 或 Evidence Doctor 有 Error，不能把该 size 纳入 production gate。
