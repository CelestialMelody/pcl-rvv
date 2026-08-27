# Phase 040 result: histogram write probe

## 当前结论

本阶段完成了 histogram write probe（直方图写回探针）的 test-only diagnostic（测试专用诊断）。
修正后的当前证据显示：RVV（RISC-V Vector，可变长度向量）负责三线性插值的
`grid_idx/h_idx/w000..w111` staging（分阶段暂存），随后仍用标量代码写入 flat histogram（扁平直方图）。
在加入 8 次 `+=` 写回后，Phase 030 的算术 / 索引收益大部分被写回边界吞掉，但仍保留稳定弱正向信号。

当前 repeated board（重复板卡测试）summary：

| case | run label | runs | median speedup | range | B/A < 1 | checksum |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| `candidate_trilinear_histogram_write_rvv` | `gasd_phase040_histogram_write_probe_repeated` | 5 | 1.060x | 1.050x-1.060x | 0/5 | Std/RVV 一致 |

Evidence Doctor（证据体检）结果为 `Errors=0，Warnings=0，Suggestions=2`。两个 suggestion 是
环境 metadata（taskset、governor、freq、temperature）和 binary identity（二进制身份）缺失；它们不阻塞
本阶段 weak-positive diagnostic（弱正向诊断）结论，但不能把该结果升级为 production evidence（生产证据）。

早期 Phase 040 run 曾因 baseline（基线）也使用 staging helper 而得到更高 speedup。该 run 已降级为
historical evidence（历史证据）：当前 truth（当前事实）只使用修正后的 scalar direct compute+write
baseline，即本文件和 `log/board/repeated_phase040_histogram_write_probe/summary.md` 记录的 1.060x median。

## 计划执行回填

| action | status | evidence / command | result |
| --- | --- | --- | --- |
| D1 test-first 红灯 | done | `src/test_gasd.cpp` 新增 `GASDHistogramWrite.AccumulatesTrilinearFlatHistogramLikeScalarReference` | helper 补齐后纳入 compare target |
| D2 scalar reference | done | `include/impl/gasd_reference.hpp` | `accumulateTrilinearHistogramStd` 改为直接计算 bin / 权重并写 flat histogram，避免 baseline 共享 staging |
| D3 RVV staged candidate | done | `include/impl/gasd_copy_candidate.hpp` | `accumulateTrilinearHistogramRVVStaged` 使用 RVV staging + scalar flat write |
| D4 bench case | done | `src/bench_gasd.cpp`、`script/generate_gasd_evidence_manifest.py` | case-filter 为 `candidate_trilinear_histogram_write_rvv` |
| D5 asm | done | `make -C test-rvv/features/gasd dump_bench_rvv` | RVV bench binary 可生成反汇编；本阶段只要求 staging helper 归属，不声称写回向量化 |
| D6 board repeated + Doctor | done | `make board_repeated ... candidate_trilinear_histogram_write_rvv ...`、`make evidence_doctor_repeated ...` | median 1.060x，Doctor Errors=0 / Warnings=0 / Suggestions=2 |
| D7 文档回填 | done | 本 result、phase README、matrix、roadmap、evaluation、README | 当前结论刷新为 Phase 040 weak-positive diagnostic |

## 诊断证据链

- correctness（正确性）：`make -C test-rvv/features/gasd run_test_compare` 通过 Std/RVV 两侧 9/9 gtest。
- QEMU（仿真器）证据：小规模 smoke 只验证日志形状和 checksum 一致，不作为性能证据。
- asm（反汇编）边界：`dump_bench_rvv` 能生成 RVV bench 反汇编；RVV 指令归属到 staging helper，histogram `+=` 写回仍是标量边界。
- board performance（板卡性能）：5-run repeated board 当前桶为 `weak_positive`，median 1.060x，且 0/5 反向。
- Evidence Doctor：无 Error / Warning；Suggestions 只影响复现解释强度，不改变本阶段桶。

本阶段不能证明 production public dispatch（生产公开入口分流）、`Eigen::VectorXf` per-cell layout
（每个 cell 一个 Eigen 向量的真实布局）、descriptor output copy（描述子输出拷贝）、`INTERP_NONE`、
`INTERP_QUADRILINEAR`、color path（颜色路径）、其它 `PointT` 或 `Scalar=double`。

## 诊断到生产错配审计回填

| question | result |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | test helper：scalar direct trilinear compute+flat write vs RVV staging+scalar flat write |
| 当前决策问题 | RVV-vs-scalar diagnostic；判断写回加入后是否仍值得推进更接近 production 的候选 |
| diagnostic 是否可外推到 production | no；flat histogram 不覆盖 `std::vector<Eigen::VectorXf>`、public wrapper、descriptor copy 和对象状态 |
| comparison-boundary / baseline mismatch 风险 | yes；flat layout 比 production Eigen layout 更连续，可能低估或高估真实写回成本 |
| weak / negative / neutral 时是否允许 bounded production probe | yes；本阶段 weak-positive 不能直接拒绝 production-shaped probe，也不能直接 clean-adopt |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；若未来已有 adopted family，新 family 还需同边界 family comparison（实现族比较） |

## Matrix / roadmap 更新

`histogram write probe` 从 `phase_deferred + unblocked` 更新为
`attempted / diagnostic-weak-positive`。Phase 030 的大幅正向和 Phase 040 的弱正向共同说明：
三线性算术 / 索引 staging 本身有收益，但写回是主导风险。下一阶段默认推进
`050-eigen-backed-histogram-write-probe`，把同一 staged candidate 放到 `std::vector<Eigen::VectorXf>`
布局上，审计 flat-vs-Eigen mismatch（扁平布局和 Eigen 布局不一致）。

## Continue / stop decision

`continue_stop_decision`：continue。

`stop_condition_hit`：none。production 文件未修改，板卡可用，Evidence Doctor 没有 Error，
当前仍有授权且未阻塞的下一动作。

`next_phase_default`：创建并推进 `050-eigen-backed-histogram-write-probe/plan.zh.md`。
