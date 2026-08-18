# Phase 003 Result：component-ablation

## 结论

本阶段完成 C1/C2 accumulation-only component（只测双四元数 C1/C2 累加前端的组件消融）和 solve-only（只测 Eigen 求解后段）拆分。Milkv-Jupiter 5-run repeated board（重复板卡测试）显示 accumulation-only 的 Std/RVV 严格同边界 B/A 在 4K / 64K / 256K 上分别为 `2.076x / 2.102x / 2.096x`，overall decision bucket 为 `positive`；Evidence Doctor（证据体检）为 Errors=0，Warnings=0，Suggestions=0。

这个结果说明 Phase 001 的 test-only helper 正向收益来自 C1/C2 前端本身，不是偶然噪声；solve-only 在 64K / 256K 为 neutral，不能解释 Phase 002 production-public neutral。当前生产源码仍保持标量，Phase 003 不重新接入 production dispatch（生产分流）。

本阶段额外把 production boundary gate（生产边界门控）排查固化为 gtest：`PointXYZ`、`PointXYZI` 和 `PointXYZRGB` 均满足 `pcl::rvv::RVVXYZAoSFloatLayout`，offset 为 `0/4/8`。因此 Phase 002 的 neutral 不应再优先归因于代表点型 traits gate 静默失败；后续更值得排查的是临时 production patch 形态、分流是否真实命中、计时边界和 public wrapper（公开入口包装层）与 test-only helper 的基线差异。

EvidenceDecision：`no_production_after_component_positive_public_neutral`。component evidence（组件证据）只保留为诊断和后续生产边界探针输入，不能替代 production direct（真实生产路径）证据。

## 执行动作

| action | 状态 | 产物 / 证据 | 结论 |
| --- | --- | --- | --- |
| A1 helper checksum | done | `include/impl/tedq_candidates.hpp` | accumulation checksum 改为量化 bucket，避免 RVV reduction tree（向量规约树）正常舍入差异误报 checksum mismatch；矩阵 correctness 仍由 gtest 保护。 |
| A2 bench cases | done | `src/bench_tedq.cpp` | `component-ablation` filter 输出 public、helper full-estimate、accum-only 和 solve-only 四类 case。 |
| A3 summary script | done | `script/generate_tedq_board_repeated_summary.py` | `--component-ablation` 生成 strict component A/B manifest，同时在 summary 中保留 helper / public / solve / mixed-boundary context。 |
| A4 Make targets / allowlist | done | `Makefile`、`test-rvv/.gitignore` | `make run_board_bench_component_ablation_repeated` 可采集、生成 summary / manifest / doctor 并记录 registry；summary-only evidence 已放行。 |
| A5 board component evidence | done | `log/board/component_ablation_repeated/*` | accumulation-only strict A/B 为 positive；doctor clean。 |
| A6 production boundary gate probe | done | `src/test_tedq.cpp`、`make run_test_compare` | Std/RVV 各 10 tests passed；代表点型满足 `RVVXYZAoSFloatLayout`，Phase 002 neutral 不优先归因于 layout gate。 |
| A7 docs / matrix | done | 本 result、phase README、roadmap、optimization matrix、evaluation 和 topic-local docs | 当前状态可由短 prompt 恢复；`doc-rvv` 仍为 not_applicable。 |

## Board Component-Ablation 结果

证据路径：

- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/component_ablation_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/component_ablation_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/component_ablation_repeated/evidence_doctor.md`

严格同边界表只覆盖 `component accum only ordered-cloud-pair`。B/A 为 `Std component ms / RVV component ms`，大于 1 表示 RVV component 更快。

| size | B/A values | median | bucket | checksum |
| --- | --- | ---: | --- | --- |
| 4K | `2.158, 2.076, 2.064, 2.034, 2.097` | 2.076 | `positive` | same |
| 64K | `2.087, 2.109, 2.105, 2.102, 2.100` | 2.102 | `positive` | same |
| 256K | `2.149, 2.094, 2.096, 2.109, 2.087` | 2.096 | `positive` | same |

Evidence Doctor result：Errors=0，Warnings=0，Suggestions=0。

## Context 表解释

`summary.md` 还保留三类 context（上下文）表，但它们不改变 strict component A/B 的证据角色。

| context | 关键结果 | 解释 |
| --- | --- | --- |
| helper full-estimate | 4K / 64K / 256K median B/A 为 `1.979x / 2.093x / 2.115x` | test-support helper 的 C1/C2 + Eigen solve 仍稳定正向，说明 solve 没有吞掉前端收益。 |
| public entry context | 4K / 64K / 256K median B/A 为 `0.999x / 1.003x / 1.003x` | 当前源码没有 retained production dispatch，因此这只是 Std/RVV 构建下 public scalar entry 的同入口对照，不是 production speedup。 |
| solve-only | 64K / 256K median 均约 `1.000x`；4K 因极小耗时为 `unstable` | 后段 Eigen solve + 矩阵构造不是 RVV 受益点，也不是 Phase 002 neutral 的主要解释。 |
| public/helper mixed-boundary cross-check | Std public/helper median 为 `1.768x / 1.819x / 1.821x`；RVV public/helper median 为 `3.508x / 3.773x / 3.781x` | public entry 明显慢于 test-support helper；这是 wrapper / baseline 差异线索，不能当严格 A/B。 |

## Gate Probe 结果

本阶段先用临时编译探针确认 `PointXYZ`、`PointXYZI`、`PointXYZRGB` 的 `RVVXYZAoSFloatLayout` 均为 true，随后把同一结论转为 `PublicGateAllowsRepresentativeXYZLayouts` gtest。当前 QEMU correctness：

- `test-rvv/registration/transformation_estimation_dual_quaternion/log/qemu/run_test_std.log`：10 tests passed。
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/qemu/run_test_rvv.log`：10 tests passed。

代表点型 gate probe 的证据边界很窄：它只证明这些点型不会被公共 xyz AoS traits gate 阻止，不证明 production dispatch 已经存在、已命中或有性能收益。

## 诊断证据链

Correctness（正确性）：Std/RVV 双构建各 10 个 gtest 通过，覆盖 public/reference/candidate 对拍、小规模 fallback、`PointXYZI` layout、`Scalar=double` 保守路径、source-indexed / dual-indexed / correspondence identity public boundary，以及本阶段新增的 layout gate probe。

QEMU path evidence（QEMU 路径证据）：QEMU 只用于 correctness 和日志形状；本阶段没有把 QEMU timing 写成性能证据。

Board performance（板卡性能）：Phase 003 strict component A/B 来自 Milkv-Jupiter 5-run repeated summary，accumulation-only 为 positive，doctor clean；helper full-estimate context 同样 positive；public entry context 仍 neutral。

Production boundary（生产边界）：当前 production 源码没有 diff。Phase 003 只能说明 C1/C2 前端仍值得作为候选保留，不能推翻 Phase 002 production-public neutral 结论，也不能创建 `doc-rvv/registration/transformation_estimation_dual_quaternion-RVV.zh.md`。

## Optimization Matrix 更新

| candidate family | row source policy | point type / Scalar / layout | board / doctor | decision | next action |
| --- | --- | --- | --- | --- | --- |
| C1/C2 accumulation-only ablation | ordered-cloud-pair | `PointXYZ` / `float` / x,y,z AoS | 5-run positive；doctor clean | `diagnostic_component_positive` | 保留为后续 production-boundary probe 输入。 |
| helper full-estimate | ordered-cloud-pair | `PointXYZ` / `float` / x,y,z AoS | context positive | `diagnostic_positive_only` | 不直接外推到 public entry。 |
| solve-only | precomputed accumulation | Eigen 4x4 solve + matrix | 64K/256K neutral，4K unstable | `not_rvv_target` | 保持标量。 |
| public entry context | ordered-cloud-pair public entry | 当前 no-production 源码 | neutral | `public_scalar_context_only` | 若重开 production，必须补真实 path-hit / asm / board direct。 |
| representative xyz AoS gate | ordered-cloud-pair production candidate scope | `PointXYZ` / `PointXYZI` / `PointXYZRGB` | QEMU gtest pass | `gate_allows_representative_layouts` | 后续不要把 Phase 002 neutral 首要归因写成 layout gate blocked。 |

## 继续 / 停止判断

本阶段已闭合 component-ablation 计划动作和 Evidence Doctor。仍有未阻塞的下一阶段动作：`004-production-boundary-probe`。建议只做 test-rvv 范围内的生产边界探针和静态接法审计，不直接重加 production patch；若要重新进入 production integration loop，需要先冻结新的 PI1 范围，并补真实 public path-hit、fallback、production asm attribution 和 repeated board evidence。

`next_phase_default`：`004-production-boundary-probe`，目标是解释 Phase 002 临时 production patch 为什么没有把 helper/component positive 转成 public entry speedup。默认动作包括：复核临时接法的 gate / path-hit 证据需求、比较 public wrapper 与 direct helper 的计时边界、设计 production-direct path-hit gtest 或可提交 asm summary 的最小方案。

## 提交边界

| 路径 | 状态 | 说明 |
| --- | --- | --- |
| `registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp` | unchanged | production 仍保持标量。 |
| `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/component_ablation_repeated/summary.md` | to-be-staged | component summary evidence。 |
| `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/component_ablation_repeated/evidence_manifest.json` | to-be-staged | Evidence Doctor manifest。 |
| `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/component_ablation_repeated/evidence_doctor.md` | to-be-staged | Errors=0，Warnings=0，Suggestions=0。 |
| `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/component_ablation_repeated/run-*/*.log` | ignored-local | raw board run logs，不默认提交。 |
| `test-rvv/registration/transformation_estimation_dual_quaternion/log/qemu/run_test_*.log` | ignored-local | QEMU correctness raw logs，可再生成。 |
