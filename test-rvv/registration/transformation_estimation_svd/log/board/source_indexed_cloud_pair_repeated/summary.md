# transformation_estimation_svd source-indexed-cloud-pair board repeated summary

## 运行合同

- run_label：`source_indexed_cloud_pair_repeated`
- runs：`run-01, run-02, run-03, run-04, run-05`
- device：`RVV board`
- case-filter：`source-indexed-cloud-pair`
- evidence_role：`diagnostic`
- iterations：`20`
- warm-up iterations：`5`
- B/A：`baseline ms / candidate ms`，大于 1 表示 candidate 更快。
- manifest（默认本地证据清单）：`log/board/source_indexed_cloud_pair_repeated/evidence_manifest.json`
- Evidence Doctor：`log/board/source_indexed_cloud_pair_repeated/evidence_doctor.md`

- 诊断模式说明：不是 production direct（真实生产路径证据）。

## public baseline vs fused RVV（混合边界）

这一表用当前 public Umeyama source-indexed-cloud-pair Std 作为 baseline，用 test-only fused RVV 作为 candidate。它只回答该 row source 是否值得进入 production integration loop；不能直接证明 production dispatch。

| size | values | median | min | max | bucket |
| --- | --- | ---: | ---: | ---: | --- |
| 4K | `7.012, 7.017, 6.986, 6.911, 7.360` | 7.012 | 6.911 | 7.360 | `positive` |
| 64K | `9.129, 9.023, 9.031, 9.005, 9.121` | 9.031 | 9.005 | 9.129 | `positive` |
| 256K | `8.843, 8.827, 8.838, 8.599, 8.740` | 8.827 | 8.599 | 8.843 | `positive` |

## fused Std/RVV（同边界）

这一表只比较同一 test-support fused helper 的 Std 和 RVV 构建，用来判断 RVV accumulation 本身是否有收益。

| size | values | median | min | max | bucket |
| --- | --- | ---: | ---: | ---: | --- |
| 4K | `2.088, 1.903, 1.917, 1.908, 1.967` | 1.917 | 1.903 | 2.088 | `positive` |
| 64K | `1.874, 1.820, 1.835, 1.874, 1.843` | 1.843 | 1.820 | 1.874 | `positive` |
| 256K | `1.783, 1.785, 1.787, 1.793, 1.778` | 1.785 | 1.778 | 1.793 | `positive` |

## public build sanity（公开入口构建一致性）

这一表只检查 public Umeyama 在 Std/RVV build 中是否有明显构建漂移，不作为候选收益。

| size | values | median | min | max | bucket |
| --- | --- | ---: | ---: | ---: | --- |
| 4K | `0.981, 1.005, 0.988, 0.973, 1.024` | 0.988 | 0.973 | 1.024 | `neutral` |
| 64K | `1.005, 1.014, 1.008, 1.002, 1.008` | 1.008 | 1.002 | 1.014 | `neutral` |
| 256K | `1.015, 1.014, 1.014, 1.003, 1.009` | 1.014 | 1.003 | 1.015 | `neutral` |

## 决策桶

- overall_decision_bucket：`positive`
- `positive` / `weak_positive` 只能支持进入 PI1；仍需 production direct tests、fallback 和 production bench。
- 任一 size 若为 `negative` 或 Evidence Doctor 有 Error，不能把该 size 纳入 production gate。
