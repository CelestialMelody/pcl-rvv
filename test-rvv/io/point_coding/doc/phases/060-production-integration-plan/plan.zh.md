# Phase 060 Production Integration Plan

## 阶段意图和边界

本阶段进入 production integration loop（生产接入闭环）PI1-PI5，只做一个有界生产探针：
在 `io/include/pcl/compression/point_coding.h` 中为
`pcl::octree::PointCoding<pcl::PointXYZ>::decodePoints` 增加 RVV dispatch（RVV 分流）。

本阶段要证明：

- 真实 production `decodePoints` 在 RVV build 下能保持现有标量语义。
- `PointXYZ` exact gate（精确点型门控）命中时，真实 production direct（生产直连）bench 在板卡上相比同一公开生产入口的 Std build 有收益。
- 非 RVV build、非 `PointXYZ` 模板实例和本阶段未覆盖点型自然走标量路径。

本阶段不证明：

- `PointCoding<PointT>` 的泛型点类型 RVV gate 已完成。
- encode path 可以接入 production。
- 完整 `OctreePointCloudCompression` public end-to-end（公开端到端）吞吐已加速。
- production patch 在计划阶段不能自动视为 adopted（已采纳）。PI5 必须暂停，等待用户确认保留或回滚；本阶段 result 已回填用户确认采纳。

## 当前状态清单

| area | 当前事实 | 路径 |
| --- | --- | --- |
| production source | `point_coding.h` 尚无本 topic 改动；`decodePoints` 是标量循环，iterator 每点消耗 3 字节。 | `io/include/pcl/compression/point_coding.h` |
| component decode | decode-only 10-run 板卡 median 全部大于 1，Evidence Doctor 为 `Errors=0，Warnings=1`。 | `040-decode-stability-profile/result.zh.md` |
| production-shaped decode | object-state context 10-run median 1.21x-1.40x，`Errors=0，Warnings=7`。 | `050-production-shaped-context-scout/result.zh.md` |
| multileaf context | multi-leaf 10-run median 1.18x-1.28x，`Errors=0，Warnings=3`。 | `055-full-octree-context-scout/result.zh.md` |
| public feasibility | `OctreePointCloudCompression<PointXYZ>` public roundtrip smoke 可构造，Std/RVV correctness 各 6 个 gtest 通过。 | `056-public-octree-roundtrip-feasibility/result.zh.md` |
| production docs | plan 阶段尚无 adopted production behavior，不创建正式 `doc-rvv/io/point_coding-RVV.zh.md`；用户确认采纳后已在 closeout 创建。 | `optimization-matrix.zh.md`、`doc-rvv/io/point_coding-RVV.zh.md` |

## validated_scope

| dimension | 本阶段覆盖 |
| --- | --- |
| production entry | `PointCoding<PointXYZ>::decodePoints` |
| row source | point coder diff byte stream -> contiguous output cloud segment |
| point type / Scalar / layout | exact `pcl::PointXYZ` / `float x/y/z` output / AoS |
| data source | `pointDiffDataVectorIterator_` 指向的连续 diff byte 三元组 |
| sizes | production-direct bench `256/1024/4096/16384`，必要时保留小规模 smoke |
| performance evidence | 板卡 10-run repeated summary + Evidence Doctor |

## unvalidated_scope

- `PointXYZI`、`PointXYZRGB/RGBA`、normal 复合点型和用户自定义点类型。
- traits-gated PointXYZ-like 泛型生产路径。
- encode `f32` fast path、`f64` exact quantize 和 boundary lane fallback。
- 完整 octree traversal / entropy decode / public stream end-to-end 性能。

## point_type_expansion_queue

| candidate | status | resume condition |
| --- | --- | --- |
| PointXYZ-like traits gate | phase_deferred + unblocked after PI5 decision | 若 `PointXYZ` production-direct 证据支持保留，下一 phase 读取公共 RVV traits/wrapper，补 fallback correctness、bench、asm、board 和 Evidence Doctor。 |
| non-PointXYZ fallback test | planned in this phase | 用 `PointXYZI` 或等价点型验证 RVV build 下非 exact gate 仍保持标量语义。 |
| public end-to-end timing | deferred | 只有 production direct positive 且用户确认保留后，再决定是否测完整 public stream。 |

## 候选族与实现假设

| candidate family | hypothesis | risk / unknown | evidence needed |
| --- | --- | --- | --- |
| production decode RVV v0 | 复用 Phase 050/055 的 `vlse8` stride load + `vsse32` strided store，可减少每点 3 字节 decode 和坐标写回成本。 | 模板头文件编译边界、iterator 更新、PointXYZ exact gate 维护成本。 | correctness、asm、production-direct board repeated、doctor。 |
| exact PointXYZ gate | 先把 production probe 限定在已测点型，避免把诊断证据外推为泛型。 | 覆盖范围窄，不是最终泛型实现。 | fallback test 和 Handoff 明确未覆盖点型。 |
| encode production | 不进入本阶段。 | f32/double 语义和 f64 成本已被 Phase 020/030 拒绝或阻塞。 | 不适用。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production decode RVV v0 | production point coder decode stream | exact `PointXYZ` / float output / AoS | `PointCoding<PointXYZ>::decodePoints` | `make run_test_compare` with production-direct tests | `decode_production_direct_256/1024/4096/16384` | 10-run board repeated | bench RVV asm includes production decode RVV instructions | repeated manifest -> doctor | plan target was PI5 checkpoint；result now adopted |
| non-PointXYZ fallback | same production entry | non-exact `PointXYZI` or equivalent | RVV build fallback | gtest compares production object behavior | not_applicable | not_applicable | no RVV path required | not_applicable | fallback correctness |
| production topic doc | adopted production behavior only | exact `PointXYZ` if adopted | `doc-rvv/io/point_coding-RVV.zh.md` | not_applicable | not_applicable | post-integration board data only | not_applicable | not_applicable | create only after user confirms adoption |

## 实现和测试动作

| action | artifact | command / evidence | done criteria |
| --- | --- | --- | --- |
| PI1 plan | 本文件 | manual review | plan exists before production edit |
| production-direct test/bench | `src/test_point_coding.cpp`、`src/bench_point_coding.cpp`、manifest script | pre-patch RVV build should compile/run using scalar production path | case labels and metadata exist |
| production patch | `io/include/pcl/compression/point_coding.h` | code review + build | non-RVV and non-PointXYZ fallback remain scalar |
| correctness | test-rvv compare | `make run_test_compare` | Std/RVV gtests pass |
| QEMU smoke | narrow bench only | `make run_qemu_bench_smoke BENCH_ARGS='--iterations 1 --warmup-iterations 0 --case-filter decode_production_direct_1024'` | log shape only |
| asm | bench RVV dump | `make dump_bench_rvv` + grep `vlse8.v` / `vsse32.v` / `vfcvt.f.xu.v` | RVV path attributable |
| board repeated | production direct case filter | `POINT_CODING_REPEATED_RUNS=10 POINT_CODING_REPEATED_DIR=log/board/repeated_phase060_production_decode BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter decode_production_direct_*' make collect_board_repeated run_board_repeated_evidence_doctor` | summary and doctor generated |
| PI5 stop | result + Handoff | report production diff and evidence | wait for user confirmation before adopted / rollback |

## Evidence Doctor 和 registry 规则

- manifest path: `log/board/repeated_phase060_production_decode/evidence_manifest.json`
- summary path: `log/board/repeated_phase060_production_decode/summary.md`
- doctor paths: `log/board/repeated_phase060_production_decode/evidence_doctor.md/json`
- Expected blocking threshold: `Errors=0` required for using performance evidence.
- Warnings must be explained; small degradation frequency can still support bounded production candidate if median bucket remains positive and implementation remains small.
- evidence policy（证据策略）为 summary-only；raw run logs 不默认提交。

## 板卡复跑预算和决策桶

| field | value |
| --- | --- |
| board availability | 当前会话用户确认板卡可用 |
| repeated runs | 10 |
| warmup / iterations | 3 warmup / 20 measured iterations |
| positive | all target sizes median speedup > 1.05x and no checksum mismatch |
| weak-positive | median > 1.0x with limited degradation warnings; PI5 may ask user to judge adoption |
| neutral | median around 1.0x or mixed sizes without clear benefit |
| negative | median < 1.0x for important sizes or frequent degradation |
| unstable | decision bucket changes across budget or doctor warnings dominate |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 040/050/055 是 diagnostic / production-shaped diagnostic；Phase 060 目标是 production-detail / production-direct。 |
| A/B boundary | 旧证据边界是 test helper；本阶段边界是真实 `PointCoding<PointXYZ>::decodePoints` production entry。 |
| 当前决策问题 | RVV-vs-scalar for current production decode path；不是 RVV-family-selection。 |
| diagnostic 是否可外推到 production | only as probe justification；最终取舍必须用本阶段 production-direct board data。 |
| comparison-boundary / baseline mismatch 风险 | yes；因此新增 `decode_production_direct_*` case。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes；用户已授权，且实现限定 decode-only exact `PointXYZ`。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | no；当前 production 没有既有 adopted RVV family，本阶段是 Std/RVV production direct。 |

## 阶段完成条件

- correctness、QEMU smoke、asm 和 board repeated 都完成，且 Evidence Doctor 无 Error。
- `result.zh.md` 回填生产 diff、actual scope、board bucket、Warnings 解释和 PI5 stop。
- matrix、roadmap、evaluation、phase README 和 Handoff 同步当前状态。
- 若证据支持采纳，暂停等待用户确认；确认前不创建正式 production `doc-rvv/io/point_coding-RVV.zh.md`。本阶段 result 已回填用户确认采纳并创建正式文档。
- 若证据不支持生产，暂停等待用户确认回滚；确认前不自行删除 production patch。

## 继续 / 停止条件

本阶段默认继续到 PI5。只有以下情况可提前停止：

- production patch 编译或语义出现无法同轮修复的 blocker。
- 板卡不可用或 repeated target 无法运行。
- Evidence Doctor Error 无法修复，需要人工判断。
- dirty isolation 发现 `point_coding` 相关文件有无法区分的用户改动。

PI5 是强制停止点；它不是 adopted closeout。

## 文档更新清单

- `060-production-integration-plan/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/point_coding-evaluation.zh.md`
- `doc/testing-overview.zh.md`
- `doc/correctness-tests.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/test-support-code-map.zh.md`
- 当前 Handoff packet
- `doc-rvv/io/point_coding-RVV.zh.md` 只在 PI5 通过且用户确认采纳后创建。

## roadmap 同步动作

本阶段会把 `production integration loop` 从 `blocked by authorization` 更新为 `PI5 pending` 或后续状态。当前 result 已回填为 adopted production behavior；若继续同 topic，下一默认 phase 是 PointXYZ-like traits expansion。
