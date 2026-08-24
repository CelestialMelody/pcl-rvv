# Phase 002 Plan: weighted-spfh-33 production probe

## 阶段意图和边界

本阶段进入 production integration loop（生产接入闭环）的有界探针：只验证
`features/include/pcl/features/impl/fpfh.hpp::weightPointSPFHSignature` 中默认
11+11+11 bin 的 FPFH（Fast Point Feature Histogram，快速点特征直方图）加权合成是否值得保留。

本阶段不证明泛型点类型全覆盖、不处理 `computePointSPFHSignature` 的 pair-feature batch（点对特征批处理），
不接 `fpfh_omp.hpp`，也不把 Phase 001 的 test-only candidate 直接写成 adopted production behavior
（已采用生产行为）。PI5 结束后无论正负，都先保留当前 production diff，报告证据并等待用户确认保留或回滚。

## 当前状态清单

| area | 当前状态 | 路径 / 证据 |
| --- | --- | --- |
| production diff | 已恢复到一份有界生产探针：`__RVV10__` 下新增 `pcl::detail::weightFPFHSignature33RVV`，入口短路尝试后 fallback 到原标量主体。 | `features/include/pcl/features/impl/fpfh.hpp` |
| correctness | 生产补丁后 QEMU Std/RVV 已跑过，5/5 pass。 | `make -C test-rvv/features/fpfh run_test_compare`; `log/qemu/run_test_std.log`, `log/qemu/run_test_rvv.log` |
| asm | 强制 rebuild 后发现 sampled vector instructions 仍只归到 test-only candidate header，production helper 归因尚未闭合。 | `make -B -C test-rvv/features/fpfh dump_bench_rvv`; `build/asm/riscv/bench_fpfh_rvv.full.asm` |
| board | production patch 后尚未完成 board smoke / repeated / Evidence Doctor。 | 待跑 |
| doc-rvv | 尚未创建长期主题文档；只有 PI5 production evidence 支持且用户确认采纳后才适用。 | 待 PI5 决策 |

## 假设与候选族

| candidate family | 假设 | 风险 / 未知 | 本阶段判据 |
| --- | --- | --- | --- |
| `weighted-spfh-33-production-rvv` | Eigen column-major SPFH matrix 的每行 11 个 bin 可以用 `vlse32`（跨步加载）和 FMA（融合乘加）减少组件耗时。 | production row indices 可能来自 `spfh_hist_lookup` remap，不一定 dense sequential；public path 可能被 search 稀释。 | production direct correctness pass、asm 能归到 `fpfh.hpp`、board repeated 中 `component_weighted_spfh_33` 正向且 public case 至少不出现明显退化。 |
| `weighted-spfh-33-test-only-candidate` | Phase 001 证明 dense-row 形态有组件空间。 | 它不是 production path；不能作为 PI5 采纳证据。 | 仅作为对照，不能单独关闭 production decision。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `weighted-spfh-33-production-rvv` | arbitrary valid SPFH row indices after production lookup | `PointNormal -> FPFHSignature33`, `float`, 33-bin contiguous output | production `weightPointSPFHSignature` helper | `run_test_compare`; direct weighted helper; public finite / NaN semantics | `component_weighted_spfh_33`, `public_fpfh_k` | PI2/PI4 board smoke and 5-run repeated | must map vector ops to `features/include/pcl/features/impl/fpfh.hpp` | repeated manifest + Doctor | pending PI5 |
| `weighted-spfh-33-test-only-candidate` | dense sequential rows | same as Phase 001 diagnostic | `candidate_weighted_spfh_dense_rows` | already pass | keep as historical comparison only | optional repeated output if included by aggregate target | maps to topic-local candidate header | diagnostic only | not adoption evidence |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| A1 asm attribution investigation | `nm -C`, `objdump`, `addr2line` over all RVV instructions; optionally add a production-focused bench case/filter if attribution is ambiguous. | 至少一组 `vlse32` / `vfmacc` / `vfmul` 或等价 RVV 指令映射到 `features/include/pcl/features/impl/fpfh.hpp`，或记录为 Doctor Warning 并降级。 |
| A2 QEMU correctness freshness | `make -C test-rvv/features/fpfh run_test_compare` | Std/RVV 5/5 pass；若新增 regression，仍必须 pass。 |
| A3 board smoke | `make -C test-rvv/features/fpfh board_smoke` | 板端 RVV test pass，bench log 形状可被 analyzer 解析。 |
| A4 board repeated | `make -C test-rvv/features/fpfh board_repeated REPEATED_BOARD_OUTPUT_DIR=log/board/pi1-production-weighted/repeated` | 5-run summary 存在，记录 `component_weighted_spfh_33`、`public_fpfh_k` 和 candidate 对照。 |
| A5 Evidence Doctor | `make -C test-rvv/features/fpfh evidence_doctor_repeated ...pi1-production-weighted...` | Errors / Warnings / Suggestions 全部解释；Error 未解决时不能 clean-adopt。 |
| A6 文档刷新 | phase result、optimization matrix、roadmap、evaluation；若 PI5 支持且用户确认采纳，再创建 `doc-rvv/features/fpfh-RVV.zh.md`。 | 所有当前 truth 使用 production patch 后的板卡数据，不复用 Phase 001 诊断数据作为采纳依据。 |

## Evidence Doctor 和 registry 规则

本阶段使用 `test-rvv/features/fpfh/script/generate_fpfh_evidence_manifest.py` 生成 repeated manifest，再调用
`test-rvv/script/evidence_doctor.py`。当前 topic 没有已维护的 `log/evidence_registry.json`；本阶段 Handoff
必须写 `evidence_registry_status=not_available`，并列出人工检查过的 log 路径。

Doctor Error 先修复或降级；Warning 必须解释，例如 asm attribution 不闭合、public path 退化、metadata
缺失或 binary identity 缺失。QEMU timing（仿真器计时）不作为性能证据。

## 板卡复跑预算和决策桶

默认复跑预算为 5 runs，不因小幅波动无限复跑。

| bucket | 判定口径 |
| --- | --- |
| `strong_positive` | `component_weighted_spfh_33` repeated 平均明显快于 Std，0/5 degradation，public case 无明显退化。 |
| `weak_positive` | 组件平均约 1.05x-1.20x，退化频率低，production diff 足够小且 Doctor 无阻塞 Error。 |
| `neutral` | 平均约 0.98x-1.05x 或方向不稳，不支持采纳但可保留给用户判断。 |
| `negative` | 组件或 public case 稳定退化。 |
| `unstable` | 5-run 方向摇摆、degradation frequency 高或 Doctor 指出 A/B 边界无法比较。 |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | 本阶段目标是 `production-detail` + `production-public`。Phase 001 candidate 只作为 historical diagnostic。 |
| A/B boundary | Std build 原标量 production helper vs RVV build 中同一 production helper 的 RVV dispatch。 |
| 当前决策问题 | `RVV-vs-scalar`：当前 production patch 是否值得保留。 |
| diagnostic 是否可外推到 production | Phase 001 只能说明值得尝试；采纳只看本阶段 production boundary 内证据。 |
| comparison-boundary / baseline mismatch 风险 | 若 aggregate target 同时包含 test-only candidate，必须在结果中分开 case；candidate speedup 不能抵消 production path 退化。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已进入有界探针；若本阶段 production evidence 弱/负/不稳，停在 PI5 给用户决定，不自行回滚。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前没有已有 adopted RVV family，因此 `RVV-vs-scalar` 正向可支持“建议保留”，但最终 adopted 仍需用户确认。 |

## 阶段完成条件

本阶段完成必须同时给出 correctness、asm、board repeated、Evidence Doctor 和生产边界解释。若 asm attribution
无法闭合或 Doctor 有 Error，则 decision 降级为 `blocked` / `unstable` / `explicit probe only`。

## 继续 / 停止条件

`continue_stop_decision` 默认继续到 PI5 证据包。合法停止条件只有：板卡不可达、工具链失败、Doctor Error 无法解释、
production diff 需要扩大到未授权入口，或 PI5 已完成并需要用户确认采纳 / 回滚。

`next_phase_default` 在 PI5 前是继续执行本计划；PI5 后若用户确认采纳，进入 production closeout 和正式
`doc-rvv` 文档；若用户要求回滚，则进入 rollback / no-production closeout；若性能中性但仍有授权方向，
下一候选优先审计 `spfh-pair-feature-batch`。

## 文档更新清单

- 更新 `doc/phases/README.zh.md` 和 `doc/phases/optimization-matrix.zh.md`。
- 完成后写本目录 `result.zh.md`。
- 刷新 `doc/optimization-roadmap.zh.md` 和 `doc/fpfh-evaluation.zh.md`。
- 只有 PI5 production evidence 支持且用户确认采纳后，创建 `doc-rvv/features/fpfh-RVV.zh.md`。
