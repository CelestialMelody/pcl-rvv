# correspondence_types 优化矩阵

本矩阵跟踪 `registration/correspondence_types` topic（主题）的 candidate family（候选实现族）、row source policy（行来源策略）、测试、bench（性能测试）、反汇编和 Evidence Doctor（证据体检）状态。QEMU（仿真器）只作为 correctness（正确性）、路径和日志形状证据；性能结论以 board / target hardware（板卡或目标硬件） repeated benchmark 为准。

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| diagnostic-strided-index-extract | correspondences（对应关系数组顺序扫描） | `pcl::Correspondence` 12-byte AoS（结构数组），`pcl::index_t` 32-bit | `getQueryIndices` / `getMatchIndices` test-only candidate（仅测试使用候选） | `make run_test_compare` 通过；板卡 `run_board_test` 6/6 通过；覆盖顺序、重复、负值和 `UNAVAILABLE=-1` sentinel（哨兵值）；空输入不命中 RVV | QEMU bench smoke checksum 一致；QEMU timing 不用于性能；board repeated 使用 `--case-filter index-extract --iterations 20 --warmup-iterations 5` | Phase 010 5-run summary：64K match median 0.959、256K query+match median 0.960、4K query median 0.987，全部 `negative` | `dump_bench_rvv` 生成 bench 二进制反汇编，存在 `vlse32.v` / `vse32.v`；归属到 bench/candidate 二进制，不是 production 符号 | QEMU doctor Errors=0 / Warnings=0 / Suggestions=0；board doctor Errors=3 / Warnings=1 / Suggestions=0，退化频率作为生产降级信号处理 | `historical_negative_input` | Phase 011 已补同 production boundary 复核 |
| production-strided-index-extract-probe | production public helpers | `pcl::Correspondence` 12-byte AoS，`pcl::index_t` 32-bit | 临时接入真实 `getQueryIndices` / `getMatchIndices`，完成 production direct probe 后回退 | 新增 production direct gtest；QEMU Std/RVV 8/8 pass；板卡 8/8 pass；非 RVV 构建自然为标量 | `production-index-extract` case-filter；5-run board repeated 使用 `--iterations 20 --warmup-iterations 5` | Phase 011 5-run summary：64K match median 0.983、256K query+match median 0.966、4K query median 0.877，全部 `negative` | 临时 patch 下 `dump_bench_rvv` 可见 `vlse32.v` / `vse32.v`，归属到 production case lambda 的内联边界；无独立 production 符号 | production probe doctor Errors=3 / Warnings=1 / Suggestions=0 | `attempted_and_rolled_back_after_negative` | none；当前 production 保持标量 |
| distance-stats-reduction | correspondences 顺序扫描 | `distance` float 字段，输出 double mean / stddev | `getCorDistMeanStd` test-only candidate | `make run_test_compare` 通过；覆盖一般样本、高动态范围样本、空输入和 `n==1` 既有 NaN 边界 | QEMU smoke checksum 一致；只证明日志形状和 same-chain 输出 | not_run after index-negative；需要单独目标硬件 A/B 和数值预算复核，但不会改变本 topic 主结论 | 反汇编中存在 `vfmul.vv`；也出现 `vfwredosum.vs`，但 candidate 源码仍把 chunk 存回后按标量顺序 double 累加，`vfwredosum.vs` 只记录为编译器对诊断二进制的自动向量化信号 | QEMU doctor clean；board 未运行该 family | `not_run_after_index_negative_no_production` | optional profiling / ablation only；默认不继续 |
| production-dispatch | production public helpers | 同上 | 真实 `correspondence_types.hpp` inline helper | attempted in Phase 011；最终源码回退到标量 | attempted with `production-index-extract` | production direct board probe negative | temporary inline attribution partial pass | doctor Errors=3 / Warnings=1 / Suggestions=0 | `rejected_no_production_after_probe` | none |
| compiler-auto-vectorization-report | 当前标量循环 | 三个 helper | missed-vectorization（未自动向量化）辅助诊断 | not_run | not_run | not_applicable | 不替代 objdump | not_run | `not_required_after_board_negative` | none |

## Evidence Freshness

当前矩阵使用的本地证据来自本 topic 重新运行或生成的：

- `make run_test_compare`
- `make dump_bench_rvv`
- `make run_evidence_doctor_qemu`
- `make check_board_ssh`
- `make run_board_test fetch_board_logs`
- 5 次 `make run_board_bench_compare fetch_board_logs BENCH_ARGS="--case-filter index-extract --iterations 20 --warmup-iterations 5"`
- `make run_evidence_doctor_board_index_extract`
- Phase 011 临时 production probe 期间的 `make run_board_test fetch_board_logs`
- 5 次 `make run_board_bench_compare fetch_board_logs BENCH_ARGS="--case-filter production-index-extract --iterations 20 --warmup-iterations 5"`
- `make run_evidence_doctor_board_production_index_extract`

`log/evidence_registry.json` 尚未接入，矩阵状态依赖人工 freshness（新鲜度）检查和 QEMU / board manifest 的路径登记。`.log` 原始输出和 `evidence_manifest.json` 受 `test-rvv/.gitignore` 规则忽略，不进入默认提交边界；board repeated `summary.md` 和 `evidence_doctor.md` 是可提交摘要证据候选，因为 evaluation 和 phase result 已经引用它们。当前 no-production 结论不发布 `doc-rvv` 长期主题文档。
