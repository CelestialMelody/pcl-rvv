# Phase 070 PointXYZ-like Traits Expansion Plan

## 阶段意图和边界

Phase 060 已把 exact `PointCoding<PointXYZ>::decodePoints` 采纳为 production behavior（已采纳生产行为）。
本阶段继续同一 topic 内的下一个未阻塞方向：把 production RVV gate（生产 RVV 准入条件）从
`std::is_same_v<PointT, pcl::PointXYZ>` 扩展到 `pcl::rvv::kRVVXYZAoSPointCompatible<PointT>`
这样的 PointXYZ-like traits gate（类似 PointXYZ 的点型特征门控）。

本阶段要证明：

- 对注册了单个 `float x/y/z` 且 AoS layout（结构数组布局）可按 byte offset 写回的 `PointT`，RVV decode helper 能保持原标量语义。
- 额外字段，例如 `PointXYZI::intensity`、`PointXYZRGB/RGBA` 的 color 字段，在 decode 中保持原值不被改写。
- `PointT` 不满足 traits gate 时仍自然 fallback 到 `decodePointsStd`。
- 代表点型的 production-direct board repeated（真实生产路径直连重复板卡测试）仍有正向收益，且 Evidence Doctor 无 Error。

本阶段不证明：

- encode path 可以接 production。
- 完整 `OctreePointCloudCompression` public end-to-end（公开入口端到端）吞吐已加速。
- 所有自定义点型都已经有板卡收益；只批准 traits gate 语义和代表点型证据覆盖的范围。
- 非 xyz 字段参与算法语义；decode 只写 `x/y/z`，其它字段保持原值。

## 当前状态清单

| area | 当前事实 | 路径 |
| --- | --- | --- |
| adopted exact gate | Phase 060 exact `PointXYZ` production-direct median 为 1.22x / 1.23x / 1.18x / 1.16x，Doctor `Errors=0，Warnings=1`。 | `060-production-integration-plan/result.zh.md` |
| traits API | 公共 `pcl::rvv::kRVVXYZAoSPointCompatible<PointT>` 和 `RVVXYZAoSFloatLayout<PointT>::kX/kY/kZ` 已存在。 | `common/include/pcl/rvv_point_traits.h` |
| store wrapper | 公共 `pcl::rvv_store::strided_store3_f32m2` 可用 traits offset 和 `sizeof(PointT)` 做 AoS store。 | `common/include/pcl/rvv_point_store.h` |
| current production helper | `decodePointsRVV` 当前写死 `pcl::PointXYZ*` 和 `sizeof(pcl::PointXYZ)`。 | `io/include/pcl/compression/point_coding.h` |
| current tests | 已有 `PointXYZI` fallback 测试；Phase 070 需把它改成 traits-hit RVV correctness，并新增 color 点型代表。 | `src/test_point_coding.cpp` |

## validated_scope

| dimension | 本阶段覆盖 |
| --- | --- |
| production entry | `PointCoding<PointT>::decodePoints` |
| row source | point coder diff byte stream -> contiguous output cloud segment |
| point type / layout | `pcl::rvv::kRVVXYZAoSPointCompatible<PointT>` 为 true 的 xyz AoS 点型；代表点型先用 `PointXYZ`、`PointXYZI`、`PointXYZRGB` 或 `PointXYZRGBA` |
| scalar semantics | double reference + float resolution + final float store |
| performance evidence | production-direct board repeated for representative PointXYZ-like cases |
| fallback | 不满足 traits gate 的点型和非 RVV build 走 `decodePointsStd` |

## unvalidated_scope

- encode path。
- 完整 octree traversal / entropy decode / public stream end-to-end。
- 非 xyz AoS、非单 float xyz 字段、非 standard-layout / POD size 不匹配点型。
- 所有自定义点型的性能；traits gate 只证明布局与字段语义，性能仍以代表点型和目标硬件为证据边界。

## 候选族与实现假设

| candidate family | hypothesis | risk / unknown | evidence needed |
| --- | --- | --- | --- |
| traits-gated decode RVV | 当前 decode 只写 `x/y/z`，可以用公共 xyz AoS traits 和 store wrapper 泛化。 | PointXYZRGB/RGBA 的字段 layout、POD、alignment 和额外字段保留必须由 test 证明。 | correctness、asm、board repeated、doctor。 |
| fallback for non-compatible point type | traits gate false 时回到 `decodePointsStd`。 | 需要构造一个可编译但不满足 RVV traits 的点型，或保留非 RVV build 证据。 | fallback correctness。 |
| public octree timing | 不进入本阶段。 | 完整 tree / entropy context 可能稀释收益。 | separate phase if requested。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| traits-gated production decode RVV | production point coder decode stream | `PointXYZ`、`PointXYZI`、`PointXYZRGB/RGBA` / float xyz / AoS | `PointCoding<PointT>::decodePoints` | `make run_test_compare` with traits-hit and extra-field preservation tests | `decode_production_direct_traits_*` or explicit point-type labels | 10-run board repeated | production bench RVV asm includes f64 widen/narrow and strided/segmented stores | repeated manifest -> doctor | planned |
| non-compatible fallback | same production entry | no xyz traits or unsupported layout | RVV build fallback | gtest compares scalar behavior / preserved fields | not_applicable | not_applicable | no RVV path required | not_applicable | planned |
| doc-rvv refresh | production long-term docs | traits-gated scope if adopted | `doc-rvv/io/point_coding-RVV.zh.md` | not_applicable | not_applicable | Phase 070 board data only if adopted | not_applicable | Phase 070 doctor | plan-time action after evidence |

## 实现和测试动作

| action | artifact | command / evidence | done criteria |
| --- | --- | --- | --- |
| plan | 本文件 | manual review | plan exists before production edits |
| production patch | `io/include/pcl/compression/point_coding.h` | code review + build | dispatch uses `kRVVXYZAoSPointCompatible<PointT>` and store wrapper |
| correctness | `src/test_point_coding.cpp` | `make run_test_compare` | Std/RVV gtests pass for PointXYZ, PointXYZI and color representative |
| bench labels | `src/bench_point_coding.cpp`、manifest script | QEMU smoke + board summary | case metadata distinguishes traits production-direct roles |
| QEMU smoke | narrow bench only | `make run_qemu_bench_smoke ... --case-filter decode_production_direct_traits_1024` | log shape only |
| asm | bench RVV dump | `make dump_bench_rvv` + grep `vlse8.v` / `vfwcvt.f.xu.v` / `vfncvt.f.f.w` / store instruction | RVV path attributable |
| board repeated | traits case filter | 10-run production-direct repeated + doctor | median/min bucket supports adoption and Doctor has no Error |
| docs | phase result、matrix、roadmap、evaluation、doc-rvv、Handoff | path refresh | adopted/deferred boundary is current |

## Evidence Doctor 和 registry 规则

- summary path: `log/board/repeated_phase070_traits_decode/summary.md`
- manifest path: `log/board/repeated_phase070_traits_decode/evidence_manifest.json`
- doctor path: `log/board/repeated_phase070_traits_decode/evidence_doctor.md`
- Expected blocking threshold: `Errors=0` required for adoption.
- Warnings must keep min/median/max and explain point-type or long-tail risk.
- evidence policy（证据策略）为 summary-only；raw run logs 不默认提交。

## 板卡复跑预算和决策桶

| field | value |
| --- | --- |
| board availability | 当前会话用户确认板卡可用 |
| repeated runs | 10 |
| warmup / iterations | 3 warmup / 20 measured iterations |
| positive | representative traits cases median speedup > 1.05x and no checksum mismatch |
| weak-positive | median > 1.0x with limited warnings; implementation remains small and fallback simple |
| neutral | median around 1.0x or mixed representative point types |
| negative | median < 1.0x for important representative point type or frequent degradation |
| unstable | decision bucket changes across budget or doctor warnings dominate |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production_direct。 |
| A/B boundary | Std/RVV 两侧调用真实 `PointCoding<PointT>::decodePoints` production entry。 |
| 当前决策问题 | traits-gated RVV production decode path 是否保持语义并在代表点型上快于 scalar production decode path。 |
| diagnostic 是否可外推到 production | 不使用旧 diagnostic 外推；本阶段重新用 production-direct case 判断。 |
| comparison-boundary / baseline mismatch 风险 | 低于早期 diagnostic；仍不覆盖完整 octree public entry。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 adoption | 不自动采纳；若代表点型中性或不稳定，则回退到 exact `PointXYZ` adopted scope。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | no；当前扩展是同一 RVV family 的点型 gate 扩围，不是新 family 取舍。 |

## 阶段完成条件

- correctness、QEMU smoke、asm 和 board repeated 都完成，且 Evidence Doctor 无 Error。
- `result.zh.md` 回填 actual scope、board bucket、Warnings 解释和是否采纳 traits gate。
- matrix、roadmap、evaluation、README、benchmark/evidence、optimization evidence、`doc-rvv` 和 Handoff 同步当前状态。
- 若 evidence 不支持扩围，production patch 回到 exact `PointXYZ` adopted scope，Phase 070 写成 rejected / no expansion。

## 继续 / 停止条件

本阶段默认继续到 Phase 070 EvidenceDecision。只有以下情况可停止：

- traits gate patch 编译或语义出现无法同轮修复的 blocker。
- 板卡不可用或 repeated target 无法运行。
- Evidence Doctor Error 无法修复，需要人工判断。
- dirty isolation 发现 `point_coding` 相关文件有无法区分的用户改动。

如果 traits expansion 采纳后仍要继续，下一方向才是完整 public octree end-to-end timing；该方向需要新的输入场景和授权边界。
