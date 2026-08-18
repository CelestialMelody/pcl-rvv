# transformation_estimation_dual_quaternion correspondence index locality summary

## 运行合同

- run_label：`correspondence_index_locality_ablation_repeated`
- runs：`run-01, run-02, run-03, run-04, run-05`
- device：`Milkv-Jupiter`
- case-filter：`correspondence-index-locality-ablation`
- evidence_role：`diagnostic`；只覆盖 correspondence-pair，不是 production direct。
- iterations：`20`
- warm-up iterations：`5`
- B/A：strided Phase 008 direct index stream RVV ms / alternative pattern RVV ms；大于 1 表示 alternative pattern 更快。
- 三种 pattern 共用 direct index stream、C1/C2 公式、Eigen 4x4 solve 和输入 corpus identity；输出 fingerprint 单独保留。
- manifest：`log/board/correspondence_index_locality_ablation_repeated/evidence_manifest.json`
- Evidence Doctor：`log/board/correspondence_index_locality_ablation_repeated/evidence_doctor.md`

| size | pattern | median ms | checksum |
| --- | --- | ---: | --- |
| 4K | `contiguous` | 0.1386 | stable output fingerprint |
| 4K | `local-window` | 0.1692 | stable output fingerprint |
| 4K | `strided` | 0.1622 | stable output fingerprint |
| 4K | `strided-vs-contiguous` | median B/A `1.171x`，min `1.126x`，max `1.194x`，bucket `weak_positive` | input corpus identity same；output fingerprint intentionally separate |
| 4K | `strided-vs-local-window` | median B/A `0.959x`，min `0.927x`，max `0.985x`，bucket `negative` | input corpus identity same；output fingerprint intentionally separate |
| 64K | `contiguous` | 2.1018 | stable output fingerprint |
| 64K | `local-window` | 6.0052 | stable output fingerprint |
| 64K | `strided` | 2.6275 | stable output fingerprint |
| 64K | `strided-vs-contiguous` | median B/A `1.243x`，min `1.208x`，max `1.367x`，bucket `positive` | input corpus identity same；output fingerprint intentionally separate |
| 64K | `strided-vs-local-window` | median B/A `0.436x`，min `0.424x`，max `0.476x`，bucket `negative` | input corpus identity same；output fingerprint intentionally separate |
| 256K | `contiguous` | 8.3971 | stable output fingerprint |
| 256K | `local-window` | 24.0911 | stable output fingerprint |
| 256K | `strided` | 11.1076 | stable output fingerprint |
| 256K | `strided-vs-contiguous` | median B/A `1.320x`，min `1.287x`，max `1.365x`，bucket `positive` | input corpus identity same；output fingerprint intentionally separate |
| 256K | `strided-vs-local-window` | median B/A `0.458x`，min `0.444x`，max `0.465x`，bucket `negative` | input corpus identity same；output fingerprint intentionally separate |

## 决策边界

- 这是输入分布消融，不是 production direct 性能结论；`input_distribution` 是有意的比较差异。
- 跨 pattern 的可比 checksum 是 input corpus identity；不同 output fingerprint 由 reduction order / floating-point rounding 产生，必须结合 correctness 解释。
- QEMU 只用于 correctness、checksum 和日志形状；性能方向只看目标板卡 repeated。
- 若只有某一 pattern positive，最多保留 locality-sensitive diagnostic hypothesis，不自动修改 production。