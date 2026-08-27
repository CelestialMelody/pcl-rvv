# Phase 060 result: production-shaped shape combined diagnostic

## 当前结论

本阶段完成 production-shaped shape combined diagnostic（生产形态 shape 组合诊断）。结果为
`attempted / production-shaped-diagnostic-negative`：在 test-only helper boundary（测试 helper 边界）里，
RVV（RISC-V Vector，可变长度向量）projection staging（三线性前的投影暂存）、
RVV trilinear staging（三线性暂存）、scalar Eigen histogram write（标量 Eigen 直方图写回）和
RVV shape copy（形状描述子拷贝）组合后，稳定慢于 scalar direct shape combined baseline（标量直接组合基线）。

本阶段没有修改 `features/include/pcl/features/impl/gasd.hpp`。当前结论只说明这个
production-shaped diagnostic boundary 不支持该 staged shape family（分阶段 shape 候选族）。它不能直接推出
`no-production` / `rejected`，也不能拒绝用户授权后的 bounded production probe（有界生产探针）。

当前 repeated board（重复板卡测试）summary：

| case | run label | runs | median speedup | range | B/A < 1 | checksum |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| `candidate_shape_combined_rvv` | `gasd_phase060_shape_combined_repeated` | 5 | 0.590x | 0.590x-0.600x | 5/5 | Std/RVV 一致 |

Evidence Doctor（证据体检）结果为 `Errors=1，Warnings=0，Suggestions=2`。Error 是
`ba_degradation_frequency`：5/5 run 的 B/A 都低于 1。处理动作是把本阶段性能证据写成稳定负向诊断，
并禁止把它作为 production positive（生产正向）或 production-ready（可接入生产）证据。
两个 suggestion 仍是环境 metadata（环境元数据）和 binary identity（二进制身份）缺失；由于 5/5 方向一致，
它们不改变本阶段 decision bucket（决策桶）。

## 计划执行回填

| action | status | evidence / command | result |
| --- | --- | --- | --- |
| C1 scalar combined reference | done | `include/impl/gasd_reference.hpp` | `computeShapeDescriptorTrilinearStd` 使用 direct projection + direct Eigen write + scalar shape copy，baseline 不共享 staging |
| C2 RVV combined candidate | done | `include/impl/gasd_copy_candidate.hpp` | `computeShapeDescriptorTrilinearRVVStaged` 使用 RVV projection staging + RVV trilinear staging + scalar Eigen write + RVV copy |
| C3 correctness test | done | `make -C test-rvv/features/gasd run_test_compare` | QEMU Std/RVV 两侧 12/12 gtest 通过 |
| C4 bench case | done | `src/bench_gasd.cpp` | case `candidate_shape_combined_rvv` 输出 timing / checksum；checksum policy 改为 tolerance-aligned 1e3 scale |
| C5 manifest metadata | done | `script/generate_gasd_evidence_manifest.py` | metadata 写清 `production-shaped diagnostic`、`test helper` 和 combined timer boundary |
| C6 asm | done | `make -C test-rvv/features/gasd dump_bench_rvv` | `build/asm/riscv/bench_gasd_rvv.asm` 重新生成，供 manifest 归属当前 RVV bench |
| C7 board repeated + Doctor | done | `make board_repeated ... candidate_shape_combined_rvv ...`、`make evidence_doctor_repeated ...` | median 0.590x，Doctor Error=1 用作负向降级信号 |
| C8 文档回填 | done | 本 result、phase README、matrix、roadmap、evaluation、README | 当前结论刷新为 Phase 060 production-shaped diagnostic-negative |

补充 correctness（正确性）证据：`make -C test-rvv/features/gasd run_board_test` 在板卡上通过 RVV gtest
12/12。这个 target 验证当前测试代码能在板端运行并通过 same-chain（同构链路）断言；性能结论仍只来自
repeated board summary。

## 诊断证据链

- correctness：QEMU `run_test_compare` 通过 Std/RVV 两侧 12/12 gtest；板卡 `run_board_test` 通过 12/12 gtest。
- QEMU（仿真器）证据：极小 `candidate_shape_combined_rvv` bench smoke 只用于 log-shape（日志形状）和 checksum 检查，两侧 checksum 为 `9.56023e+18`；QEMU timing 不进入性能结论。
- asm（反汇编）边界：`build/asm/riscv/bench_gasd_rvv.asm` 由当前 RVV bench 二进制生成；RVV 指令属于 test-only staging / copy helper 边界，Eigen histogram write 仍是 scalar boundary。
- board performance（板卡性能）：5-run repeated board 当前桶为 `negative`，median 0.590x，range 0.590x-0.600x，5/5 反向。
- Evidence Doctor：`ba_degradation_frequency` Error 已通过降级结论处理；本阶段不声明 production positive。

本阶段不能证明 public `computeFeature` dispatch（公开入口分流）、alignment transform（对齐变换）、
`indices_` row source（索引行来源）、descriptor object state（描述子对象状态）、`INTERP_NONE`、
`INTERP_QUADRILINEAR`、color path（颜色路径）、其它 `PointT` 或 `Scalar=double`。负向诊断只约束当前
production-shaped test helper boundary。

## 诊断到生产错配审计回填

| question | result |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper：scalar direct shape combined helper vs RVV staged shape combined helper |
| 当前决策问题 | RVV-vs-scalar diagnostic；判断 shape path 的 projection 正向是否能抵消 Eigen write 退化 |
| diagnostic 是否可外推到 production | no；仍缺 public dispatch、alignment transform、indices、descriptor object state 和真实 `computeFeature` output path |
| comparison-boundary / baseline mismatch 风险 | yes；synthetic cloud 已是 transformed-like 输入，不覆盖 transform、indices、object setup 和 public wrapper |
| weak / negative / neutral 时是否允许 bounded production probe | yes，但需要用户显式授权 production integration loop；当前默认先做 no-production closeout / profile 恢复条件审计 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；当前没有 production boundary 内的 RVV family-selection 证据 |

## Matrix / roadmap 更新

`production-shaped shape combined diagnostic` 从 `phase_deferred + unblocked` 更新为
`attempted / production-shaped-diagnostic-negative`。Phase 010 / 030 的局部正向在 Phase 050 / 060 的
Eigen write 和组合边界下被写回成本吞掉；当前 staged shape family 不应进入 production patch。

下一阶段默认进入 `070-no-production-closeout-profile-audit`。该阶段只在 topic-local 文档内完成：
汇总 no-production closeout（不接入生产收尾）判断、profile（性能剖析）恢复条件、bounded production probe
允许条件和 `doc-rvv` not_applicable（不适用）边界。它不修改 production。

## Continue / stop decision

`continue_stop_decision`：continue。

`stop_condition_hit`：none。production 文件未修改，板卡可用，Evidence Doctor Error 已解释并降级为负向诊断。
当前仍有授权且未阻塞的 topic-local closeout / profile 恢复条件审计动作。

`next_phase_default`：创建并推进 `070-no-production-closeout-profile-audit/plan.zh.md`。
