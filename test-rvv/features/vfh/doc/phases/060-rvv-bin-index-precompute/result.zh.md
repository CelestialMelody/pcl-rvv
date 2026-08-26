# Phase 060 RVV Bin Index Precompute Result

## 当前状态

Phase 060 已在 Phase 050 的 chunk-local staging（分块局部暂存）基础上继续优化：
`vfhRVVAngularBins()` 和 `vfhRVVViewpointBins()` 用 RVV 预计算每个 lane（向量通道）的 histogram bin
（直方图箱号），再把 `int32` bin 写入 O(VLmax) 小缓冲。最终 histogram increment（直方图加一类更新）
仍按 lane 顺序标量执行，避免同 bin 写冲突和浮点累加顺序变化。

这是当前保留的 production（生产源码）实现形态。用户已明确说明板卡生产结果显示收益即可采纳，因此
Phase 060 的 production-public board evidence（公开生产入口板卡证据）足以进入 adopted production behavior
（已采用生产行为）文档 closeout。

## 计划动作回填

| action | 状态 | 证据 / 结果 |
| --- | --- | --- |
| IMPL | done | `features/include/pcl/features/impl/vfh.hpp` 新增 RVV bin helper；SPFH 和 viewpoint helper 使用 `int32` bin buffer。 |
| CORRECTNESS | done | `make -C test-rvv/features/vfh run_test_compare` 通过；Std/RVV 各 9 个测试，新增覆盖 over-range normal 语义和 `normalize_bins=false` fallback。 |
| ASM | done | `make -B -C test-rvv/features/vfh dump_bench_rvv` 通过；production helper 区域可见 `vfmin.vf`、`vfmax.vf`、`vfcvt.rtz.x.f.v`，`computeFeature()` 调用 production helper。 |
| BOARD | done | `make -C test-rvv/features/vfh board_repeated REPEATED_BOARD_OUTPUT_DIR=log/board/repeated-production-phase060-postreview BENCH_ARGS='--side 80 --iterations 8 --warmup 2'` 完成 5-run。 |
| DOCTOR | done | `make -C test-rvv/features/vfh evidence_doctor_repeated REPEATED_BOARD_OUTPUT_DIR=log/board/repeated-production-phase060-postreview` 输出 `0E/0W/11S`。 |
| DOC | done | 以 Phase 060 post-review production board 数据刷新 README、evaluation、matrix、roadmap、topic-local doc suite、正式 `doc-rvv/features/vfh-RVV.zh.md` 和保留候选复筛表。 |

## 数值边界

Phase 060 在 RVV 中先把 scaled bin 值 clamp 到 `[0, bins - 1]`，再使用
`vfcvt.rtz.x.f.v`。由于 clamp 后输入非负，round-towards-zero（向零取整）与标量 `floor` 等价；超过范围的值已经在转换前钳到最终边界。直方图写入仍保持原标量 lane 顺序，因此不引入同 bin 并行 scatter 或浮点归约重排。

## 板卡结果

`production_vfh_compute_default` 的 checksum 在 5 次运行中均一致：Std/RVV 均为 `448016`。

| run | Std ms | RVV ms | speedup |
| --- | ---: | ---: | ---: |
| run_01 | 8.86645 | 5.36115 | 1.65383x |
| run_02 | 8.82497 | 5.38891 | 1.63762x |
| run_03 | 8.79738 | 5.37904 | 1.63549x |
| run_04 | 8.76886 | 5.38770 | 1.62757x |
| run_05 | 8.81418 | 5.37161 | 1.64088x |
| mean | 8.81437 | 5.37768 | 1.63906x |

Phase 060 明显优于 Phase 050 mean `1.44115x`，也优于 Phase 040 mean `1.36195x`。decision bucket
为 `positive`，5-run 范围为 `1.62757x-1.65383x`，未出现方向摇摆。

## Evidence Doctor

`log/board/repeated-production-phase060-postreview/evidence_doctor.md` 结果为 `Errors=0，Warnings=0，Suggestions=11`。
Suggestions 仅要求后续采集补充环境 metadata、binary identity，以及解释历史 `component_vfh_reference`
near-threshold baseline；production case 本身没有 Error 或 Warning。当前结论保留这些 metadata 建议为审查风险，
不降级 adopted production behavior。

## Doc-suite parity audit

| area | current shape scan | quality bar | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | 已刷新 `README.zh.md`，列出当前采用状态、阅读路径、命令和证据边界。 | topic_navigation role。 | adopted | 当前文件存在并引用 Phase 060。 | none |
| testing-overview | 新增 `doc/testing-overview.zh.md`。 | 测试类型、target 粒度和证据边界独立可读。 | adopted | 文档列出 Makefile / board target。 | none |
| correctness-tests | 新增 `doc/correctness-tests.zh.md`。 | 每个 gtest family 说明输入、断言和不能证明范围。 | adopted | 文档映射 `src/test_vfh.cpp`。 | none |
| benchmark-and-evidence | 新增 `doc/benchmark-and-evidence.zh.md`。 | bench label、checksum、board repeated、Doctor 和提交边界独立可读。 | adopted | 文档引用 Phase 060 evidence paths。 | none |
| optimization-evidence | 新增 `doc/optimization-evidence.zh.md`。 | adopted / attempted / rejected / deferred 路线有主归属。 | adopted | 文档说明 Phase 040-060 取舍。 | none |
| test-support-code-map | 新增 `doc/test-support-code-map.zh.md`。 | 聚合头、内部 helper、src、script、production helper 可互相定位。 | adopted | 文档覆盖当前 topic 真实文件形态。 | none |
| evaluation | 已刷新 `doc/vfh-evaluation.zh.md`。 | evaluation_production role。 | adopted | 记录 EvidenceDecision、fallback matrix、Traceability Map。 | none |
| production topic doc | 新增 `doc-rvv/features/vfh-RVV.zh.md`。 | adopted production behavior 的长期事实主归属。 | adopted | 使用 Phase 060 production data。 | none |
| phase suite | Phase 040/050/060 result、README、matrix 已刷新。 | phase index / matrix role。 | adopted | 本文件与 matrix 记录继续 / 停止判断。 | none |
| artifact tracking | 路径限定 status 扫描用于最终 Handoff。 | 新增文档需列入 topic artifact boundary。 | adopted | 由最终 `git status --short --untracked-files=all -- ...` 复核。 | none |

## 继续 / 停止决策

当前同一 production boundary 内没有更值得立即推进的优化方向。histogram scatter（直方图离散累加）是剩余热点候选，但直接 RVV scatter 会遇到同 bin 冲突和浮点累加顺序风险；做私有 bin、分块合并或 atomic-like（类似原子更新）方案会引入新的数值和维护边界，当前没有 profile 证据要求继续冒这个风险。

因此本阶段 `continue_stop_decision` 为：Phase 060 adopted；当前 topic 暂停在 production closeout。后续若继续，应另开窄 phase 或 follow-up topic，分别覆盖 point type expansion（点类型扩展）、`size_component`、`normalize_distances`、非 dense / subset indices、CVFH / OUR-CVFH 调用路径，或 histogram scatter 专项消融。
