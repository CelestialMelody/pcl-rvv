# sac_model_stick RVV 函数级评估

## S2 函数级评估

`SampleConsensusModelStick<PointT>` 是 deprecated（已废弃）的 3D stick 模型。公开距离相关入口包括 `getDistancesToModel`、`selectWithinDistance` 和 `countWithinDistance`，源码位于 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp`。

当前已完成三个 RVV 诊断目标，并在 Phase 080 把三条公开入口接入 production direct（真实生产入口直连）RVV 路径：

- `countWithinDistance`：先验证 model coefficients（模型系数），把系数 0-2 当作第一个线端点、3-5 当作第二个线端点，归一化 `line_pt2 - line_pt1`，然后按 `indices_` 扫描点云。每个点计算到该方向线的平方距离，小于 `threshold^2` 时计入 `nr_i`，落在 `[threshold^2, 4 * threshold^2)` 时计入 `nr_o`，最后返回 `nr_i <= nr_o ? 0 : nr_i - nr_o`。
- `selectWithinDistance`：使用同一个端点语义和平方距离公式，小于 `threshold^2` 时按 `indices_` 扫描顺序输出原始点云 index，并把同一个平方距离以 `double` 写入 `error_sqr_dists_`。它没有 count 的外圈惩罚。
- `getDistancesToModel`：当前源码把系数 0-2 当作 line point（线上的点），把系数 3-5 直接当作 line direction（线方向）并归一化，不是第二个端点。它按 `indices_` 顺序写 dense（稠密）`distances` 输出；`sqr_distance < radius_max_^2` 时写 `sqrt(sqr_distance)`，否则写 `2 * sqrt(sqr_distance)`。

这三段循环都是 direct-main-path（直接主路径）：每个 index 都独立读取 xyz 并执行固定公式，适合做 RVV indexed gather（索引离散加载）、mask（掩码）、`vcompress`（向量压缩）或 `vfsqrt`（向量平方根）诊断。count/select 与 getDistances 的系数语义不同，因此三条入口的生产接入必须分别批准。

## Traceability Map（可追踪性地图）

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `SampleConsensusModelStick<PointT>::countWithinDistance` | production public entry | 公开入口、RVV short-circuit（短路分流）和 Standard fallback（标量回退） | SAC 模型评分 | `countWithinDistanceRVV` / `countWithinDistanceStandard` | production boundary（生产边界） | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp` |
| `SampleConsensusModelStick<PointT>::selectWithinDistance` | production public entry | 公开入口、RVV short-circuit 和 Standard fallback | SAC 模型内点选择 | `selectWithinDistanceRVV` / `selectWithinDistanceStandard`、`error_sqr_dists_` | production boundary | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp` |
| `SampleConsensusModelStick<PointT>::getDistancesToModel` | production public entry | 公开入口、RVV short-circuit 和 Standard fallback | SAC / MLESAC / MSAC 等上游模型评分 | `getDistancesToModelRVV` / `getDistancesToModelStandard`、dense `distances` 输出 | production boundary | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp` |
| `*Standard` helpers | production fallback | 保留原标量语义，供非 RVV 构建或 gate 不满足时调用 | public entries / RVV helpers | 原标量循环 | fallback coverage（回退覆盖） | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp` |
| `*RVV` helpers | production RVV helper | 承载 indexed gather、mask、`vcompress` 和 `vfsqrt` 实现 | public entries | `pcl::rvv_load` xyz AoS load wrapper | asm attribution（反汇编归属） | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp` |
| `test_sac_model_stick.cpp` | correctness test | 对拍公开入口和测试专用 candidate | `make run_test_compare` | diagnostic helper、production public entry | correctness gate（正确性验收） | `test-rvv/sample_consensus/sac_model_stick/src/test_sac_model_stick.cpp` |
| `bench_sac_model_stick.cpp` | bench wrapper | 计时三条 public entry | `board_smoke` / board repeated | analyzer / Evidence Doctor | production direct bench | `test-rvv/sample_consensus/sac_model_stick/src/bench_sac_model_stick.cpp` |
| `doc/testing-overview.zh.md` | documentation role | 测试入口分类、target 粒度和证据边界 | worker / reviewer | README、correctness、benchmark 文档 | recovery pointer（恢复入口） | `test-rvv/sample_consensus/sac_model_stick/doc/testing-overview.zh.md` |
| `doc/correctness-tests.zh.md` | documentation role | 11 个 gtest 的输入、断言和证明范围 | worker / reviewer | test source、diagnostic helper、production public entry | correctness explanation | `test-rvv/sample_consensus/sac_model_stick/doc/correctness-tests.zh.md` |
| `doc/benchmark-and-evidence.zh.md` | documentation role | bench 输出、board summary、manifest、doctor 和 registry 边界 | worker / reviewer | bench source、script、summary evidence | evidence explanation | `test-rvv/sample_consensus/sac_model_stick/doc/benchmark-and-evidence.zh.md` |
| `doc/optimization-evidence.zh.md` | documentation role | candidate family 与证据、决策和 PI gate 的索引 | worker / reviewer | roadmap、matrix、phase results | decision explanation | `test-rvv/sample_consensus/sac_model_stick/doc/optimization-evidence.zh.md` |
| `doc/test-support-code-map.zh.md` | documentation role | test-only helper、bench wrapper、script 和 output 定位 | worker / reviewer | source tree、Makefile | traceability support | `test-rvv/sample_consensus/sac_model_stick/doc/test-support-code-map.zh.md` |
| `optimization-matrix.zh.md` | documentation section | 记录候选族、证据状态和下一步 | worker / reviewer | Handoff | recovery pointer（恢复入口） | `test-rvv/sample_consensus/sac_model_stick/doc/phases/optimization-matrix.zh.md` |
| `doc-rvv/sample_consensus/sac_model_stick-RVV.zh.md` | production topic doc | 保存 adopted production behavior（已采用生产行为）、证据链和长期边界 | worker / reviewer | evaluation、phase result | long-term doc | `doc-rvv/sample_consensus/sac_model_stick-RVV.zh.md` |

## 当前判断

当前判断是 `production-adopted`：

- Phase 080 已把 `countWithinDistance`、`selectWithinDistance` 和 `getDistancesToModel` 三条 public entry 接入 RVV 生产路径。
- `run_test_compare` 在 Std/RVV 两个构建中各通过 11 个 gtest，覆盖历史 candidate 回归、接入后的公开入口语义和代表点型 correctness。
- `check_production_asm` 证明 `countWithinDistanceRVV`、`selectWithinDistanceRVV` 和 `getDistancesToModelRVV` 三个生产 helper 均命中 RVV 指令；Phase 100 后还要求 `getDistancesToModelRVV` 源码无 staged scalar lane（暂存后逐向量通道标量写回），并在 asm 中出现 `vfwcvt.f.f.v` 和 `vse64.v`。
- Phase 080 post-integration board repeated（接入后板卡重复采集）显示 public count/select/getDistances 的 median speedup 分别为 4.1729x、3.3023x 和 2.5883x，Evidence Doctor 为 Errors=0、Warnings=0、Suggestions=0。
- Phase 100 把 `getDistancesToModelRVV` 的 penalty 写回改成 RVV mask / merge（掩码 / 合并）和 double 向量 store（双精度向量写回）；接入后的 public getDistances 5-run board speedup min / median / max 为 3.4122x / 3.6879x / 3.7150x，Evidence Doctor 为 Errors=0、Warnings=0、Suggestions=0。Phase 080 getDistances 数据现在只作为 historical baseline（历史基线），不作为当前源码性能 truth。
- 用户本轮明确说明“板卡上的测试结果如果显示有收益即可采纳，接入后也需要测试”，因此当前 production patch 以 Phase 080 接入后板卡数据作为采纳依据，并创建正式 `doc-rvv/sample_consensus/sac_model_stick-RVV.zh.md`。

Phase 000 / 020 / 040 的诊断结论现在是历史输入：

- Phase 000 已建立 `countWithinDistance` 的测试专用 RVV candidate 和证据链。candidate 在 5-run board repeated performance（板卡重复性能）中达到 median 4.1767x、min 4.1429x，Evidence Doctor（证据体检）为 Errors=0、Warnings=1、Suggestions=1；Warning 只落在未接 RVV 的 public entry 弱对照行。
- Phase 020 已建立 `selectWithinDistance` 的测试专用 RVV `vcompress` candidate 和证据链。candidate 在 5-run board repeated performance 中达到 median 3.4432x、min 3.4208x，Evidence Doctor 为 Errors=0、Warnings=1、Suggestions=1；Warning 只落在未接 RVV 的 public entry 弱对照行。
- Phase 040 已建立 `getDistancesToModel` 的测试专用 RVV `vfsqrt` candidate 和证据链。candidate 在 5-run board repeated performance 中达到 median 2.5553x、min 2.5416x，Evidence Doctor 为 Errors=1、Warnings=0、Suggestions=0；唯一 Error 落在未接 RVV 的 public `getDistancesToModel` negative cross-check（负向交叉检查）行，candidate 行没有 issue。

## 证据链

### Production direct 证据

| 证据层 | 路径 / 命令 | 证明内容 | 不能证明什么 |
| --- | --- | --- | --- |
| correctness | `make -C test-rvv/sample_consensus/sac_model_stick run_test_compare` | Std/RVV 两个构建各 11 个 gtest 通过；candidate 回归、public production direct 输出语义和代表点型 correctness 都受保护。 | 不证明其它点型的独立性能。 |
| asm attribution | `make -C test-rvv/sample_consensus/sac_model_stick clean_bench_rvv && make -C test-rvv/sample_consensus/sac_model_stick check_production_asm` | `countWithinDistanceRVV`、`selectWithinDistanceRVV`、`getDistancesToModelRVV` 均有 RVV 指令归属；getDistances 还要求源码和 asm 都呈现向量 double 写回形态。 | 不证明上游 RANSAC 总耗时。 |
| Phase 080 production board performance | `doc/phases/080-stick-production-integration/production-repeated-evidence-manifest.json` | public count/select/getDistances 的 speedup min / median / max 分别为 4.1331x / 4.1729x / 4.1756x、3.2085x / 3.3023x / 3.3860x、2.5722x / 2.5883x / 2.7872x。 | getDistances 数字属于旧 staged writeback 实现的历史基线；不代表 Phase 100 当前源码。 |
| Phase 100 getDistances board performance | `doc/phases/100-stick-getdistances-vector-writeback/vector-writeback-evidence-manifest.json` | public getDistances vector writeback 的 speedup min / median / max 为 3.4122x / 3.6879x / 3.7150x。 | 只覆盖 `PointXYZ` direct indexed board case；旧/新 RVV 差值不是同轮严格 A/B。 |
| Evidence Doctor | `doc/phases/080-stick-production-integration/production-repeated-evidence-doctor.md`、`.json`；`doc/phases/100-stick-getdistances-vector-writeback/vector-writeback-evidence-doctor.md`、`.json` | Phase 080 和 Phase 100 均为 Errors=0、Warnings=0、Suggestions=0。 | 不替代 reviewer 对源码维护性的审查。 |
| registry | `log/evidence_registry.json` | Phase 080 和 Phase 100 summary evidence 都已登记；Phase 100 run label 为 `stick-phase100-vector-writeback-board`。 | raw board logs 默认不是提交证据。 |

### 历史诊断证据

| 证据层 | 路径 / 命令 | 证明内容 | 不能证明什么 |
| --- | --- | --- | --- |
| correctness | `make -C test-rvv/sample_consensus/sac_model_stick run_test_compare` | Phase 000 / 020 / 040 时 Std/RVV 两个构建的 6 个 gtest 均通过；count candidate 复刻 `nr_i <= nr_o ? 0 : nr_i - nr_o`，select candidate 复刻保序 `inliers` 和 `error_sqr_dists_`，getDistances candidate 复刻方向系数、dense 输出和 penalty。 | 当前采纳结论不再依赖这些 diagnostic 数据。 |
| asm attribution | `make -C test-rvv/sample_consensus/sac_model_stick dump_bench_rvv`，`build/asm/riscv/bench_sac_model_stick_rvv.full.asm` | `countWithinDistanceCandidateRVV` 中可见 FMA、mask compare、`vmandn` 和两次 `vcpop.m`；`selectWithinDistanceCandidateRVV` 中可见 FMA、mask、`vcpop.m` 和两处 `vcompress.vm`；`getDistancesToModelCandidateRVV` 中可见 indexed load、FMA、`vfsqrt.v` 和 staged store。 | 不证明生产符号命中。 |
| count board performance | `doc/phases/000-stick-count-diagnostic/repeated-evidence-manifest.json` | count diagnostic candidate 的 speedup min / median / max 为 4.1429x / 4.1767x / 4.2169x。 | 不证明真实生产入口收益；public 行约 1.0083x 只是弱交叉检查。 |
| select board performance | `doc/phases/020-stick-select-diagnostic/repeated-evidence-manifest.json` | select diagnostic candidate 的 speedup min / median / max 为 3.4208x / 3.4432x / 3.4761x。 | 不证明真实生产入口收益；public 行 median 1.0040x 只是弱交叉检查。 |
| getDistances board performance | `doc/phases/040-stick-getdistances-diagnostic/repeated-evidence-manifest.json` | getDistances diagnostic candidate 的 speedup min / median / max 为 2.5416x / 2.5553x / 2.7849x。 | 不证明真实生产入口收益；public 行 median 0.9359x 是未接 RVV 的 negative cross-check。 |
| Evidence Doctor | `doc/phases/000-stick-count-diagnostic/repeated-evidence-doctor.md`、`doc/phases/020-stick-select-diagnostic/repeated-evidence-doctor.md`、`doc/phases/040-stick-getdistances-diagnostic/repeated-evidence-doctor.md` | Phase 000 / 020 为 Errors=0，Warnings=1，Suggestions=1；Warnings 已降级解释。Phase 040 为 Errors=1，Warnings=0，Suggestions=0，唯一 Error 已降级到 public negative cross-check。 | 不替代 Phase 080 production direct 证据。 |
| registry | `log/evidence_registry.json` | Phase 000、020 和 040 的 manifest、doctor md、doctor json 已登记，run label 分别为 `stick-phase000-repeated-board`、`stick-phase020-repeated-board` 和 `stick-phase040-repeated-board`。 | raw board logs 默认不是提交证据。 |

诊断证据不能替代 production evidence（生产证据）。当前真实采纳依据是 Phase 080 的 production patch、production direct correctness、fallback、反汇编、接入后板卡 repeated 和 Evidence Doctor。

## 文档归属矩阵

| 信息类型 | 主归属 | 当前状态 |
| --- | --- | --- |
| 函数级评估、Traceability Map、诊断证据链和生产证据链 | 本文档 | 已更新为 Phase 080 production-adopted 当前证据。 |
| 阶段计划和结果 | `doc/phases/000-stick-count-diagnostic/`、`010`、`020`、`030`、`040`、`050`、`060`、`070`、`080`、`090`、`100-stick-getdistances-vector-writeback/` | Phase 100 result 已记录 getDistances vector writeback 的源码、正确性、asm、板卡、Evidence Doctor 和 registry。 |
| 候选搜索空间和恢复队列 | `doc/optimization-roadmap.zh.md` | 三条 production probe 和 Phase 100 getDistances vector writeback 均已采纳；下一可选范围是 dedicated point-type board performance。 |
| 优化矩阵 | `doc/phases/optimization-matrix.zh.md` | 已把 Phase 100 production public getDistances evidence 置为 adopted。 |
| production 长期主题文档 | `doc-rvv/sample_consensus/sac_model_stick-RVV.zh.md` | 已刷新为 Phase 100 当前 getDistances 实现和接入后板卡数据。 |

## doc_suite_role_inventory

| role | 状态 | 路径 / 证据 | 说明 |
| --- | --- | --- | --- |
| topic_navigation | standalone | `README.zh.md` | 入口导航、常用命令、证据白名单和当前边界。 |
| testing_overview | standalone | `doc/testing-overview.zh.md` | Phase 060 拆出测试入口分类和 target 粒度审计，Phase 070 补齐 getDistances alias。 |
| correctness_tests | standalone | `doc/correctness-tests.zh.md` | 当前说明 11 个 gtest，包含历史 candidate 回归、public production direct 语义和代表点型 correctness。 |
| benchmark_and_evidence | standalone | `doc/benchmark-and-evidence.zh.md` | Phase 060 拆出 bench、board、manifest、doctor 和 registry 说明。 |
| optimization_evidence | standalone | `doc/optimization-evidence.zh.md` | Phase 060 拆出 candidate family 到证据和决策的索引。 |
| optimization_roadmap | standalone | `doc/optimization-roadmap.zh.md` | 保存跨阶段候选搜索空间和默认恢复队列。 |
| test_support_code_map | standalone | `doc/test-support-code-map.zh.md` | Phase 060 拆出测试支撑代码地图。 |
| phase_index | standalone | `doc/phases/README.zh.md` | 保存 phase suite、Phase 100 和 next phase default。 |
| evaluation_closeout | standalone | `doc/sac_model_stick-evaluation.zh.md` | 保存函数级评估、诊断证据链、production 证据链和后续范围。 |
| production_topic_doc | standalone | `doc-rvv/sample_consensus/sac_model_stick-RVV.zh.md` | Phase 080 接入后板卡证据支持当前生产采纳。 |

## 后续路径

Phase 100 当前范围已闭合，当前 topic 状态为 `production-adopted`。Phase 090 已补 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 和 `PointXYZRGBNormal` 的代表性 correctness。剩余有价值方向是 dedicated point-type board performance（专门点型板卡性能），需要新 bench label、板卡预算和 Evidence Doctor；它不属于 Phase 080 / 090 / 100 已证明范围。
