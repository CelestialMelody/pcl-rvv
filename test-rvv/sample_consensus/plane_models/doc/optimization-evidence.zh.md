# plane_models normal-plane 优化证据索引

本文是 optimization evidence（优化证据）role，记录当前已采用、暂缓或不适用的优化方式到代码、测试、bench、board 和 Evidence Doctor（证据体检）的映射。跨阶段候选搜索空间归属 `optimization-roadmap.zh.md`。

## 当前结论摘要

| 状态 | 内容 |
| --- | --- |
| adopted | 既有 `f32m2` normal-plane RVV helper 继续保留；公开入口 dispatch / fallback 和 helper buffer contract 已补证。 |
| adopted | phase 010 把 topic-owned C++ source 移到 `src/`，并修正 board fixture 默认参数。 |
| adopted | topic-local Evidence registry（证据登记表）和 manifest wrapper 已自动化，`evidence_status` 当前为 fresh。 |
| adopted | phase 030 已生成 5-run repeated board summary，`repeated_evidence_status` 当前为 fresh。 |
| adopted | phase 040 已采用 source AoS byte-offset dispatch gate，并关闭 `PointXYZI` / `PointXYZINormal` 代表性 source correctness 与 non-AoS source fallback。 |
| adopted | phase 050 已关闭 `PointXYZI + Normal` 和 `PointXYZINormal + Normal` 代表性 source protected helper performance；`phase050_evidence_status` 当前为 fresh。 |
| adopted | phase 060 已关闭 `PointXYZ + PointNormal`、`PointXYZ + PointXYZINormal` 代表性 normal layout public correctness，以及 non-AoS registered normal fallback。 |
| adopted | phase 070 已关闭 `PointXYZI` / `PointXYZINormal` source 与 `PointNormal` / `PointXYZINormal` normal cloud 的 4 个代表性交叉组合 public correctness。 |
| deferred | 更多 source 点型、完整 normal 点型全集、公开入口性能和 `Scalar=double` 扩展未关闭。 |
| not_applicable | 当前 phase 不做新的 RVV math approximation family（数学近似实现族）选择。 |

## 优化方式总表

| candidate family | 代码路径 | 测试路径 | bench / board evidence | asm evidence | decision | 边界 |
| --- | --- | --- | --- | --- | --- | --- |
| existing `f32m2` normal-plane helper | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_plane.hpp` | `src/test_sample_consensus_plane_models.cpp` | `log/board/analyze_bench_compare.log`；9.79x / 11.52x / 10.52x | `build/asm/riscv/bench_sac_normal_plane_rvv.asm` | adopted / retained patch | `PointXYZ + Normal`、float layout、ordered `indices_`。 |
| select helper buffer resize | `selectWithinDistanceStandard`、`selectWithinDistanceRVV` | `SampleConsensusModelNormalPlane.SelectHelperResizesEmptyOutputBuffers` | current board public alias 13/13 passed；historical full board test passed | not_applicable | adopted | 只修复 select helper 输出缓冲区合同。 |
| unsupported curvature fallback | public `select/count/getDistances` fallback gate | `PublicEntriesFallbackForUnregisteredNormalLayout` | board public alias / full board test 可复跑 | not_applicable | adopted | 注册 normal 类型但 curvature 非 float 时回退标量。 |
| source layout split | `Makefile` `SRCS_* := src/*.cpp` | `run_normal_plane_public_tests`、`run_test_compare` | board test / bench 默认路径通过 | not_applicable | adopted | 只移动 topic-owned test/bench source。 |
| board fixture default | `Makefile` target-specific args | `run_board_test fetch_board_logs` | `run_board_bench_compare fetch_board_logs` 默认不需 override | not_applicable | adopted | 只修 host path 到 board path 的 topic-local 参数。 |
| Evidence registry automation | `Makefile`、`script/generate_normal_plane_board_evidence_manifest.py` | `generate_board_evidence_manifest`、`run_board_evidence_doctor`、`record_board_evidence_state`、`evidence_status` | `log/evidence_registry.json` 当前 fresh | not_applicable | adopted | 只登记 summary artifact，不默认提交 raw logs。 |
| repeated board summary | `Makefile`、`script/generate_normal_plane_board_evidence_manifest.py` | `run_board_bench_compare_repeated`、`analyze_board_bench_compare_repeated`、`record_repeated_board_evidence_state`、`repeated_evidence_status` | `log/board/normal-plane-phase030-repeated-board/summary.md`；9.64x / 12.72x / 12.02x median | not_applicable | adopted / positive-stable | protected helper hot path，5-run board evidence，不外推到泛型点类型。 |
| source AoS dispatch gate | `kNormalPlaneRVVLayoutCompatible`、public dispatch、RVV helper `PointLayout` | `PublicEntriesMatchDirectRVVForSupportedLayout`、`PublicEntriesMatchDirectRVVForPointXYZISource`、`PublicEntriesMatchDirectRVVForPointXYZINormalSource`、`PublicEntriesFallbackForNonAoSRegisteredXYZSource` | board public alias 13/13 passed；Phase 050 另补 representative source helper performance | helper asm unchanged from phase 000 | adopted for representative source correctness | source 需满足 `RVVXYZAoSFloatLayout`；normal 需满足 normal/curvature AoS-compatible gate；公开入口计时不是 Phase 050 的证据边界。 |
| representative AoS source performance | `src/bench_sac_normal_plane.cpp`、`Makefile`、`script/generate_normal_plane_board_evidence_manifest.py` | `run_board_bench_compare_phase050`、`record_phase050_evidence_state`、`phase050_evidence_status` | `log/board/normal-plane-phase050-representative-aos-source-performance/summary.md`；`PointXYZI` median 8.04x / 6.10x / 5.41x，`PointXYZINormal` median 7.37x / 8.52x / 8.44x | `dump_bench_rvv` refreshed | adopted / positive-stable | protected helper hot path，不替代 public overload 计时，也不外推到其它 source 或 normal layout。 |
| representative normal layout correctness | `NormalPlaneRVVNormalAoSLayout`、`kNormalPlaneRVVLayoutCompatible` | `PublicEntriesMatchDirectRVVForPointNormalNormalLayout`、`PublicEntriesMatchDirectRVVForPointXYZINormalNormalLayout`、`PublicEntriesFallbackForNonAoSRegisteredNormalLayout` | board public alias 13/13 passed；不新增性能 summary | helper asm unchanged; production source not modified | adopted for representative normal correctness | `PointXYZ` source + representative normal cloud；交叉组合另由 Phase 070 关闭，不证明完整 normal 点型全集。 |
| representative source × normal correctness | `kNormalPlaneRVVLayoutCompatible` 同时实例化 source 与 normal layout gate | `PublicEntriesMatchDirectRVVForPointXYZIAndPointNormal`、`PublicEntriesMatchDirectRVVForPointXYZIAndPointXYZINormal`、`PublicEntriesMatchDirectRVVForPointXYZINormalAndPointNormal`、`PublicEntriesMatchDirectRVVForPointXYZINormalAndPointXYZINormal` | board public alias 13/13 passed；不新增性能 summary | helper asm unchanged; production source not modified | adopted for representative cross correctness | 只证明 4 个代表性交叉组合的 production-public correctness，不证明更多 normal-like 点型或公开入口性能。 |

## 标量路径与 RVV 路径差异

| 阶段 | Standard | RVV | 当前证据 |
| --- | --- | --- | --- |
| index load | `(*indices_)[i]` 标量读取。 | `vle32` 连续加载 `indices_`，再计算 byte offsets。 | asm + helper bench。 |
| point / normal load | 标量字段访问 `pt.x/y/z`、`nt.normal_x/y/z/curvature`。 | indexed gather（按索引离散加载）读取 AoS 字段。 | helper correctness 和 board compare。 |
| distance formula | double / Eigen + `getAngle3D`。 | float `distRVV_f32m2` + `getAcuteAngle3DRVV_f32m2`。 | 容差 correctness；阈值边界允许少量差异。 |
| select writeback | 预分配后按 `current_count` 定址写入。 | mask + `vcompress` 后连续写入 inliers 和 double distances。 | buffer resize RED/GREEN 测试。 |
| getDistances writeback | `distances[i]` double 写入。 | float 结果宽化为 double 后顺序写入。 | `SIMD_getDistancesToModel`。 |

## 细粒度 target 字典

| target | 隔离对象 | 证据状态 |
| --- | --- | --- |
| `run_normal_plane_public_tests` | public dispatch、fallback、select helper resize；Phase 070 后覆盖 13 个 normal-plane public/fallback 用例。 | adopted。 |
| `run_test_compare` | Std/RVV 完整 GTest correctness；Phase 070 后为各 32/32。 | adopted。 |
| `run_board_normal_plane_public_tests` | board public/fallback alias；Phase 070 后为 13/13。 | adopted。 |
| `run_board_test fetch_board_logs` | board 完整 GTest。 | adopted。 |
| `run_board_bench_compare fetch_board_logs` | protected helper board performance。 | adopted。 |
| `dump_bench_rvv` | helper asm attribution。 | adopted。 |
| `generate_board_evidence_manifest` | phase 000 board manifest 重建。 | adopted。 |
| `run_board_evidence_doctor` | Evidence Doctor 报告重建。 | adopted；Errors=0、Warnings=0、Suggestions=0。 |
| `record_board_evidence_state` | 登记 board summary、manifest 和 doctor summary artifact。 | adopted。 |
| `evidence_status` | freshness check（新鲜度检查）。 | adopted；当前输出 fresh。 |
| `run_board_bench_compare_repeated` | 5-run protected helper board compare。 | adopted；已抓回 run_01 到 run_05。 |
| `analyze_board_bench_compare_repeated` | 从 run-labelled（按运行编号分目录保存）compare logs 生成 repeated summary。 | adopted；summary 写入 `log/board/normal-plane-phase030-repeated-board/summary.md`。 |
| `run_repeated_board_evidence_doctor` | repeated manifest / doctor。 | adopted；Errors=0、Warnings=0、Suggestions=0。 |
| `record_repeated_board_evidence_state` | 登记 repeated summary、manifest 和 doctor summary artifact。 | adopted。 |
| `repeated_evidence_status` | repeated freshness check。 | adopted；当前输出 fresh。 |
| `run_board_bench_compare_phase050` | 代表性 AoS source 5-run protected helper board compare。 | adopted；已抓回 run_01 到 run_05。 |
| `record_phase050_evidence_state` | 生成 Phase 050 repeated summary、manifest、doctor 并登记 registry。 | adopted；Evidence Doctor 为 Errors=0、Warnings=5、Suggestions=0。 |
| `phase050_evidence_status` | Phase 050 freshness check。 | adopted；当前输出 fresh。 |
| `run_bench_load` | RVV load strategy historical probe。 | not default；不参与当前 retained patch decision。 |

## 结论边界

当前生产补丁可以保留，但仍是窄范围结论。Phase 040 已把 source gate 收紧到 AoS byte-offset 前提，并证明 `PointXYZI` / `PointXYZINormal` 代表性 source 点型的 public correctness；Phase 050 进一步证明这两个代表点型在 protected helper hot path 中仍为 positive-stable；Phase 060 已证明 `PointNormal` / `PointXYZINormal` 作为 normal cloud 时 public entry 与 direct RVV helper 一致，并补 non-AoS registered normal fallback；Phase 070 已证明 `PointXYZI` / `PointXYZINormal` source 与 `PointNormal` / `PointXYZINormal` normal cloud 的 4 个代表性交叉组合 public correctness。后续若要扩大到更多点型，应先读取 generic point type strategy（泛型点类型策略），再补 traits / layout fallback、编译测试、QEMU correctness、asm、board 和 Evidence Doctor。Evidence registry automation 不改变 RVV 性能结论，但已降低 stale evidence（过期证据）风险；repeated board summary 已把当前 `PointXYZ + Normal` 和两个代表 source 点型的 helper hot-path 性能证据写成独立 positive-stable 条目。
