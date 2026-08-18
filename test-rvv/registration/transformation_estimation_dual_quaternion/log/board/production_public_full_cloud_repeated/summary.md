# transformation_estimation_dual_quaternion board repeated summary

## 运行合同

- run_label：`production_public_full_cloud_repeated`
- runs：`run-01, run-02, run-03, run-04, run-05`
- device：`Milkv-Jupiter`
- case-filter：`public-dual-quaternion`
- evidence_role：`production_direct`；只覆盖 ordered full-cloud public entry，不覆盖 indices / correspondences；若 production patch 后续被撤回，该证据只作为撤回原因记录。
- iterations：`20`
- warm-up iterations：`5`
- B/A：`Std public entry ms / RVV public entry ms`，大于 1 表示 RVV public entry 更快。
- manifest（默认本地证据清单）：`log/board/production_public_full_cloud_repeated/evidence_manifest.json`
- Evidence Doctor：`log/board/production_public_full_cloud_repeated/evidence_doctor.md`

## production public Std/RVV（同边界）

这一表比较同一 production ordered full-cloud public entry 的 Std 与 RVV 构建。若 RVV 构建包含临时 production dispatch，则用于判断该 dispatch 是否满足生产证据门槛；否则它只是 public entry 的同入口构建对比。

| size | values | median | min | max | bucket | checksum |
| --- | --- | ---: | ---: | ---: | --- | --- |
| 4K | `1.002, 1.001, 0.992, 1.008, 0.988` | 1.001 | 0.988 | 1.008 | `neutral` | same |
| 64K | `0.993, 1.000, 1.003, 1.002, 1.013` | 1.002 | 0.993 | 1.013 | `neutral` | same |
| 256K | `0.997, 1.001, 1.003, 1.001, 1.006` | 1.001 | 0.997 | 1.006 | `neutral` | same |

## 决策桶

- overall_decision_bucket：`neutral`（默认按 64K / 256K 等非 4K 目标规模判断；4K 保留为小规模诊断）。
- `positive` / `weak_positive` 只能按当前 mode 的证据边界解释；indexed / correspondences 不从 ordered full-cloud 自动继承。
- 任一 size 若为 `negative` 或 Evidence Doctor 有 Error，不能把该 size 纳入 production gate。
