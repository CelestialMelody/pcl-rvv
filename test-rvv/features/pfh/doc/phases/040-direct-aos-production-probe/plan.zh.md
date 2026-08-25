# Phase 040 Plan: direct AoS production probe

## 阶段意图和边界

本阶段进入 production integration loop（生产接入闭环）的有界探针，把 Phase 030 更强的 `pfh-direct-point-load-rvv` family 接到 `features/include/pcl/features/impl/pfh.hpp`。本阶段只证明：

- `__RVV10__` 构建；
- `PFHEstimation<pcl::PointNormal, pcl::PointNormal, pcl::PFHSignature125>`；
- `use_cache_ == false`；
- dense finite synthetic `PointNormal` cloud；
- fixed KSearch neighborhood / component helper；
- `nr_split == 5`，PFHSignature125 默认 5x5x5 histogram；
- 32-bit byte offset 可表达的 cloud size。

不证明 `PointXYZ + Normal`、PointXYZ-like / Normal-only traits、cache path、PFHRGB/VFH/FPFH shared helper、OMP、custom point type、`Scalar=double` 或其它 row source。

## 当前状态清单

| 项目 | 当前事实 |
| --- | --- |
| Phase 030 family selection | direct AoS 5-run `2.86,2.86,2.90,2.87,2.87`，staged SoA 5-run `2.34,2.34,2.35,2.34,2.33`；Doctor `0E/0W/10S`。 |
| production source | `pfh.hpp::computePointPFHSignature` 当前全标量，无 `__RVV10__` 分支。 |
| production doc | 尚无 adopted production behavior；`doc-rvv/features/pfh-RVV.zh.md` 只有 PI5 production evidence 支持且用户确认采纳后才创建。 |
| board availability | 当前会话确认板卡可用；本阶段必须跑 board smoke / repeated / Doctor。 |

## 候选与风险

| candidate family | 假设 | 风险 | 必需证据 |
| --- | --- | --- | --- |
| `pfh-direct-aos-production-rvv` | 真实 production helper 中直接按 pair index gather xyz/normal，可把 Phase 030 的组件收益带到 public PFH。 | exact `PointNormal` gate 覆盖面窄；public case 可能被 KdTree search 稀释；RVV atan2 近似可能导致 bin 边界误差；每次 descriptor 分配临时数组有成本。 | production direct gtest/fallback、asm 归到 `pfh.hpp`、board repeated 中 component positive 且 public 不退化、Doctor 无阻塞 Error。 |
| `pfh-direct-aos-point-type-expansion` | 后续可拆成 source xyz + normals normal 两侧 traits，覆盖 `PointXYZ + Normal`。 | 需要新增 normal-only AoS load / gate、fallback tests 和 production direct bench。 | 独立后续 phase；本阶段不做。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `pfh-direct-aos-production-rvv` | fixed KSearch neighborhood indices | exact `PointNormal -> PointNormal`, float, AoS xyz+normal | production `computePointPFHSignature` under `__RVV10__` | `run_test_compare` with production component/public correctness and cache fallback | `component_pfh_signature`, `public_pfh_k`; keep diagnostic candidate cases as comparison only | PI4 board smoke and 5-run repeated | RVV gather/math instructions must map to `features/include/pcl/features/impl/pfh.hpp` | repeated manifest + Doctor | pending PI5 |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| A1 production direct RED | 在 `test_pfh.cpp` 增加生产直连 / fallback 语义测试。 | 未接 production RVV 时，RVV-specific gate 不能闭合；接入后 Std/RVV correctness 仍 pass。 |
| A2 production patch | `pfh.hpp` 新增 `pcl::detail::computePointPFHSignatureDirectAoSRVV`，入口在标量主体前短路尝试。 | 非 RVV、非 exact `PointNormal`、cache、`nr_split != 5`、规模不足或 offset gate 失败时自然 fallback。 |
| A3 QEMU correctness | `make -B -C test-rvv/features/pfh run_test_compare` | Std/RVV 全部 pass。 |
| A4 asm | `make -B -C test-rvv/features/pfh dump_bench_rvv` | 至少一组 gather/math RVV 指令可归到 `pfh.hpp` production helper 或其 inline 区域。 |
| A5 board smoke | `make -C test-rvv/features/pfh board_smoke BENCH_ARGS='--side 32 --k 32 --iterations 3 --warmup 1'` | 板端 test pass，bench log 可解析。 |
| A6 board repeated | `make -C test-rvv/features/pfh board_repeated BENCH_ARGS='--side 32 --k 32 --iterations 8 --warmup 2' REPEATED_BOARD_OUTPUT_DIR=log/board/pi2-production-direct-aos/repeated` | 5-run summary 覆盖 component/public/candidate cases。 |
| A7 Evidence Doctor / registry | `make -C test-rvv/features/pfh evidence_doctor_repeated ...pi2-production-direct-aos...`；刷新 `log/evidence_registry.json`。 | Errors / Warnings / Suggestions 全部解释；Error 未解决时不能建议保留。 |
| A8 文档刷新 | phase result、matrix、roadmap、evaluation / queue；PI5 后等待用户确认。 | 正式 `doc-rvv/features/pfh-RVV.zh.md` 只在用户确认采纳后创建，且使用 production 接入后的板卡数据。 |

## 板卡复跑预算和决策桶

- 默认预算：5 runs；若 Doctor 无 Error 且 decision bucket 稳定，不无限复跑。
- `positive`：production component mean/median >= `1.20x` 且 public mean/median > `1.00x`，退化频率低。
- `weak_positive`：component 明显正向但 public 仅 `1.05x-1.20x`；若 diff 小、fallback 清楚可建议保留。
- `neutral`：public 约 `1.00x` 或方向摇摆；PI5 停下给用户判断。
- `negative`：public median < `1.00x` 或 `B/A < 1` 频率高；PI5 建议回滚但不自行回滚。
- `unstable`：5-run direction bucket 摇摆或 Doctor 指出无法解释的矛盾；降级证据并暂停。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | 本阶段目标是 `production-detail` + `production-public`；Phase 030 direct/staged 只作为 historical diagnostic。 |
| A/B boundary | Std build 原标量 production helper vs RVV build 中同一 production helper 的 RVV dispatch。 |
| 当前决策问题 | `RVV-vs-scalar`：当前 production patch 是否值得保留。 |
| diagnostic 是否可外推到 production | 只能作为 PI2 候选排序。PI5 采纳判断只看 production boundary 内证据。 |
| comparison-boundary / baseline mismatch 风险 | aggregate bench 仍包含 test-only candidate cases；result 必须分开 production component/public 与 diagnostic candidate。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已进入有界探针；若生产证据弱/负/不稳，停在 PI5，不自行回滚。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前没有已有 adopted RVV family；若 production RVV-vs-scalar 正向，可建议保留，但最终 adopted 仍需用户确认。 |

## 继续 / 停止条件

PI5 前默认继续。只有 production diff 需要扩大到本计划未授权点型 / API、production correctness 或 asm 无法闭合、板卡不可用、Doctor Error 未能修正，或 PI5 已完成需要用户确认采纳 / 回滚时停止。
