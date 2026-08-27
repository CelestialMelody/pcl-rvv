# Phase 080 public extract component profile 结果

## 阶段结论

本阶段完成 `public_extract_profile` 诊断剖析。该 case 使用测试专用 wrapper（包装层）拆分真实
`setBackgroundPointsIndices()` + `extract()` 形态的主要组件，不修改 production（生产源码）。

Milkv-Jupiter 96x72、`iterations=3`、`warmup=1`、5-run repeated board（重复板卡性能测试）结果为
positive：B/A 为 `1.169912, 1.181390, 1.167619, 1.166456, 1.160458`，median `1.167619x`。
Std median 为 `1671.833667 ms`，RVV median 为 `1433.259123 ms`，两侧 checksum 均为
`9089139176994405943`。Evidence Doctor（证据体检）结果为 `Errors=0, Warnings=0, Suggestions=0`。

本阶段的决策桶是 `profile_actionable`。`learn_gmms` 在 Std profile 中约占 `11.46%`，并且 Std/RVV 几乎无差异；
它不是最大热点，但它是当前剩余组件中唯一同时满足“非微小占比”和“可复用已验证 GMM 公式族”的方向。
因此下一阶段应先做 `learnGMMs()` component assignment（分量归属选择）诊断，而不是直接修改 production。

## 实际执行范围

| 项 | 状态 | 证据 |
| --- | --- | --- |
| profile bench case | done | `src/bench_grabcut.cpp` 的 `--case public_extract_profile` 输出 `BENCH grabcut_component` 和 `BENCH grabcut_profile_component`。 |
| manifest metadata | done | `script/generate_grabcut_board_evidence_manifest.py` 识别 `public_extract_profile`。 |
| Make targets | done | `collect_public_extract_profile_repeated_board`、`run_public_extract_profile_repeated_evidence_doctor` 和 registry target 已接入。 |
| QEMU smoke | done | Std/RVV `public_extract_profile` checksum 一致；QEMU timing 不进入性能结论。 |
| board repeated | done | `doc/phases/080-public-extract-component-profile/repeated-board-20260827-clean-96x72`。 |
| Evidence Doctor / registry | done | `repeated-evidence-doctor.md` 为 `0/0/0`，`log/evidence_registry.json` 已登记。 |

`doc/phases/080-public-extract-component-profile/repeated-board-20260827-211135` 是 SSH / rsync 失败时产生的历史目录。
该目录不作为当前证据。当前 canonical（规范使用）目录是
`doc/phases/080-public-extract-component-profile/repeated-board-20260827-clean-96x72`。

## 板卡剖析摘要

| component | Std median ms | Std median pct | RVV median ms | RVV median pct | Std/RVV | delta ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `base_init_and_validate` | 0.278097 | 0.016618% | 0.274777 | 0.019215% | 1.012083 | 0.003320 |
| `image_color_staging` | 0.125555 | 0.007503% | 0.124375 | 0.008696% | 1.009487 | 0.001180 |
| `state_vector_resize` | 0.090167 | 0.005386% | 0.086847 | 0.006069% | 1.038228 | 0.003320 |
| `compute_beta_organized` | 4.302542 | 0.257929% | 4.267514 | 0.297746% | 1.008208 | 0.035028 |
| `compute_nlinks_organized` | 1.485250 | 0.088140% | 1.470542 | 0.103097% | 1.010002 | 0.014708 |
| `seed_background_indices` | 0.024347 | 0.001456% | 0.023500 | 0.001648% | 1.036043 | 0.000847 |
| `build_gmms` | 2.673667 | 0.160025% | 2.686028 | 0.187405% | 0.995398 | -0.012361 |
| `initgraph_fit` | 19.827278 | 1.184769% | 18.761736 | 1.309026% | 1.056793 | 1.065542 |
| `learn_gmms` | 191.261680 | 11.462780% | 191.279708 | 13.410219% | 0.999906 | -0.018028 |
| `initgraph_refine` | 1189.285031 | 71.276517% | 989.046863 | 69.107552% | 1.202456 | 200.238168 |
| `graph_solve` | 257.464485 | 15.426855% | 221.292599 | 15.412822% | 1.163457 | 36.171886 |
| `update_hard_segmentation` | 1.556193 | 0.093291% | 1.709056 | 0.119242% | 0.910557 | -0.152863 |
| `output_clusters` | 0.214444 | 0.012751% | 0.221639 | 0.015539% | 0.967537 | -0.007195 |

`initgraph_refine` 的收益来自已采纳 terminal weight RVV helper。`graph_solve` 的差异是同一 public-shaped profile
中的观察值，但 solver 本身没有 RVV candidate，不能把该差异解释成 solver 被 RVV 化。`image_color_staging`、
`compute_beta_organized` 和 `compute_nlinks_organized` 的占比低，不支持立即进入 production integration loop
（生产接入闭环）。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic-profile（诊断剖析，只用于定位下一候选） |
| A/B boundary | test-only subclass wrapper（测试专用子类包装层），形态模拟 public `setBackgroundPointsIndices()` + `extract()` |
| 当前决策问题 | implementation-shape：是否存在值得规划的新 RVV family（实现族） |
| 计时边界 | 拆分 `initCompute`、GMM build / learn、`initGraph`、graph solve、hard segmentation update 和 output cluster；包含 public-shaped wrapper 成本 |
| row source / point type / layout | organized image grid、`PointXYZRGB`、float `Color`、96x72 |
| diagnostic 是否可外推到 production | no。它只能决定下一阶段候选排序，不能直接证明新 helper 可接入 production。 |
| comparison-boundary / baseline mismatch 风险 | yes。profile 切分了 protected 调用，不能替代真实 public wall-time 或同一 production boundary 内的 RVV-vs-RVV A/B。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no。只有 profile 指向明确组件且后续同边界诊断为 positive，才考虑有界 production probe。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。新增 production family 若要采纳，必须补 production-shaped 或 production direct 证据。 |

## Evidence Doctor 和 registry

- manifest: `doc/phases/080-public-extract-component-profile/repeated-evidence-manifest.json`
- Doctor: `doc/phases/080-public-extract-component-profile/repeated-evidence-doctor.md`
- result: `Errors=0, Warnings=0, Suggestions=0`
- registry: `log/evidence_registry.json` 记录 `public_extract_profile` summary files。

本阶段没有使用 QEMU timing（QEMU 计时）做性能排序。性能结论只来自 Milkv-Jupiter repeated board。

## 继续 / 停止决策

`continue_stop_decision=continue`。当前没有理由直接扩大 production patch。可继续方向是 Phase 090：
在测试资产中单独验证 `learnGMMs()` component assignment 子阶段，因为它复用 Phase 010/020 已验证的 GMM
概率公式，并且 Phase 080 profile 显示 `learn_gmms` 是当前可优化组件中唯一具有可见占比的公式型路径。
