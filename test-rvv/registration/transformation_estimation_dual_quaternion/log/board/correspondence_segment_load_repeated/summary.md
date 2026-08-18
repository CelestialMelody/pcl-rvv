# transformation_estimation_dual_quaternion correspondence segment-load summary

## 运行合同

- run_label：`correspondence_segment_load_repeated`
- runs：`run-01, run-02, run-03, run-04, run-05`
- device：`Milkv-Jupiter`
- case-filter：`correspondence-segment-load-comparison`
- evidence_role：`diagnostic`；只覆盖 `correspondence-pair`，不是 production direct。
- iterations：`20`
- warm-up iterations：`5`
- B/A：Phase 008 direct index stream RVV ms / segment-load RVV ms，大于 1 表示 segment load 更快。
- 两侧共享同一 RVV binary、C1/C2 公式、Eigen 4x4 solve 和 checksum policy。
- manifest：`log/board/correspondence_segment_load_repeated/evidence_manifest.json`
- Evidence Doctor：`log/board/correspondence_segment_load_repeated/evidence_doctor.md`

| size | values | median | min | max | bucket | checksum |
| --- | --- | ---: | ---: | ---: | --- | --- |
| 256K | `1.023, 1.012, 0.989, 0.998, 1.043` | 1.012 | 0.989 | 1.043 | `unstable` | same |
| 4K | `0.872, 1.049, 0.955, 0.979, 0.981` | 0.979 | 0.872 | 1.049 | `negative` | same |
| 64K | `1.001, 0.975, 1.017, 1.011, 0.925` | 1.001 | 0.925 | 1.017 | `negative` | same |

## 决策边界

- overall_decision_bucket：`negative`（默认按 64K / 256K 判断）。
- segment-load 只替换 correspondence index ingress；正向结果不能外推到其它点型、其它 AoS 或 production。
- 64K 若出现长尾，保留 min/median/max，并按 cache、调度、AoS locality、segment-load 和 solver 稀释解释。
- 若 segment-load 无收益或不稳定，保留 Phase 008 direct index stream 作为 baseline。