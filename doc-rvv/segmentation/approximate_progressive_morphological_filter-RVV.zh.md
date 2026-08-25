# approximate_progressive_morphological_filter RVV 生产实现

## 当前状态

`pcl::ApproximateProgressiveMorphologicalFilter<PointT>::extract(Indices&)` 当前已有 adopted production behavior（已采用生产行为）。在 `__RVV10__` 构建中，公开入口先尝试 `apmfExtractRVV`；如果编译期点类型 gate（准入条件）、规模 gate 或 byte offset（字节偏移）gate 不满足，则回到 `extractStd` 标量路径。非 RVV 构建不包含 RVV helper，只执行标量实现。

PI5 EvidenceDecision（生产证据决策）已由用户确认采纳。接入后的 production public board（真实公开入口板卡）证据为 positive：`PointXYZ`、`PointXYZI`、`PointXYZRGB` 和 `PointXYZRGBA` 的 dense / non-dense label 均有 5-run 板卡收益，Evidence Doctor（证据体检）为 Errors=0 / Warnings=0 / Suggestions=0。本文档记录当前已采用的生产实现；性能结论只覆盖这些已测点型、单线程、synthetic production public case，不外推到所有点型或真实多线程场景。

## 函数语义和标量路径

`extract` 用于从输入点云中筛选 ground return（地面点索引）。入口先通过 `initCompute()` 建立输入和 `indices_` 状态，然后生成多轮 window size（窗口尺寸）和 height threshold（高度阈值）。标量路径的主要阶段如下：

| 阶段 | 标量行为 | 输出 / 后续消费者 |
| --- | --- | --- |
| grid z-min | 扫描完整 `input_`，按 `floor((x/y - global_min) / cell_size_)` 找到每个 grid cell（栅格单元）的最小 z。non-dense 输入跳过非有限点。 | `A(row,col)` 保存当前 cell 最小 z。 |
| 初始 ground | dense 输入复制 `indices_`；non-dense 输入只保留 `pcl::isFinite` 的索引。 | `ground` 作为每轮 threshold tail 的候选索引。 |
| window open | 每轮对 `A` 做 min pass，再对中间矩阵 `Z` 做 max pass，得到 `Zf`。 | `Zf(row,col)` 作为该轮局部地面面估计。 |
| threshold tail | 复制当前 `ground` 对应点云，按 `point.z - Zf(erow,ecol) < height_threshold` 保序保留索引。 | `ground` 更新为该轮输出，随后 `A.swap(Zf)`。 |

## 当前采用的优化方式

当前生产实现只让 RVV 接管两个可批量处理的阶段：grid z-min 的 xyz 读取、cell 坐标计算和有限值 mask（掩码），以及每轮 threshold tail 的 indexed xyz gather（按索引离散加载）和 row/col 坐标计算。`Zf(row,col)` lookup（查表）、最终阈值比较与 `out.push_back` 保持标量；050 已验证继续向量化这段剩余逻辑相对 040 基线整体中性，因此不采纳。window open 也保持标量，因为早期 component board（组件板卡）证据显示该片段中性到负向。

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| dispatch / fallback | `extract` 调用 `apmfExtractRVV`，返回 false 时调用 `extractStd`。 | 公开入口保持短分流，标量主体可独立复核。 | production diff + `run_test_compare`。 | 已由用户确认采纳；提交仍需单独授权。 |
| 点类型 gate | 使用 `pcl::rvv::RVVXYZAoSFloatLayout<PointT>`。 | APMF 只读 xyz 并输出 indices，适合 xyz AoS traits gate。 | `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` correctness 和 board。 | 其它点型不自动外推。 |
| grid z-min RVV | strided load xyz，计算 row/col，non-dense 时用 finite mask，压缩到小 staging buffer 后标量更新 cell min。 | cell min 是随机写和冲突敏感状态，压缩后标量更新更稳。 | component positive；production public positive。 | OpenMP 与 RVV 组合未单独证明。 |
| window open | 保留标量 min/max 双 pass。 | 单独 RVV window-open 在板卡上不稳定且收益弱。 | phase 000/010 component evidence。 | 只有 production public 退化或用户要求时再重开 A/B。 |
| threshold tail RVV | indexed load 当前 `ground` 的 xyz，计算 `Zf` lookup 坐标，把 row/col/z/source 暂存后用标量查表、比较和保序输出。 | 离散点云读取和坐标计算适合 RVV；最终 `Zf` 查表不连续，输出追加保持标量更稳。 | component positive；production public positive；050 A/B 拒绝更激进 tail 压缩。 | `ground` 索引有效性沿用 `PCLBase` / 标量路径前提。 |

## VL chunk 算例

假设一个 VL chunk（可变向量长度分块）含 4 个点，`global_min=(0,0)`、`cell_size=1`，加载到的 `(x,y,z)` 为 `(0.2,0.1,1.4)`、`(1.8,0.2,0.9)`、`(1.1,1.6,1.2)`、`(NaN,2.0,0.5)`。

grid z-min RVV 先计算 `col=floor(x)`、`row=floor(y)`，得到前三个有效 lane（向量通道）对应 `(0,0,1.4)`、`(0,1,0.9)`、`(1,1,1.2)`；non-dense 情况下第四个 lane 被 finite mask 排除。RVV 用 `vcompress` 把有效 row、col、z 压缩到连续 staging buffer，随后标量循环按 cell 更新 `A(row,col)` 的最小 z。

threshold tail 阶段对当前 `ground` 做 indexed gather，得到点坐标后计算每个点对应的 row/col。生产实现把 row、col、z 和原索引写入小 staging buffer，再由标量代码执行 `diff = z - Zf(row,col)` 和输出追加。050 曾尝试把 `diff < threshold` 与索引压缩也放入 RVV，但板卡 A/B 不支持采纳。

## 覆盖范围与 fallback

| 条件 | 当前行为 | 证据 |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | `apmfExtractRVV` 不参与编译，`extract` 直接执行 `extractStd`。 | Std 构建 `run_test_compare`。 |
| 点类型不满足 xyz AoS float layout | `if constexpr` 返回 false，公开入口回到 `extractStd`。 | traits gate 源码审计。 |
| 输入规模小于 64 | RVV helper 返回 false 或 tail 阶段回退标量。 | public small fallback test。 |
| 32-bit byte offset 超界 | RVV helper 返回 false。 | `rvvMaxU32ByteOffsetElements<PointT>()` gate 源码审计。 |
| non-dense 输入 | grid z-min RVV 使用 finite mask，初始 `ground` 仍按 `pcl::isFinite` 过滤。 | public non-dense correctness 和 board bench。 |
| window open | 始终标量执行。 | phase 010 负向组件证据和 production diff。 |

## Bench 与证据

production public bench wrapper（真实公开入口性能测试包装）是 `test-rvv/segmentation/approximate_progressive_morphological_filter/src/bench_apmf_production.cpp`。它构造 `ApproximateProgressiveMorphologicalFilter<PointT>`，设置单线程、固定阈值参数，并直接调用 `extract`。计时边界包含 public filter 对象构造后的 `extract` 调用、grid 初始化、多轮 window open 和 threshold tail；checksum 来自输出 indices。

| case | 输入 | run_count | iterations / warmup | median speedup | min / max | decision |
| --- | --- | ---: | --- | ---: | --- | --- |
| `apmf production public dense` | `PointXYZ` synthetic dense cloud，size 262144 | 5 | 8 / 2 | 1.60x | 1.59x / 1.67x | positive |
| `apmf production public non-dense` | `PointXYZ` synthetic non-dense cloud，size 262144 | 5 | 8 / 2 | 1.35x | 1.35x / 1.39x | positive |
| `apmf production public PointXYZI dense` | `PointXYZI` synthetic dense cloud，size 262144 | 5 | 8 / 2 | 1.52x | 1.52x / 1.54x | positive |
| `apmf production public PointXYZI non-dense` | `PointXYZI` synthetic non-dense cloud，size 262144 | 5 | 8 / 2 | 1.42x | 1.42x / 1.44x | positive |
| `apmf production public PointXYZRGB dense` | `PointXYZRGB` synthetic dense cloud，size 262144 | 5 | 8 / 2 | 1.50x | 1.49x / 1.50x | positive |
| `apmf production public PointXYZRGB non-dense` | `PointXYZRGB` synthetic non-dense cloud，size 262144 | 5 | 8 / 2 | 1.43x | 1.42x / 1.50x | positive |
| `apmf production public PointXYZRGBA dense` | `PointXYZRGBA` synthetic dense cloud，size 262144 | 5 | 8 / 2 | 1.51x | 1.50x / 1.55x | positive |
| `apmf production public PointXYZRGBA non-dense` | `PointXYZRGBA` synthetic non-dense cloud，size 262144 | 5 | 8 / 2 | 1.43x | 1.42x / 1.49x | positive |

QEMU（仿真器）只用于 correctness、path-hit（路径命中）和 log-shape（日志形状）检查，不作为性能证据。

## 正确性与高效性证据链

| 证据层 | 路径 / 命令 | 当前结果 | 结论边界 |
| --- | --- | --- | --- |
| correctness | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter run_test_compare` | 最终状态 Std / RVV 均通过 14 个测试。 | 覆盖 diagnostic component/full、public dense、public non-dense、小规模 fallback、`PointXYZI`、`PointXYZRGB` 和 `PointXYZRGBA` public 对拍。 |
| production asm | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter check_production_rvv_asm` | 最终状态 production bench RVV binary 包含 `apmfExtractRVV` 和关键 RVV 指令。 | 证明生产二进制包含目标 helper，不证明所有 fallback case 都走 RVV。 |
| board performance | `test-rvv/segmentation/approximate_progressive_morphological_filter/log/board/production-public-repeated-v2/summary.md`、`log/board/point-type-production-repeated-v1/summary.md` | 已测 8 个 production public label 均为 positive。 | 只覆盖已测点型、单线程、synthetic production public case。 |
| Evidence Doctor | `test-rvv/segmentation/approximate_progressive_morphological_filter/log/board/production-public-repeated-v2/evidence_doctor.md`、`log/board/point-type-production-repeated-v1/evidence_doctor.md` | 两批 production public evidence 均 Errors=0 / Warnings=0 / Suggestions=0。 | 环境字段 governor/freq/temperature/VLEN 未记录；当前脚本未把该缺失升为 finding。 |
| 050 rejected probe | `test-rvv/segmentation/approximate_progressive_morphological_filter/log/board/tail-vector-filter-probe-v1/summary.md`、`doc/phases/050-tail-vector-filter-probe/result.zh.md` | 050 相对 040 RVV 基线整体 neutral，已回退。 | 说明更激进 tail 比较 / 压缩不作为当前生产行为。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 下游 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- |
| `ApproximateProgressiveMorphologicalFilter<PointT>::extract` | production public entry | 公开入口、init/deinit 和 RVV/Std dispatch。 | 用户代码、tools、bench wrapper。 | production boundary | `segmentation/include/pcl/segmentation/impl/approximate_progressive_morphological_filter.hpp` |
| `extractStd` | production scalar fallback | 保留原标量主体。 | `extract` fallback。 | fallback correctness | 同上 |
| `apmfExtractRVV` | production RVV helper | 已采用的生产 RVV helper，组织 grid z-min、标量 window-open 和 tail threshold。 | `extract` short-circuit。 | production direct + asm | 同上 |
| `pcl::detail::apmf_rvv::computeGridZMinRVV` | production detail helper | RVV 计算 row/col/z staging，并由标量更新 grid min。 | `apmfExtractRVV`。 | RVV path | 同上 |
| `pcl::detail::apmf_rvv::thresholdGroundRVV` | production detail helper | RVV indexed gather 和 row/col staging，最终查表、比较、输出保留标量。 | `apmfExtractRVV`。 | RVV path | 同上 |
| `src/test_apmf.cpp` | test-rvv correctness | component/full diagnostic 和 public direct 对拍。 | `run_test_compare`。 | correctness / fallback | `test-rvv/segmentation/approximate_progressive_morphological_filter/src/test_apmf.cpp` |
| `src/bench_apmf_production.cpp` | test-rvv bench | 真实公开入口 production public bench。 | board/QEMU production targets。 | board performance | `test-rvv/segmentation/approximate_progressive_morphological_filter/src/bench_apmf_production.cpp` |
| `script/generate_apmf_board_evidence_manifest.py` | analysis script | 生成 repeated board summary 和 Evidence Doctor manifest。 | `run_board_evidence_doctor`。 | evidence manifest | `test-rvv/segmentation/approximate_progressive_morphological_filter/script/generate_apmf_board_evidence_manifest.py` |
| `doc/approximate_progressive_morphological_filter-evaluation.zh.md` | topic-local evaluation | 决策审计、候选取舍和未覆盖范围。 | worker / reviewer。 | decision record | `test-rvv/segmentation/approximate_progressive_morphological_filter/doc/approximate_progressive_morphological_filter-evaluation.zh.md` |
| phase suite | topic-local phase docs | 阶段计划、结果、矩阵和恢复入口。 | worker / reviewer。 | recovery pointer | `test-rvv/segmentation/approximate_progressive_morphological_filter/doc/phases/` |

## 未覆盖范围和后续方向

其它 `PCL_XYZ_POINT_TYPES` 只由 traits gate、显式实例化编译和源码审计支撑，不能把已测点型 board speedup 外推为所有点型性能结论。当前已测常用 xyz AoS 点型全部 positive，因此继续扩展更多点型属于覆盖验证，不是当前证据下值得自动推进的生产优化方式。

window-open RVV 当前不建议继续接入。当前生产实现已经用标量 window-open 获得 positive production public 结果；只有后续场景显示 window-open 成为瓶颈、production public 结果退化，或 reviewer 明确要求同边界 A/B 时，才值得重开。

OpenMP 多线程与 RVV 的组合未单独测量。当前 production bench 使用 `setNumberOfThreads(1)`，因此多线程收益和调度交互应作为独立 profiling / bench 主题处理。

## Production closeout 检查点

当前文档状态为 adopted production behavior。freshness check（新鲜度检查）已覆盖 production diff、`run_test_compare`、`check_production_rvv_asm`、`PointXYZ` board summary、040 点型扩展 board summary、050 rejected probe 和 Evidence Doctor。提交仍需单独用户授权；若后续要求回滚，应先撤回 production 补丁，再把本文档改为历史候选或删除。
