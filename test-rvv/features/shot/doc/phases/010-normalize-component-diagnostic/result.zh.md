# Phase 010 result: normalize component diagnostic

## 执行范围

本阶段按 `plan.zh.md` 完成 `normalizeHistogram` 的 component ablation（组件消融）。实际覆盖范围：测试专用 `float*` descriptor buffer，长度 352 和 1344，有限非零输入，synthetic descriptor batch，Std / RVV 同构链路对拍和板卡 A/B。

本阶段没有修改 `features/include/pcl/features/impl/shot.hpp`，没有接入 production dispatch（生产分流），也没有把 component positive（组件正向）外推成 public entry（公开入口）生产收益。

## 动作回填

| action | 状态 | 命令 / 产物 | 结论 |
| --- | --- | --- | --- |
| B1 red test | done | `make -C test-rvv/features/shot run_test_compare` | 先在缺 helper 时得到预期编译失败：`normalizeDescriptorScalar` / `normalizeDescriptorRVV` / 数组版 L2 norm 不存在。 |
| B2 helper implementation | done | `include/impl/shot_normalize.hpp`、`include/shot.h` | 新增标量 reference（参考链路）和 RVV candidate（RVV 候选）；非 RVV 构建回退到标量 helper。 |
| B3 component bench | done | `src/bench_shot.cpp` | 新增 `normalize_352_component` 与 `normalize_1344_component` case。 |
| B4 asm | done | `make -C test-rvv/features/shot dump_bench_rvv` | 反汇编中可见 normalization 相关 `vfmacc.vv`、`vfredosum.vs`、`vle32.v`、`vse32.v` 指令组合；归因到 test-only helper / bench callsite。 |
| B5 board evidence | done | `board_smoke` + 2 次 `run_board_bench_compare fetch_board_logs` | normalization component 在 3 个 all-case board run 中稳定正向；public entry 仍不稳定 / 近 1x。 |
| B6 docs | done | 本文件、matrix、roadmap、evaluation、README | Phase 010 闭合为 `partial-production-candidate` 的输入证据，但生产接入仍需明确授权和 PI1 计划。 |

## Correctness 证据

`make -C test-rvv/features/shot run_test_compare` 通过 Std / RVV 两侧 5 个测试：

- `SHOT352.PublicEntryWithProvidedReferenceFramesProducesUnitDescriptors`
- `SHOT352.InvalidReferenceFrameMarksDescriptorAsNonDenseNaN`
- `SHOT1344.ColorPublicEntryWithProvidedReferenceFramesProducesUnitDescriptors`
- `ShotNormalizeComponent.RvvMatchesScalarReferenceForShot352`
- `ShotNormalizeComponent.RvvMatchesScalarReferenceForShot1344`

normalization 测试的 same-chain（同构链路）合同是：参考链路按 production 当前语义用 double 累加平方和，sqrt 后转成 float norm，再逐元素除以 norm；RVV 链路用 vector reduction（向量规约）与向量除法，对拍误差预算为 `2e-6`。

## 板卡证据

目标硬件为 Milkv-Jupiter，bench 参数为 side=21、441 个合成点、30 iterations、3 warmup。三次 all-case run 的方向如下：

| run label | public SHOT352 | public SHOT1344 | normalize 352 | normalize 1344 | 证据路径 |
| --- | ---: | ---: | ---: | ---: | --- |
| `phase010_all_case_smoke` | 0.99x | 1.04x | 1.59x | 1.50x | `log/board/phase010_all_case_smoke/analyze_bench_compare.log` |
| `phase010_all_case_rerun1` | 0.99x | 0.99x | 1.59x | 1.51x | `log/board/phase010_all_case_rerun1/analyze_bench_compare.log` |
| `phase010_all_case_rerun2` | 1.00x | 0.98x | 1.58x | 1.51x | `log/board/phase010_all_case_rerun2/analyze_bench_compare.log` |

解释：normalization component 的方向在有界复跑预算内稳定为 positive；public SHOT fixed-LRF 入口没有稳定收益，说明 search / interpolation / 未接 production helper 会稀释组件收益。

## Evidence Doctor 结果

current manifest / doctor 路径：

- `log/board/evidence_manifest.json`
- `log/board/evidence_doctor.md`

当前 doctor：Errors=2，Warnings=4，Suggestions=0。

两个 Error 都来自 public entry 的 `ba_degradation_frequency`，处理动作是把 public entry 证据降级为 diagnostic smoke，不能写 production performance。四个 Warning 都是 `low_run_count`，因为 manifest 当前只表达 current run（当前运行）；Phase 010 另用 run-labelled 目录保留两次额外复跑作为人工稳定性证据。由于 component case 的三次方向都远高于 1.10x，decision bucket（决策桶）为 component positive；但它仍不是 production-ready。

## Diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | component diagnostic。 |
| A/B boundary | test helper / component bench；不是 public overload，也不是 production detail helper。 |
| 当前决策问题 | RVV-vs-scalar component A/B 与 implementation-shape。 |
| diagnostic 是否可外推到 production | limited。只说明连续 descriptor normalization 值得进入有界 production probe；不能证明公开入口整体加速。 |
| comparison-boundary / baseline mismatch 风险 | yes。component helper 使用 synthetic descriptor batch，不含 `Eigen::VectorXf` 对象状态、search、LRF、shape/color bin 和 interpolation。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | not_applicable for component result；本阶段 component 是 positive。public entry 负向不能直接拒绝 probe，因为 production helper 尚未接入。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。后续若进入 production integration loop，必须补 production direct tests、fallback、asm 归因和 board evidence。 |

## 决策

`current_decision`: `partial-production-candidate` for descriptor normalization component。

`production_decision`: no production change in this phase。当前证据支持规划一个窄 production probe（生产探针），候选只覆盖 `normalizeHistogram` 的 descriptor 连续数组归一化；是否进入 production patch 需要显式授权，并且 PI1 必须先冻结 fallback、数值预算、点类型 / descriptor length 边界和 public-entry evidence plan。

`continue_stop_decision`: stop at authorization boundary for production modification; otherwise continue with non-production component diagnostics。

`stop_condition_hit`: production modification requires explicit authorization under `AGENTS.md`。若不进入 production probe，当前 topic 内仍有未阻塞 test-only 下一阶段：`020-shape-bin-component-diagnostic`。

`next_phase_default`: `PI1-normalize-production-probe-plan` if user authorizes production integration; fallback default is `020-shape-bin-component-diagnostic` within test-rvv only.
