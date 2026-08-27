# Phase 050 result: Eigen-backed histogram write probe

## 当前结论

本阶段完成 Eigen-backed histogram write probe（使用 `std::vector<Eigen::VectorXf>` 的直方图写回探针）。
结果为 `attempted / diagnostic-negative`：在更接近 production（生产源码）的 `Eigen::VectorXf` histogram
layout（直方图布局）下，RVV（RISC-V Vector，可变长度向量）trilinear staging（三线性暂存）加 scalar
Eigen write（标量 Eigen 写回）稳定慢于 scalar direct compute+Eigen write baseline（标量直接计算并写回基线）。

当前 repeated board（重复板卡测试）summary：

| case | run label | runs | median speedup | range | B/A < 1 | checksum |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| `candidate_trilinear_eigen_histogram_write_rvv` | `gasd_phase050_eigen_histogram_write_probe_repeated` | 5 | 0.820x | 0.800x-0.820x | 5/5 | Std/RVV 一致 |

Evidence Doctor（证据体检）结果为 `Errors=1，Warnings=0，Suggestions=2`。Error 是
`ba_degradation_frequency`：5/5 run 的 B/A 都低于 1。处理动作不是修成正向，而是把本阶段性能证据
明确写成 `diagnostic-negative`，禁止把该候选作为 production evidence（生产证据）或生产接入理由。
两个 suggestion 仍是环境 metadata 和 binary identity 缺失；由于方向强负且 5/5 一致，它们不改变本阶段桶。

## 计划执行回填

| action | status | evidence / command | result |
| --- | --- | --- | --- |
| E1 scalar Eigen reference | done | `include/impl/gasd_reference.hpp` | `accumulateTrilinearHistogramEigenStd` 直接计算 bin / 权重并写 `HistogramGrid` |
| E2 RVV staged Eigen candidate | done | `include/impl/gasd_copy_candidate.hpp` | `accumulateTrilinearHistogramEigenRVVStaged` 使用 RVV staging + scalar Eigen write |
| E3 correctness test | done | `src/test_gasd.cpp` | `GASDEigenHistogramWrite.AccumulatesTrilinearEigenHistogramLikeScalarReference` 纳入 compare |
| E4 bench case | done | `src/bench_gasd.cpp` | case `candidate_trilinear_eigen_histogram_write_rvv` 输出 timing / checksum |
| E5 manifest metadata | done | `script/generate_gasd_evidence_manifest.py` | metadata 写清 Eigen-backed timer boundary 和 diagnostic role |
| E6 asm | done | `make -C test-rvv/features/gasd dump_bench_rvv` | `accumulateTrilinearHistogramEigenRVVStaged` 调用 `computeTrilinearInterpolationRVVToBuffers`，写回仍是 scalar boundary |
| E7 board repeated + Doctor | done | `make board_repeated ... candidate_trilinear_eigen_histogram_write_rvv ...`、`make evidence_doctor_repeated ...` | median 0.820x，Doctor Error=1 用作负向降级信号 |
| E8 文档回填 | done | 本 result、phase README、matrix、roadmap、evaluation、README | 当前结论刷新为 Phase 050 diagnostic-negative |

## 诊断证据链

- correctness（正确性）：`make -C test-rvv/features/gasd run_test_compare` 通过 Std/RVV 两侧 10/10 gtest。
- QEMU（仿真器）证据：新 case 能在 QEMU compare 中运行且 checksum 一致；QEMU timing 不进入性能结论。
- asm（反汇编）边界：`build/asm/riscv/bench_gasd_rvv.full.asm` 显示
  `accumulateTrilinearHistogramEigenRVVStaged` 调用 `computeTrilinearInterpolationRVVToBuffers`；RVV 指令存在于
  staging helper，Eigen histogram `+=` 写回仍为 scalar boundary。
- board performance（板卡性能）：5-run repeated board 当前桶为 `negative`，median 0.820x，且 5/5 反向。
- Evidence Doctor：`ba_degradation_frequency` Error 已通过降级结论处理；本阶段不声明 production positive。

本阶段不能证明 public `computeFeature`、descriptor output copy（描述子输出拷贝）、`INTERP_NONE`、
`INTERP_QUADRILINEAR`、color path（颜色路径）、其它 `PointT` 或 `Scalar=double`。负向诊断不能直接推出
`no-production` / `rejected`；它只说明 Eigen-backed test helper boundary 当前不支持该 staged-write 候选。

## 诊断到生产错配审计回填

| question | result |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | test helper：scalar direct trilinear compute+Eigen write vs RVV staging+scalar Eigen write |
| 当前决策问题 | RVV-vs-scalar diagnostic；判断 production-like histogram layout 下是否仍值得继续 |
| diagnostic 是否可外推到 production | no；仍缺 public dispatch、descriptor copy、object state 和完整 compute |
| comparison-boundary / baseline mismatch 风险 | yes；bench wrapper 预先构造 projection staging，不覆盖 transform、normalization、public wrapper 和 output copy |
| weak / negative / neutral 时是否允许 bounded production probe | yes，但需改成更高层 production-shaped combined smoke 或 profile；不能直接用本阶段拒绝所有 production probe |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes |

## Matrix / roadmap 更新

`Eigen-backed histogram write probe` 从 `phase_deferred + unblocked` 更新为
`attempted / diagnostic-negative`。Phase 040 flat layout 的 weak-positive 在 Eigen-backed layout 下反转为
negative，说明 staging buffer 的额外写回和 Eigen per-cell write boundary（每个 cell 的 Eigen 写回边界）
对该候选不友好。

下一阶段默认不进入 production patch。仍可在 test-only 范围内做
`060-production-shaped-shape-combined-diagnostic`：把 shape projection staging、trilinear Eigen write 和
copy boundary 放进同一个 production-shaped helper（生产形态 helper）里，检查上游 projection 收益能否抵消
Eigen write 退化；如果组合路径仍 negative，再把 production 接入候选降级到 profile / no-production closeout
讨论。

## Continue / stop decision

`continue_stop_decision`：continue。

`stop_condition_hit`：none。production 文件未修改，板卡可用，Evidence Doctor Error 已解释并降级为负向诊断，
当前仍有授权且未阻塞的 test-only 下一动作。

`next_phase_default`：创建并推进 `060-production-shaped-shape-combined-diagnostic/plan.zh.md`。
