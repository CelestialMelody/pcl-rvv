# Phase 040: RGB extrema quantize production RVV result

## 实际执行范围

本阶段按计划验证 `ColorModality<PointXYZRGB>::processInputData()` 的 quantize+filter production RVV family。覆盖范围是 organized `PointXYZRGB`、`width >= 3 && height >= 3`、RGB extrema quantize、3x3 dominant filter 和后续 spread 调用；不覆盖 `extractFeatures()`、`computeDistanceMap()`、其它 RGB-like 模板点型和非 organized 输入。

## 计划动作回填

| action | status | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| 扩展生产 helper | done | `recognition/include/pcl/recognition/color_modality.h` | `processInputData()` 在 `__RVV10__` 下尝试 `quantizeColorsRVV()` 和 `filterQuantizedColorsRVV()`，失败回到 `processInputDataStd()`。 |
| 扩展测试支撑 | done | `test-rvv/recognition/color_modality/src/test_cm.cpp` | forced scalar/RVV 对拍覆盖 `quantized` 和 `spreaded` map，RVV path hook 命中。 |
| 扩展 bench | done | `test-rvv/recognition/color_modality/src/bench_cm.cpp` | `production_process_320x240` 和 `production_process_641x481_tail` 直接计时公开入口。 |
| QEMU correctness | done | `make -C test-rvv/recognition/color_modality run_test_compare` | Std/RVV 两侧 5/5 通过，QEMU 只作为正确性和路径证据。 |
| QEMU bench smoke | done | `make -C test-rvv/recognition/color_modality run_bench_rvv BENCH_ARGS="--case-filter production_process_320x240 --iterations 1 --warmup-iterations 1"` | RVV bench 可运行，不作为性能结论。 |
| asm gate | done | `make -C test-rvv/recognition/color_modality check_cm_rvv_asm` | 反汇编匹配 RVV byte load/store、compare 和 merge 指令。 |
| board repeated | done | `SSH_AUTH_SOCK=/run/user/1001/keyring/ssh make -C test-rvv/recognition/color_modality board_repeated record_evidence_state_repeated` | 5-run 板卡 repeated positive，registry 已登记。 |

## 板卡结果

证据路径：`test-rvv/recognition/color_modality/log/board/repeated_phase040_quantize_filter_rvv/summary.md`，run label 为 `cm_phase040_quantize_filter_rvv_repeated`。

| case | runs | median speedup | min | max | B/A < 1 | checksum |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `production_process_320x240` | 5 | `2.490x` | `2.430x` | `2.510x` | `0/5` | Std/RVV 一致：`4030162183548831619` |
| `production_process_641x481_tail` | 5 | `2.410x` | `2.380x` | `2.430x` | `0/5` | Std/RVV 一致：`5008288661140242712` |

本阶段正式证据覆盖 public overload（公开入口）边界，计时包含 RGB quantize、3x3 filter 和 spread，不包含 feature extraction。恢复扫描时曾产生一次误命名的 `repeated_phase030_process_rvv` 本地 registry 记录；该记录已从当前 registry 移除，不作为 current truth（当前事实）。

## Evidence Doctor

`test-rvv/recognition/color_modality/log/board/repeated_phase040_quantize_filter_rvv/evidence_doctor.md` 结果为 `Errors=0, Warnings=0, Suggestions=4`。Suggestions 是缺少 `taskset`、`governor`、`freq`、`temperature` 和 binary hash（等价二进制身份）字段；当前 5-run 无方向反转、无 checksum mismatch（校验和不一致）且 `B/A < 1` 为 0，因此这些建议不阻塞采纳。板卡输出仍有 clock skew warning（时钟偏移警告），不改变本阶段结论。

## Diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | production direct |
| A/B boundary | public overload |
| 当前决策问题 | RVV-family-selection：quantize+filter family 是否优于只接 filter 的历史生产 family |
| diagnostic 是否可外推到 production | not_applicable，本阶段直接通过 public entry 计时 |
| comparison-boundary / baseline mismatch 风险 | public Std/RVV positive 证明当前 RVV 快于当前 scalar；与 Phase 030 的 family selection 通过恢复 handoff 里的历史基线和当前同 case board 数值做有界比较，不写成完整 RVV-vs-RVV detail A/B |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已完成 bounded probe；结果 positive，不触发回滚 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 用户本轮授权“板卡有收益即可采纳”，且当前收益从 Phase 030 约 `1.38x/1.36x` 提升到 Phase 040 `2.490x/2.410x`；后续若要做严格 family paper trail，可补 RVV-vs-RVV detail A/B，但不阻塞本轮采纳 |

## Doc-suite parity 审计

| area | current shape scan | quality bar | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | 已有 topic README | 提供入口、阅读路径、命令和证据白名单 | adopted | `README.zh.md` | none |
| testing_overview | 独立文档 | target 分类和证据边界可复核 | adopted | `doc/testing-overview.zh.md` | none |
| correctness_tests | 独立文档 | gtest 输入、断言和范围明确 | adopted | `doc/correctness-tests.zh.md` | none |
| benchmark_and_evidence | 独立文档 | case-filter、board repeated、doctor、registry 明确 | adopted | `doc/benchmark-and-evidence.zh.md` | none |
| optimization_evidence | 独立文档 | adopted/deferred candidate 到证据映射 | adopted | `doc/optimization-evidence.zh.md` | none |
| optimization_roadmap | 独立文档 | 记录候选前沿和恢复条件 | adopted | `doc/optimization-roadmap.zh.md` | none |
| test_support_code_map | 独立文档 | 聚合头、internal helper、src、script 和 production 对照 | adopted | `doc/test-support-code-map.zh.md` | none |
| evaluation | 独立文档 | production closeout 决策审计 | adopted | `doc/color_modality-evaluation.zh.md` | none |
| production_topic_doc | 独立长期文档 | 只写 adopted production 行为和证据链 | adopted | `doc-rvv/recognition/color_modality-RVV.zh.md` | none |
| artifact tracking | 当前新增文档在 topic 路径扫描中可见 | 未跟踪文件需列入提交边界 | adopted | `git status --short --untracked-files=all -- ...` 待最终扫描 | none |

## 继续 / 停止决定

`continue_stop_decision`: stop-for-review with stop_condition_hit。停止条件是当前 roadmap 和 matrix 中没有仍在授权范围内、无需 profile 或更大生产范围即可继续推进的高价值动作。`extractFeatures()` 和 `computeDistanceMap()` 需要先有 profile 或 component ablation 证明热点和收益可能性；泛型 RGB traits 扩展需要新的点型需求和独立证据矩阵。当前默认下一动作是 reviewer 审查当前 production patch、test-rvv 资产、doc suite 和 evidence registry。
