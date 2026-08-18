# transformation_estimation_dual_quaternion board repeated summary

## 运行合同

- run_label：`component_ablation_repeated`
- runs：`run-01, run-02, run-03, run-04, run-05`
- device：`Milkv-Jupiter`
- case-filter：`component-ablation`
- evidence_role：`component_ablation`；严格表只覆盖 ordered full-cloud C1/C2 accumulation-only component，不覆盖 production public entry。
- iterations：`20`
- warm-up iterations：`5`
- B/A：`Std component ms / RVV component ms`，大于 1 表示 RVV component 更快。
- manifest（默认本地证据清单）：`log/board/component_ablation_repeated/evidence_manifest.json`
- Evidence Doctor：`log/board/component_ablation_repeated/evidence_doctor.md`

## component accum-only Std/RVV（严格同边界）

这一表只比较 C1/C2 accumulation-only component（只测累加前端组件）的 Std 与 RVV 构建，不包含 Eigen 4x4 solve 或矩阵构造。

| size | values | median | min | max | bucket | checksum |
| --- | --- | ---: | ---: | ---: | --- | --- |
| 4K | `2.158, 2.076, 2.064, 2.034, 2.097` | 2.076 | 2.034 | 2.158 | `positive` | same |
| 64K | `2.087, 2.109, 2.105, 2.102, 2.100` | 2.102 | 2.087 | 2.109 | `positive` | same |
| 256K | `2.149, 2.094, 2.096, 2.109, 2.087` | 2.096 | 2.087 | 2.149 | `positive` | same |

## 决策桶

- overall_decision_bucket：`positive`（默认按 64K / 256K 等非 4K 目标规模判断；4K 保留为小规模诊断）。
- `positive` / `weak_positive` 只能按当前 mode 的证据边界解释；indexed / correspondences 不从 ordered full-cloud 自动继承。
- component strict A/B 的 `positive` / `weak_positive` 只能说明 C1/C2 前端仍有局部收益，不能替代 production direct。


## helper full-estimate context（同边界）

这一表复查 test-support helper full-estimate（C1/C2 累加 + Eigen solve）的 Std/RVV 构建差异，作为 Phase 001 的同 run context。

B/A：`Std helper full-estimate ms / RVV helper full-estimate ms`，大于 1 表示 RVV helper 更快。

| size | values | median | min | max | bucket | checksum |
| --- | --- | ---: | ---: | ---: | --- | --- |
| 4K | `2.014, 2.024, 1.964, 1.763, 1.979` | 1.979 | 1.763 | 2.024 | `positive` | same |
| 64K | `2.130, 2.093, 2.075, 2.107, 2.069` | 2.093 | 2.069 | 2.130 | `positive` | same |
| 256K | `2.133, 2.115, 2.072, 2.124, 2.078` | 2.115 | 2.072 | 2.133 | `positive` | same |

## public entry context（同入口构建对比）

这一表只比较当前源码状态下 public ordered full-cloud entry 的 Std/RVV 构建。当前没有 retained production dispatch，因此它不是 production speedup 证据。

B/A：`Std public entry ms / RVV public entry ms`，大于 1 表示 RVV 构建下 public entry 更快。

| size | values | median | min | max | bucket | checksum |
| --- | --- | ---: | ---: | ---: | --- | --- |
| 4K | `0.999, 0.990, 1.008, 0.996, 1.005` | 0.999 | 0.990 | 1.008 | `neutral` | same |
| 64K | `1.000, 1.009, 1.000, 1.003, 1.009` | 1.003 | 1.000 | 1.009 | `neutral` | same |
| 256K | `1.007, 1.010, 1.000, 1.002, 1.003` | 1.003 | 1.000 | 1.010 | `neutral` | same |

## solve-only context（预计算累加后段）

这一表只测用预计算 C1/C2 accumulation 输入 Eigen solve 和矩阵构造的后段成本；预期不含 RVV 前端。

B/A：`Std solve-only ms / RVV solve-only ms`，接近 1 表示后段不是 RVV 受益点。

| size | values | median | min | max | bucket | checksum |
| --- | --- | ---: | ---: | ---: | --- | --- |
| 4K | `1.648, 1.000, 1.000, 1.019, 1.618` | 1.019 | 1.000 | 1.648 | `unstable` | same |
| 64K | `0.980, 0.980, 1.000, 1.000, 1.000` | 1.000 | 0.980 | 1.000 | `neutral` | same |
| 256K | `1.000, 1.000, 1.022, 1.000, 1.000` | 1.000 | 1.000 | 1.022 | `neutral` | same |

## public/helper mixed-boundary cross-check

这一表是 mixed-boundary cross-check（混合边界交叉检查）：`public / helper` 大于 1 表示 public entry 比 test-support helper 更慢。它只帮助解释 wrapper / baseline 差异，不能作为严格 A/B。

| size | Std public/helper median | Std min | Std max | RVV public/helper median | RVV min | RVV max |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 4K | 1.768 | 1.726 | 1.800 | 3.508 | 3.124 | 3.589 |
| 64K | 1.819 | 1.811 | 1.834 | 3.773 | 3.760 | 3.857 |
| 256K | 1.821 | 1.799 | 1.825 | 3.781 | 3.766 | 3.861 |
