# approximate_progressive_morphological_filter 函数级评估

## S2 函数级评估

目标入口是 `pcl::ApproximateProgressiveMorphologicalFilter<PointT>::extract(Indices& ground)`，源码位于 `segmentation/include/pcl/segmentation/impl/approximate_progressive_morphological_filter.hpp`。该入口先通过 `initCompute()` 建立输入状态，再生成 window size（窗口尺寸）和 height threshold（高度阈值），对完整输入点云建立 grid z-min（栅格最小 z），然后多轮执行 morphological open（形态学开运算），每轮按 `point.z - Zf(row,col) < threshold` 保序更新 `ground`。

当前热点来自三个片段：

| 片段 | trip count（循环规模来源） | RVV 价值 | 生产风险 |
| --- | --- | --- | --- |
| grid z-min | `input_->size()` | 可批量读取 `x/y/z`，计算 row/col 和有限值 mask | 同 cell min 更新仍是标量随机写；OpenMP 分工需重新审计 |
| window open | `rows * cols * window_area` | Eigen `MatrixXf` 列主序可按列连续加载 row 区间 | 000 和 010 板卡证据显示该组件中性到负向，不能作为独立生产收益 |
| threshold tail | `ground.size()` 每轮变化 | 可用 indexed load、row/col 计算和 staging 减少点云读取成本 | `Zf` lookup 不连续；050 证明继续向量化最终比较 / 输出压缩整体不值得 |

## 当前证据链

| 证据 | 路径 / 命令 | 结果 | 不能证明什么 |
| --- | --- | --- | --- |
| diagnostic correctness（诊断正确性） | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter run_test_compare` | Std / RVV 均通过 component/full diagnostic 测试 | 不证明生产入口 dispatch |
| QEMU smoke（QEMU 小型冒烟） | `ALLOW_QEMU_BENCH_COMPARE=1 ... run_bench_compare` | full-pipeline / point-type label 和 checksum 可解析 | QEMU timing 不作为性能结论 |
| diagnostic asm attribution（诊断反汇编归属） | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter dump_bench_rvv` | 诊断二进制包含 floor、load/store、gather、compress 等 RVV 指令 | 不是 production hot symbol |
| full-pipeline board | `log/board/full-pipeline-repeated/summary.md` | full-pipeline 5-run median 1.31x，positive | 不是 public overload，也不覆盖 OpenMP 真实调度 |
| production direct correctness（真实生产路径正确性） | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter run_test_compare` | 最终状态 Std / RVV 各 14/14 passed | 不证明所有 xyz 点型性能 |
| production asm attribution（生产反汇编归属） | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter check_production_rvv_asm` | 最终状态 passed；production bench RVV 二进制包含 `apmfExtractRVV` 和关键 RVV 指令 | 不证明运行时每个 fallback case 都命中 RVV |
| production public board - `PointXYZ` | `log/board/production-public-repeated-v2/summary.md` | dense median 1.60x，non-dense median 1.35x，5-run positive | 不证明其它点型、多线程或真实数据分布 |
| production public board - measured point types | `log/board/point-type-production-repeated-v1/summary.md` | `PointXYZ` 1.48x / 1.28x；`PointXYZI` 1.52x / 1.42x；`PointXYZRGB` 1.50x / 1.43x；`PointXYZRGBA` 1.51x / 1.43x | 不外推到其它 `PCL_XYZ_POINT_TYPES` 或用户自定义点型 |
| production Evidence Doctor（证据体检） | `production-public-repeated-v2/evidence_doctor.md`、`point-type-production-repeated-v1/evidence_doctor.md` | 两批 production public evidence 均 Errors=0 / Warnings=0 / Suggestions=0 | governor/freq/temperature/VLEN 未记录；当前脚本未把该缺失升为 finding |
| 050 RVV-vs-RVV A/B | `log/board/tail-vector-filter-probe-v1/summary.md`、`evidence_doctor.md` | Std/RVV 仍 positive，但相对 040 RVV 基线整体 neutral；050 已回退 | 不支持把 tail 最终比较 / 输出压缩写成 adopted |

## 候选取舍

当前 EvidenceDecision（证据决策）是 `adopted production behavior`：production-shaped diagnostic（生产形态诊断）显示 full pipeline 在板卡上稳定正向，phase 030 用真实 public overload（公开入口重载）证明 `PointXYZ` dense / non-dense positive；phase 040 又用接入后的 production public board 证明 `PointXYZI`、`PointXYZRGB` 和 `PointXYZRGBA` 也为 positive。用户已确认“板卡上的测试结果如果显示有收益即可采纳”。提交仍需单独授权。

`window-open-row-reduction` 单独组件为中性 / 负向，production patch 没有采用。`tail-vector-filter-compress` 在 050 中完成 production probe，但相对 040 已采纳 RVV 基线整体中性，已回退。当前生产路径只保留证据支持的 grid z-min RVV 和 threshold tail 的 indexed gather / row-col staging。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 上游 / 下游 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- |
| `ApproximateProgressiveMorphologicalFilter<PointT>::extract` | production public entry | 真实公开入口和标量语义来源 | 上游工具 / 用户代码调用；下游 RVV/Std 分流 | production boundary | `segmentation/include/pcl/segmentation/impl/approximate_progressive_morphological_filter.hpp` |
| `extractStd` | production scalar fallback | 非 RVV 构建与 RVV gate 失败后的完整标量实现 | `extract` 调用 | fallback boundary | 同上 |
| `apmfExtractRVV` | production RVV helper | traits-gated RVV 生产实现，接管 grid z-min 与 threshold tail staging | `extract` 调用；失败返回 false | production direct + asm attribution | 同上 |
| `pcl::detail::apmf_rvv::computeGridZMinRVV` | production detail helper | strided xyz load、row/col floor、finite mask、staging 后标量 cell min 更新 | `apmfExtractRVV` | adopted RVV path | 同上 |
| `pcl::detail::apmf_rvv::thresholdGroundRVV` | production detail helper | indexed xyz gather、row/col floor、staging；最终 `Zf` lookup 和输出保留标量 | `apmfExtractRVV` | adopted RVV path | 同上 |
| `src/test_apmf.cpp` | test-rvv correctness | component/full diagnostic 和 public direct 对拍 | `run_test_compare` | correctness / fallback | `test-rvv/segmentation/approximate_progressive_morphological_filter/src/test_apmf.cpp` |
| `src/bench_apmf_production.cpp` | production bench wrapper | 真实公开入口 production public bench | board/QEMU production targets | production-public performance | `test-rvv/segmentation/approximate_progressive_morphological_filter/src/bench_apmf_production.cpp` |
| `script/generate_apmf_board_evidence_manifest.py` | analysis script | 解析 repeated board logs，生成 summary 和 manifest | Evidence Doctor 调用 | evidence manifest | `test-rvv/segmentation/approximate_progressive_morphological_filter/script/generate_apmf_board_evidence_manifest.py` |
| phase docs | documentation section | 保存阶段计划、结果、矩阵和恢复入口 | worker / reviewer 恢复 | recovery pointer | `test-rvv/segmentation/approximate_progressive_morphological_filter/doc/phases/` |
| `doc-rvv/segmentation/approximate_progressive_morphological_filter-RVV.zh.md` | production topic doc | 保存已采用生产实现、证据链和维护边界 | reviewer / 后续 worker | adopted production documentation | `doc-rvv/segmentation/approximate_progressive_morphological_filter-RVV.zh.md` |

## production 接入判断

phase 030-040 已完成生产闭环：

- production scope（生产范围）：`extract(Indices&)`；`__RVV10__` + `RVVXYZAoSFloatLayout<PointT>` + 规模 / offset gate。
- production patch：`extractStd` 保留完整标量主体，`apmfExtractRVV` 返回 false 时自然 fallback。
- RVV 覆盖：grid z-min 和 threshold tail 的 indexed xyz gather / row-col staging；window-open 和 tail 最终输出更新保持标量。
- production direct tests：真实 public `extract` 的 dense、non-dense、小规模 fallback、`PointXYZI`、`PointXYZRGB` 和 `PointXYZRGBA` 对拍。
- production evidence：已测点型 5-run board production public positive，Evidence Doctor clean。

当前建议：保留已采纳生产补丁。050 已证明继续把 tail 最终比较 / 输出压缩向量化不值得，当前没有值得自动推进的下一生产优化候选。

## 未覆盖范围和恢复条件

| 未覆盖范围 | 当前处理 | 恢复条件 |
| --- | --- | --- |
| 其它 `PCL_XYZ_POINT_TYPES` | 当前不外推；已测常用 xyz AoS 点型均 positive | 用户或 reviewer 要求完整点型覆盖矩阵时另建 point-type coverage phase。 |
| window-open RVV | 不接入 production | production public 后续退化、profile 证明 window-open 成为瓶颈，或用户要求重开 A/B。 |
| tail 最终比较 / 输出压缩 RVV | 050 已尝试并回退 | 新输入分布显示 tail 输出比例显著不同，或出现更低 staging 成本的新实现。 |
| OpenMP 多线程 + RVV | 当前 bench 使用 `setNumberOfThreads(1)` | 需要真实多线程应用场景或 profile 后另建 phase。 |
