# extract_polygonal_prism_data 函数级评估

## 范围和目标源码

目标源码为 `segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp`。当前评估对象是 `ExtractPolygonalPrismData<PointT>::segment` 的投影后逐点扫描段：height mask（高度范围筛选）、point-to-plane signed distance（点到平面有符号距离）、投影点按 `k1/k2` 选择的 polygon predicate（二维多边形判定）和保序 `output.indices` 压缩。

## 函数级结论

Phase 040 已把 full-scan single polygon RVV 路径接入真实 public entry（公开入口）并完成 production public（公开入口生产证据）验证。Phase 045 根据用户对 PI5 正收益的采纳确认完成 S11 production closeout（生产收口）。Phase 050 继续把 concave hull（凹包）多 polygon XOR（多个多边形内外结果异或）纳入同一生产 RVV 扫描段。Phase 060 把 `PointXYZI`、`PointXYZRGB` 和 `PointXYZRGBA` 纳入当前 production RVV 点型范围，同时把 `PointXYZINormal` 这类 `sizeof(PointT) > 32` 的宽 stride 点型显式回退标量。Phase 070 又把 production RVV 的 work item count 阈值从 64 降到 32。当前生产补丁在 `__RVV10__` 构建下先尝试 `segmentRvv`，不满足 gate（验收条件）时回到 `segmentStd`；非 RVV 构建只走标量实现。

EvidenceDecision（证据决策）是 `adopted_production_behavior`。Milkv-Jupiter production public repeated board（重复板卡性能测试）结果为：single polygon dense / indexed median 均为 1.75x；nested polygon dense median 2.18x、min 2.11x、max 2.23x；nested polygon indexed median 2.13x、min 2.11x、max 2.17x；Phase 060 post-gate 点型结果为 `PointXYZI` median 1.85x、`PointXYZRGB` median 1.83x、`PointXYZRGBA` median 1.84x，三组 Evidence Doctor（证据体检）均为 Errors=0 / Warnings=0 / Suggestions=0；Phase 070 threshold32 confirm5 为 median 1.19x、min 1.18x、max 1.38x，doctor 为 0 / 0 / 0。`PointXYZINormal` post-gate fallback confirmation median 1.00x，doctor 为 0 / 1 / 1，不写成 RVV 收益。正式 production 长期主题文档为 `doc-rvv/segmentation/extract_polygonal_prism_data-RVV.zh.md`。

## 函数 / 函数族作用速览

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | 与主流程关系 | RVV 判断 |
| --- | --- | --- | --- | --- |
| `ExtractPolygonalPrismData<PointT>::segment` | 从输入点云中筛出位于 polygonal prism（多边形棱柱）内的点索引 | 读取 `input_`、`indices_`、`planar_hull_`、`polygons_`；写 `output.indices` | 公开入口 | Phase 045 已完成有界 RVV dispatch 采纳 |
| `segmentStd` | 保存原标量主体 | 与旧 `segment` 语义一致 | RVV fallback | 非 RVV 构建或 RVV gate 不满足时调用 |
| `segmentRvv` | 在合法 single / nested polygon 范围内向量化扫描段 | 读取真实对象状态，调用原有 plane setup 与 `projectPoints`，RVV 扫描输出 indices | RVV helper | production public 证据正向 |
| `SampleConsensusModelPlane::projectPoints` | 把 `indices_` 对应点投影到 hull 平面 | 生成 `projected_points` | 扫描段前置成本 | 保持标量，计入 production bench |
| `isXYPointIn2DXYPolygon` | 单点对 polygon edge 做奇偶判定 | 输入投影点和二维 polygon | 标量 fallback 内继续使用 | RVV path 内用向量 mask / XOR 复刻同一语义 |
| concave hull polygon XOR | 多个 polygon 的 inside 结果按 XOR 合并 | `polygons_` 非空且多于 1 个时使用 `Vertices` 分组 | 保持凹包洞语义 | Phase 050 已纳入 production RVV |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `ExtractPolygonalPrismData<PointT>::segment` | production public entry | 真实公开入口，执行 RVV 短路分流和标量 fallback | 用户调用 `segment` | `segmentRvv`、`segmentStd` | production boundary | `segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp` |
| `segmentStd` | production Std helper | 原标量主体 | `segment` fallback、production direct tests | `projectPoints`、`isXYPointIn2DXYPolygon` | scalar baseline / fallback | `segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp` |
| `segmentRvv` | production RVV helper | polygon 扫描段 RVV 化，覆盖 single / nested polygon | `segment` | `pcl::rvv_load`、RVV intrinsic、`vcompress` | production public candidate | `segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp` |
| `segmentPolygonalPrismFullScanReference` | diagnostic reference | 复刻真实扫描段的平面距离、`projected_points[k1/k2]` 和 polygon XOR | `src/test_eppd.cpp`、`src/bench_eppd.cpp` | diagnostic baseline | correctness / benchmark baseline | `include/impl/eppd_reference.hpp` |
| `segmentPolygonalPrismRvvFullScanCandidate` | candidate formula / output compress | RVV 化 full-scan polygon 诊断路径 | RVV test / bench | plane distance mask、polygon parity / XOR、`vcompress` | production-shaped diagnostic | `include/impl/eppd_candidates.hpp` |
| indexed gather branch | row source adapter | 非 dense indices 下按 source index gather 原始点字段，投影点保持扫描顺序 | diagnostic helper 和 production helper | 32-bit byte offset gate、source-index compress | indexed row source evidence | `include/impl/eppd_candidates.hpp`、production `segmentRvv` |
| `test_eppd.cpp` | correctness gate（正确性验收） | diagnostic、production direct 和 fallback 测试 | `make run_test_compare`、`make run_board_test` | QEMU / board test logs | correctness / fallback | `src/test_eppd.cpp` |
| `bench_eppd.cpp` | bench wrapper | 支持 `--path diagnostic|production` 和 dense / indexed case | `make run_bench_*`、board targets | compare script | diagnostic / production public performance | `src/bench_eppd.cpp` |
| `generate_eppd_board_evidence_manifest.py` | analysis script | 生成 repeated summary 和 Evidence Doctor manifest | board repeated targets | `evidence_doctor.py` | evidence summary | `script/generate_eppd_board_evidence_manifest.py` |
| board repeated summaries | evidence output summary | 保存 diagnostic 和 production public 5-run speedup | board repeated targets | evaluation / phase result / Handoff | board performance | `log/board/repeated*/summary.md` |
| Phase 060 point-type summaries | evidence output summary | 保存 post-gate 点型收益和 `PointXYZINormal` fallback confirmation | point-type board targets | Phase 060 result、本 evaluation、长期主题文档 | production public / fallback confirmation | `log/board/repeated-production-*-postgate/summary.md` |
| Phase 070 threshold summaries | evidence output summary | 保存 32 阈值初筛和 confirm5 采纳证据 | threshold sweep / confirmation runs | Phase 070 result、本 evaluation、长期主题文档 | production public threshold evidence | `log/board/repeated-production-size*-threshold32*/summary.md` |

## 文档归属

| role | 主归属 | 说明 |
| --- | --- | --- |
| testing_overview | `doc/testing-overview.zh.md` | 运行入口分类、覆盖矩阵、QEMU / board 边界 |
| correctness_tests | `doc/correctness-tests.zh.md` | gtest 输入、断言、证明范围和不能外推范围 |
| benchmark_and_evidence | `doc/benchmark-and-evidence.zh.md` | bench CLI、case label、summary / manifest / Evidence Doctor 和 registry |
| optimization_evidence | `doc/optimization-evidence.zh.md` | adopted / rejected / deferred 路线到代码、target 和 evidence 的映射 |
| test_support_code_map | `doc/test-support-code-map.zh.md` | 聚合头、internal helper、src、script 和 production helper |
| production_topic_doc | `doc-rvv/segmentation/extract_polygonal_prism_data-RVV.zh.md` | 当前 adopted production behavior 的长期维护说明 |

## 标量流程与 RVV 流程对照

| 阶段 | production 标量流程 | Phase 040 production RVV 流程 | 边界 |
| --- | --- | --- | --- |
| entry dispatch | `segment` 直接执行完整标量主体 | `segment` 在 `__RVV10__` 下先调用 `segmentRvv`，失败后调用 `segmentStd` | public API 不变 |
| plane setup | 从 hull covariance / eigenvector 得到平面系数，并按 viewpoint 翻转 | 与标量流程保持一致，仍在 `segmentRvv` 中用标量 Eigen / PCL helper 完成 | 计入 production bench，但不是 RVV 化阶段 |
| projection | `projectPoints(*indices_, model_coefficients, projected_points, false)` | 同样调用 `projectPoints` | 计入 production bench |
| height mask | 对原始点做 `pointToPlaneDistanceSigned` 后比较高度上下限 | RVV 用 `a*x+b*y+c*z+d` 形成 signed distance，再比较 height limits | 使用 `float` distance，与 Phase 010 / tests 对齐 |
| polygon predicate | 每个点遍历 polygon edges，条件成立时翻转 inside；多个 polygon 之间做 XOR | RVV 对一个 edge 同时处理一组点，用 `vmxor` 累计 inside，并在 polygon 之间继续 XOR；坐标来自 `projected_points[k1/k2]` | 覆盖合法 single / nested polygon |
| output compress | 标量按扫描顺序写入 `output.indices` | RVV 用 `vcompress` 压缩 kept lanes；dense 输出 lane id，indexed 输出 source index | 保序输出由 tests 覆盖 |

## fallback 矩阵

| gate | production 行为 | 证据 |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | public `segment` 直接调用 `segmentStd` | `make run_test_compare` 中 Std build 通过 |
| `RVVXYZAoSFloatLayout<PointT>` 不满足 | 编译期返回 false，fallback 到 `segmentStd` | 非覆盖点类型保持标量 |
| `sizeof(PointT) > 32` | 编译期返回 false，fallback 到 `segmentStd` | Phase 060：`PointXYZINormal` 接入前 20-run 不稳定，post-gate fallback confirmation median 1.00x |
| work item count 小于 32 | fallback 到 `segmentStd` | `SegmentRvvDeclinesSmallInputs`；Phase 070 threshold32 confirm5 支撑 32 点进入 RVV |
| invalid index 或 32-bit byte offset 超界 | fallback 到 `segmentStd` | production helper 逐项检查 index 范围，cloud size 检查 byte offset |
| active polygon 顶点不足、polygon 顶点索引越界或 projection 尺寸不一致 | fallback 到 `segmentStd` | `SegmentRvvDeclinesDegeneratePolygons`；异常时 public entry 继续标量 |

## 测试计划和结果

| 测试 / target | 层级 | 结果 | 能证明 | 不能证明 |
| --- | --- | --- | --- | --- |
| `make run_test_compare` | correctness / fallback | pass；Std 2 tests，RVV 16 tests | diagnostic helper、production direct helper、nested polygon、点型扩展和 fallback tests 通过 | 目标硬件性能 |
| `make run_board_test` | board correctness | pass；16 tests | 板卡上 production direct / fallback 测试通过 | repeated performance |
| QEMU production bench smoke | build / log-shape smoke | dense 和 indexed Std/RVV checksum 一致 | production bench 二进制可运行、日志可解析 | 性能结论 |
| `make dump_bench_rvv` | asm attribution（反汇编归属） | pass | `segmentRvv` 符号内出现 `vlse32.v`、`vluxseg3ei32.v`、`vfmacc.vf`、`vmxor.mm`、`vcompress.vm` | 单独证明运行时热度 |
| `make run_board_eppd_repeated` | diagnostic board performance | pass；median 1.98x | dense full-scan diagnostic 正向 | production public 收益 |
| `make run_board_eppd_repeated_indexed` | diagnostic board performance | pass；median 2.20x | indexed full-scan diagnostic 正向 | production public 收益 |
| `make run_board_eppd_repeated_production` | production public board performance | pass；median 1.75x | 真实 public entry dense RVV 正向 | 未覆盖点类型或多 polygon |
| `make run_board_eppd_repeated_production_indexed` | production public board performance | pass；median 1.75x | 真实 public entry indexed RVV 正向 | invalid indices 或其它点类型性能 |
| `make run_board_eppd_repeated_production_nested` | production public board performance | pass；median 2.18x | 真实 public entry nested dense RVV 正向 | 非法 polygon 或其它点类型性能 |
| `make run_board_eppd_repeated_production_nested_indexed` | production public board performance | pass；median 2.13x | 真实 public entry nested indexed RVV 正向 | 非法 polygon 或其它点类型性能 |
| `make run_board_eppd_repeated_production_pointtypes` | production public board performance | pass；post-gate `PointXYZI` 1.85x、`PointXYZRGB` 1.83x、`PointXYZRGBA` 1.84x；`PointXYZINormal` fallback 1.00x | 三种 <=32-byte 常见点型 RVV 正向；宽 stride 点型回退 | 用户自定义点型、nested/indexed 点型组合性能 |
| Phase 070 threshold32 confirm5 | production public board performance | pass；32 点 median 1.19x、min 1.18x、max 1.38x；doctor 0 / 0 / 0 | `indices_->size() >= 32` 的小规模生产入口仍有正收益 | 低于 32、其它点型小规模、nested 小规模 |

## 正确性与高效性证据链

当前 correctness（正确性）证据覆盖真实 public entry 的 Std/RVV 对拍、single / nested polygon、dense / indexed 输入、`<32` small input fallback、degenerate polygon fallback、`PointXYZI` / `PointXYZRGB` / `PointXYZRGBA` traits-gated 路径，以及 `PointXYZINormal` 显式 fallback。当前 asm attribution（反汇编归属）显示 production `segmentRvv` 符号内出现预期 RVV 指令。当前 board performance（板卡性能）证据来自 production public repeated summary，single / nested 的 dense 和 indexed 都进入 positive bucket；Phase 060 post-gate 点型 summary 也支持 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 采纳；Phase 070 confirm5 支持阈值降到 32。Evidence Doctor 中，已采纳生产收益组均为 0 / 0 / 0；`PointXYZINormal` post-gate 为 0 / 1 / 1，作为 fallback confirmation 记录。

这些证据支持并已经用于采纳当前有界生产补丁。它们不覆盖 `projectPoints` 本身向量化、所有 PointXYZ-like 点类型、`PointXYZINormal` / 更宽 AoS stride、非 float xyz layout、invalid index 容错性能或低于 32 点强行 RVV。后续若扩大范围，需要单独 phase、correctness、asm、board repeated 和 Evidence Doctor。

## 生产接入判断

`EvidenceDecision = adopted_production_behavior`。当前生产补丁已经完成 S11 production closeout，并创建正式 `doc-rvv/segmentation/extract_polygonal_prism_data-RVV.zh.md`。Phase 050 已证明 concave hull 多 polygon XOR 在生产公开入口中有稳定正收益并接入。Phase 060 已证明 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 值得纳入当前生产 RVV 范围，并把 `PointXYZINormal` 降级为标量 fallback。Phase 070 已证明 `indices_->size() >= 32` 的规模阈值值得采纳。当前 topic 内剩余方向要么需要 dedicated wide-stride phase，要么越过本文件扫描段进入 `projectPoints` component ablation（组件消融）或新点型 / `Scalar` 边界；因此默认暂停自动优化循环，进入 review / 提交判断。
