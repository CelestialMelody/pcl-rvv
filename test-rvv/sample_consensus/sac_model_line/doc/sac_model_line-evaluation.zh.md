# sac_model_line 函数级评估

## 范围和目标源码

目标源码是 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp`。当前已评估并接入 `countWithinDistance`、`selectWithinDistance` 和 `getDistancesToModel` 的 direct indexed `indices_` 主循环。`getDistancesToModel` 共用点到直线距离公式；Phase 020 证明 RVV 平方距离后回到标量 sqrt / dense store（连续写回）的形状退化，Phase 030 证明把 sqrt 移入 RVV `vfsqrt`（RVV 向量平方根）后恢复正向收益。Phase 040 完成接入后 production direct（真实生产入口直连）证据，Phase 050 把 `getDistancesToModelRVV` 的写回改成 `vfwcvt + vse64` direct double store（直接 double 写回）并复测通过，Phase 060 又把 `selectWithinDistanceRVV` 的 compressed error double store（压缩误差 double 写回）改成 `vfwcvt + vse64`。Phase 070 尝试 identity-index strided load（恒等索引跨步加载）后，strict RVV-vs-RVV A/B（同一生产边界内 RVV 实现族严格对比）没有满足三入口采纳条件，候选已拒绝并回退。当前窄范围按本轮采纳条件写成 adopted production behavior（已采纳生产行为），当前生产加载族保持 gather-only（只使用离散加载）。

## 函数族作用速览

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | 与主流程关系 | RVV 判断 |
| --- | --- | --- | --- | --- |
| `getDistancesToModel` | 为每个 index 写出点到直线距离。 | 读 `input_` / `indices_` / model coefficients，写 `distances`。 | 每点 `cross3` 后还要 `sqrt` 和 double store。 | Phase 060 production direct positive-stable；当前窄范围已采纳。 |
| `selectWithinDistance` | 收集阈值内 index，并记录平方误差。 | 写 `inliers` 和 `error_sqr_dists_`。 | 距离公式与 count 相同，但需要保序写回。 | Phase 060 production direct positive-stable；当前窄范围已采纳。 |
| `countWithinDistance` | 统计阈值内点数。 | 只返回 count，不写输出容器。 | 最小输出语义，适合作为第一 RVV 目标。 | Phase 060 production direct positive-stable；当前窄范围已采纳。 |

## 标量流程

`countWithinDistance` 先检查 `isModelValid`。随后计算 `threshold * threshold`，把 model coefficients 的前三项作为 line point，后三项作为 line direction，并在循环外归一化 direction。主循环按 `indices_` 逐个读取点，计算 `(line_pt - point).cross3(line_dir).squaredNorm()`，再用严格小于 `sqr_threshold` 的判断累加 inlier 数。`selectWithinDistance` 复用同一平方距离判断，并保序写回 `inliers` 和 `error_sqr_dists_`。`getDistancesToModel` 复用同一距离公式，但每个输出元素写的是平方根后的 double distance。

## RVV 诊断设计

测试专用 `SampleConsensusModelLineDiagnostic<PointT>` 继承 production class（生产类），复用真实 `input_`、`indices_` 和 `countWithinDistance` 公开入口。`countWithinDistanceCandidate` 在 `__RVV10__` 且 `RVVXYZAoSFloatLayout<PointT>` 成立时进入 RVV path（RVV 路径）；其它情况使用同构标量 fallback（回退路径）。Phase 040 后，production class 也有受同类 gate 保护的 RVV helper：`countWithinDistanceRVV`、`selectWithinDistanceRVV` 和 `getDistancesToModelRVV`。公开入口负责 dispatch（分流逻辑）；非覆盖构建、点型、index 类型或点云规模回到 Standard helper。

RVV path 用 indexed gather（按索引离散加载）读取 xyz，按 VL chunk（可变向量长度分块）展开三维叉乘，再用 FMA（融合乘加）形成 squared distance。`countWithinDistanceCandidate` 用 `vmflt` 生成阈值 mask（掩码），再用 `vcpop` 做 mask popcount（掩码计数）。`selectWithinDistanceCandidate` 复用同一公式，用 `vcompress`（向量压缩）保序写回 inlier index 和平方误差暂存。`getDistancesToModelCandidate` 只把平方距离放进 RVV，sqrt 和 double store 仍由标量后处理完成；该形状在 Phase 020 退化。`getDistancesToModelVFSqrtCandidate` 在 RVV chunk 内执行 `vfsqrt`，再把 float distance 转成 double 写回，Phase 030 证明它在 diagnostic boundary 下正向；Phase 040 证明同一实现族在 production public entry 下仍正向。Phase 050 把 `getDistancesToModelRVV` 的写回改成 RVV `vfwcvt + vse64`，Phase 060 又把 `selectWithinDistanceRVV` 的压缩平方误差写回改成 RVV `vfwcvt + vse64`，避免对应 scratch `vse32` 和标量 lane loop。

Phase 070 的 identity-index strided load 只改变 load family（加载实现族），不改变距离公式和写回形态。它用同一 public overload 下的 gather-only RVV baseline 与 identity-strided RVV candidate 做 production-detail A/B（生产细节对比）；identity 输入只给 count/getDistances 弱正向，select 退化，shuffled 控制组也出现退化频率 Error。因此该候选不进入 production，当前源码保持 indexed gather。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `SampleConsensusModelLine::countWithinDistance` | production public entry | 接入后公开入口；RVV gate 命中时调用 `countWithinDistanceRVV`。 | PCL sample consensus 调用方。 | RVV helper 或 Standard helper。 | production direct correctness / board。 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp` |
| `SampleConsensusModelLine::selectWithinDistance` | production public entry | 接入后公开入口；RVV gate 命中时调用 `selectWithinDistanceRVV`。 | PCL sample consensus 调用方。 | RVV helper 或 Standard helper，写 `inliers` 和 `error_sqr_dists_`。 | production direct correctness / board。 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp` |
| `SampleConsensusModelLine::getDistancesToModel` | production public entry | 接入后公开入口；RVV gate 命中时调用 `getDistancesToModelRVV`。 | PCL sample consensus 调用方。 | RVV helper 或 Standard helper，写 dense distance output。 | production direct correctness / board。 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp` |
| `countWithinDistanceRVV` / `selectWithinDistanceRVV` / `getDistancesToModelRVV` | production helper | `PointXYZ` 风格 float xyz AoS direct indexed 主循环。 | public entry dispatch。 | RVV indexed load、cross3 公式、mask/count、`vcompress` 或 `vfsqrt`。 | production asm attribution。 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp` |
| `SampleConsensusModelLineDiagnostic::countWithinDistanceCandidate` | production-shaped diagnostic | 测试专用候选入口。 | gtest 和 bench。 | RVV helper 或 scalar fallback。 | correctness / board diagnostic。 | `include/impl/sac_model_line_diagnostic.hpp` |
| `SampleConsensusModelLineDiagnostic::selectWithinDistanceCandidate` | production-shaped diagnostic | 测试专用 select 候选入口。 | gtest 和 bench。 | RVV helper 或 scalar fallback。 | correctness / board diagnostic。 | `include/impl/sac_model_line_diagnostic.hpp` |
| `SampleConsensusModelLineDiagnostic::getDistancesToModelCandidate` | production-shaped diagnostic | 测试专用 scalar-sqrt 候选入口。 | gtest 和 bench。 | RVV 平方距离 helper 或 scalar fallback。 | correctness / negative board diagnostic。 | `include/impl/sac_model_line_diagnostic.hpp` |
| `SampleConsensusModelLineDiagnostic::getDistancesToModelVFSqrtCandidate` | production-shaped diagnostic | 测试专用 vfsqrt 候选入口。 | gtest 和 bench。 | RVV vfsqrt helper 或 scalar fallback。 | correctness / board diagnostic。 | `include/impl/sac_model_line_diagnostic.hpp` |
| `test_sac_model_line.cpp` | correctness test | 乱序 indices、阈值边界、bench-scale select 和 getDistances 对拍。 | `run_test_compare`。 | public entry 与 candidate。 | QEMU correctness。 | `src/test_sac_model_line.cpp` |
| `bench_sac_model_line.cpp` | bench wrapper | 计时 public count/select/getDistances 和 diagnostic candidates。 | board / bench target。 | topic-local manifest script。 | board performance diagnostic。 | `src/bench_sac_model_line.cpp` |
| `generate_line_board_evidence_manifest.py` | analysis script | 把 repeated board logs 转成 manifest。 | `generate_*_board_evidence_manifest`。 | Evidence Doctor。 | summary evidence。 | `script/generate_line_board_evidence_manifest.py` |
| `repeated-evidence-manifest.json` | evidence output summary | 保存 5-run repeated B/A 和 metadata。 | manifest script。 | Evidence Doctor / docs。 | current board summary。 | `doc/phases/000-line-count-diagnostic/repeated-evidence-manifest.json` |
| `repeated-evidence-doctor.md` | evidence output summary | 保存 Evidence Doctor 结果。 | `run_repeated_board_evidence_doctor`。 | phase result / evaluation。 | doctor gate。 | `doc/phases/000-line-count-diagnostic/repeated-evidence-doctor.md` |
| `010-line-select-diagnostic/repeated-evidence-manifest.json` | evidence output summary | 保存 select 5-run repeated B/A 和 metadata。 | manifest script。 | Evidence Doctor / docs。 | current board summary。 | `doc/phases/010-line-select-diagnostic/repeated-evidence-manifest.json` |
| `010-line-select-diagnostic/repeated-evidence-doctor.md` | evidence output summary | 保存 select Evidence Doctor 结果。 | `run_repeated_board_select_evidence_doctor`。 | phase result / evaluation。 | doctor gate。 | `doc/phases/010-line-select-diagnostic/repeated-evidence-doctor.md` |
| `020-line-get-distances-diagnostic/repeated-evidence-manifest.json` | evidence output summary | 保存 scalar-sqrt 5-run repeated B/A 和 metadata。 | manifest script。 | Evidence Doctor / docs。 | negative board summary。 | `doc/phases/020-line-get-distances-diagnostic/repeated-evidence-manifest.json` |
| `020-line-get-distances-diagnostic/repeated-evidence-doctor.md` | evidence output summary | 保存 scalar-sqrt Evidence Doctor 结果。 | `run_repeated_board_get_distances_evidence_doctor`。 | phase result / evaluation。 | doctor gate。 | `doc/phases/020-line-get-distances-diagnostic/repeated-evidence-doctor.md` |
| `030-line-get-distances-vfsqrt-diagnostic/repeated-evidence-manifest.json` | evidence output summary | 保存 vfsqrt 5-run repeated B/A 和 metadata。 | manifest script。 | Evidence Doctor / docs。 | current board summary。 | `doc/phases/030-line-get-distances-vfsqrt-diagnostic/repeated-evidence-manifest.json` |
| `030-line-get-distances-vfsqrt-diagnostic/repeated-evidence-doctor.md` | evidence output summary | 保存 vfsqrt Evidence Doctor 结果。 | `run_repeated_board_get_distances_vfsqrt_evidence_doctor`。 | phase result / evaluation。 | doctor gate。 | `doc/phases/030-line-get-distances-vfsqrt-diagnostic/repeated-evidence-doctor.md` |
| `040-line-production-integration/production-repeated-evidence-manifest.json` | evidence output summary | 保存接入后 public count/select/getDistances 5-run repeated B/A 和 metadata。 | manifest script。 | Evidence Doctor / docs。 | production direct board summary。 | `doc/phases/040-line-production-integration/production-repeated-evidence-manifest.json` |
| `040-line-production-integration/production-repeated-evidence-doctor.md` | evidence output summary | 保存接入后 production direct Evidence Doctor 结果。 | `run_repeated_board_production_evidence_doctor`。 | phase result / evaluation。 | doctor gate。 | `doc/phases/040-line-production-integration/production-repeated-evidence-doctor.md` |
| `050-line-get-distances-vse64-store/production-repeated-evidence-manifest.json` | evidence output summary | 保存 Phase 050 接入后 public count/select/getDistances 5-run repeated B/A 和 metadata。 | manifest script。 | Evidence Doctor / docs。 | 当前 production direct board summary。 | `doc/phases/050-line-get-distances-vse64-store/production-repeated-evidence-manifest.json` |
| `050-line-get-distances-vse64-store/production-repeated-evidence-doctor.md` | evidence output summary | 保存 Phase 050 production direct Evidence Doctor 结果。 | `run_repeated_board_production_vse64_evidence_doctor`。 | phase result / evaluation。 | doctor gate。 | `doc/phases/050-line-get-distances-vse64-store/production-repeated-evidence-doctor.md` |
| `060-line-select-vse64-compressed-store/production-repeated-evidence-manifest.json` | evidence output summary | 保存 Phase 060 接入后 public count/select/getDistances 5-run repeated B/A 和 metadata。 | manifest script。 | Evidence Doctor / docs。 | 当前 production direct board summary。 | `doc/phases/060-line-select-vse64-compressed-store/production-repeated-evidence-manifest.json` |
| `060-line-select-vse64-compressed-store/production-repeated-evidence-doctor.md` | evidence output summary | 保存 Phase 060 production direct Evidence Doctor 结果。 | `run_repeated_board_production_select_vse64_evidence_doctor`。 | phase result / evaluation。 | doctor gate。 | `doc/phases/060-line-select-vse64-compressed-store/production-repeated-evidence-doctor.md` |
| `070-line-identity-index-strided-load/identity-repeated-evidence-manifest.json` | evidence output summary | 保存 identity 输入下 gather-only RVV baseline / identity-strided RVV candidate 的 5-run A/B。 | manifest script。 | Evidence Doctor / docs。 | rejected family selection evidence。 | `doc/phases/070-line-identity-index-strided-load/identity-repeated-evidence-manifest.json` |
| `070-line-identity-index-strided-load/identity-repeated-evidence-doctor.md` | evidence output summary | 保存 identity 输入 Evidence Doctor 结果。 | `record_identity_board_evidence_state`。 | phase result / evaluation。 | doctor gate。 | `doc/phases/070-line-identity-index-strided-load/identity-repeated-evidence-doctor.md` |
| `070-line-identity-index-strided-load/identity-shuffled-repeated-evidence-manifest.json` | evidence output summary | 保存 shuffled 控制组下 gather-only RVV baseline / identity-strided RVV candidate 的 5-run A/B。 | manifest script。 | Evidence Doctor / docs。 | rejected family selection evidence。 | `doc/phases/070-line-identity-index-strided-load/identity-shuffled-repeated-evidence-manifest.json` |
| `070-line-identity-index-strided-load/identity-shuffled-repeated-evidence-doctor.md` | evidence output summary | 保存 shuffled 控制组 Evidence Doctor 结果。 | `record_identity_shuffled_board_evidence_state`。 | phase result / evaluation。 | doctor gate。 | `doc/phases/070-line-identity-index-strided-load/identity-shuffled-repeated-evidence-doctor.md` |

## 实现方式审计

| 实现维度 | 当前状态 | 证据 | 边界 / 恢复条件 |
| --- | --- | --- | --- |
| dispatch / fallback | production patch 已实现并采纳当前窄范围。 | `run_test_compare` Std/RVV 9/9；Phase 060 production direct board positive-stable。 | 更多点型和 layout 需要后续 phase。 |
| layout / traits gate | `RVVXYZAoSFloatLayout<PointT>` / signed 32-bit `pcl::index_t` / u32 byte offset gate。 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp`。 | `PointXYZI`、RGB/RGBA、normal 和自定义点型未做 dedicated production direct bench。 |
| row source policy | direct indexed `indices_`。 | 测试使用乱序 indices；bench 使用 adjacent-pair shuffle；Phase 070 另测 identity / shuffled RVV-vs-RVV A/B 并拒绝 identity-strided load。 | 其它入口未覆盖。 |
| formula / FMA / sqrt / store | RVV 展开 cross3 + squaredNorm；production getDistances 使用 `vfsqrt` 后 `vfwcvt + vse64` 直接写 double；production select 对压缩平方误差也用 `vfwcvt + vse64` 直接写 double。 | production helper asm 归属通过；Phase 060 doctor 0/0/0。 | 阈值极近样本和更多点型仍需扩展 phase。 |
| production scope | `adopted_production_behavior`。 | public count median `4.8068x`；public select median `3.2460x`；public getDistances median `4.0460x`；production doctor 0/0/0。 | 长期 `doc-rvv` 使用 Phase 060 接入后板卡数据。 |

## 验证结果

| 证据 | 命令 / 路径 | 结果 | 不能证明什么 |
| --- | --- | --- | --- |
| QEMU correctness | `make -C test-rvv/sample_consensus/sac_model_line run_test_compare` | Phase 070 回退后 Std/RVV 各 9 个 gtest 通过。 | 不证明目标硬件性能。 |
| asm attribution | `make -C test-rvv/sample_consensus/sac_model_line dump_bench_rvv` | count candidate symbol 内出现 indexed load、FMA、mask compare 和 popcount；select candidate symbol 内出现 indexed load、FMA、mask compare、`vcompress` 和 store；getDistances vfsqrt symbol 内出现 indexed load、FMA、`vfsqrt.v` 和 store。 | 不证明 production 入口已分流。 |
| board repeated | `SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_evidence` | diagnostic candidate median/min/max `4.4569x / 4.4350x / 4.4920x`。 | 不证明 production direct performance。 |
| board repeated select | `SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_select_evidence` | diagnostic candidate median/min/max `3.1010x / 3.0622x / 3.1434x`。 | 不证明 production direct performance。 |
| board repeated getDistances scalar-sqrt | `SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_get_distances_evidence` | diagnostic candidate median/min/max `0.9340x / 0.9289x / 0.9732x`。 | 只拒绝当前 scalar-sqrt diagnostic boundary。 |
| board repeated getDistances vfsqrt | `SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_get_distances_vfsqrt_evidence` | diagnostic candidate median/min/max `3.5208x / 3.3746x / 3.5378x`。 | 不证明 production direct performance。 |
| production direct board repeated, Phase 040 baseline | `SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_production_evidence` | public count `4.8178x / 4.7916x / 4.8321x`；public select `3.0744x / 3.0420x / 3.1055x`；public getDistances `3.4036x / 3.3409x / 3.5229x`。 | 历史 production baseline；getDistances 仍含 scratch + 标量 lane store。 |
| production direct board repeated, Phase 050 getDistances store baseline | `SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_production_vse64_evidence` | public count `4.7793x / 4.7463x / 4.8269x`；public select `3.1014x / 3.0318x / 3.2815x`；public getDistances `4.2237x / 4.0338x / 4.2728x`。 | 历史 production baseline；Phase 060 后 select 写回形态已改变。 |
| production direct board repeated, Phase 060 current | `SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_production_select_vse64_evidence` | public count `4.8068x / 4.6009x / 4.8607x`；public select `3.2460x / 2.9755x / 3.3620x`；public getDistances `4.0460x / 4.0334x / 4.2406x`。 | 当前 production truth；select 已去掉 compressed error scratch + 标量 lane store。 |
| Evidence Doctor | `make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_evidence_state` | Errors=0，Warnings=0，Suggestions=0。 | 环境 metadata 仍缺 taskset / governor / freq / temperature / binary hash。 |
| Evidence Doctor select | `make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_select_evidence_state` | Errors=0，Warnings=0，Suggestions=0。 | 环境 metadata 仍缺 taskset / governor / freq / temperature / binary hash。 |
| Evidence Doctor getDistances scalar-sqrt | `make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_get_distances_evidence_state` | Errors=1，Warnings=0，Suggestions=0。 | Error 表示 5/5 B/A 低于 1；已降级为该候选拒绝证据。 |
| Evidence Doctor getDistances vfsqrt | `make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_get_distances_vfsqrt_evidence_state` | Errors=0，Warnings=0，Suggestions=0。 | 环境 metadata 仍缺 taskset / governor / freq / temperature / binary hash。 |
| Evidence Doctor production direct, Phase 040 baseline | `make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_production_evidence_state` | Errors=0，Warnings=0，Suggestions=0。 | 历史 production baseline；环境 metadata 仍缺 taskset / governor / freq / temperature / binary hash。 |
| Evidence Doctor production direct, Phase 050 getDistances store baseline | `make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_production_vse64_evidence_state` | Errors=0，Warnings=0，Suggestions=0。 | 历史 production baseline；环境 metadata 仍缺 taskset / governor / freq / temperature / binary hash。 |
| Evidence Doctor production direct, Phase 060 current | `make -C test-rvv/sample_consensus/sac_model_line record_repeated_board_production_select_vse64_evidence_state` | Errors=0，Warnings=0，Suggestions=0。 | 当前 production truth；环境 metadata 仍缺 taskset / governor / freq / temperature / binary hash。 |
| identity RVV-vs-RVV A/B, Phase 070 | `SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_identity_repeated_board_evidence` | identity count/getDistances 弱正向，select median `0.9967x` 且 4/5 退化。 | 只回答 identity-strided load 是否优于当前 gather-only RVV family。 |
| shuffled RVV-vs-RVV A/B, Phase 070 | `SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_identity_shuffled_repeated_board_evidence` | shuffled getDistances median `0.9959x` 且 3/5 退化；doctor Errors=3。 | 控制组说明候选不稳定，不作为 production performance truth。 |

## 诊断证据链

Phase 000/010/030 证明 `PointXYZ + direct indexed count/select/getDistances vfsqrt` 的测试专用 RVV helper 与公开标量入口一致，并在 board 上有稳定正向性能。Phase 020 的 scalar-sqrt 负向证据说明该实现形状不值得进入 production probe。Phase 040 已补齐 production evidence（生产证据）：接入后公开入口在板卡上仍为 positive-stable。Phase 050 证明 `getDistancesToModelRVV` 的 direct double store 写回形态在同一 production boundary 下继续提升 getDistances；Phase 060 证明 `selectWithinDistanceRVV` 的 compressed error direct double store 在同一 production boundary 下可小幅改善 select。Phase 070 证明 identity-index strided load 没有比当前 gather-only RVV family 更值得保留，当前生产实现保持 Phase 060 形态。

## Production 接入判断

EvidenceDecision：`adopted_production_behavior`。count、select 和 getDistances vfsqrt/vse64 均已在真实 production dispatch 下通过 QEMU correctness、production asm attribution 和 5-run board repeated；当前证据支持当前窄范围接入。`doc-rvv/sample_consensus/sac_model_line-RVV.zh.md` 是长期生产主题文档，性能数据采用 Phase 060 接入后的板卡数据。Phase 070 的 identity-index strided load 为 `rejected with evidence`，不改变长期文档中的当前生产性能表。

## doc_suite_role_inventory

| role | 状态 | 证据 |
| --- | --- | --- |
| topic_navigation | `standalone:README.zh.md` | 已提供阅读路径、命令和证据边界。 |
| testing_overview | `standalone:doc/testing-overview.zh.md` | 已列 target 粒度和覆盖范围。 |
| correctness_tests | `standalone:doc/correctness-tests.zh.md` | 已解释 gtest 输入和断言。 |
| benchmark_and_evidence | `standalone:doc/benchmark-and-evidence.zh.md` | 已解释 repeated board、manifest、doctor 和 registry。 |
| optimization_evidence | `standalone:doc/optimization-evidence.zh.md` | 已记录候选取舍。 |
| optimization_roadmap | `standalone:doc/optimization-roadmap.zh.md` | 已记录后续 phase。 |
| test_support_code_map | `standalone:doc/test-support-code-map.zh.md` | 已定位 helper、src、script 和 output。 |
| phase_index | `standalone:doc/phases/README.zh.md` | 已提供 phase 恢复入口。 |
| evaluation_diagnostic | `standalone:doc/sac_model_line-evaluation.zh.md` | 本文件。 |
| production_topic_doc | `standalone:doc-rvv/sample_consensus/sac_model_line-RVV.zh.md` | Phase 060 已有 production direct 证据，且长期文档使用接入后板卡数据。 |
