# PFH Optimization Roadmap

## 当前边界

当前 topic 目标是 `features/include/pcl/features/impl/pfh.hpp`。当前已采纳生产边界是 exact
`PointNormal -> PointNormal` 与 exact `PointXYZ -> Normal`、`PFHSignature125`、`float`、
AoS xyz / normal float 布局、`use_cache_ == false`、默认 5x5x5 PFH histogram。Phase 040 已完成
exact `PointNormal` PI2-PI5，接入后板卡 production-public 5-run 为 `1.92x-1.94x`，用户已确认采纳；
Phase 060 已独立完成 `PointXYZ + Normal` 扩展，production-public 5-run 为 `1.84x-1.87x`，按用户策略采纳。
两个 exact 组合都不能外推为完整模板泛型结论。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `pfh-scalar-reference-scaffold` | PFH production source | `computePointPFHSignature` same-chain tests | 建立后续候选的 correctness gate | reference 漏语义会污染全部证据 | Std/RVV gtest 对拍、degenerate pair fallback、public finite descriptor | adopted_as_test_baseline | no next phase inside this family |
| `pfh-pair-feature-batch-diagnostic` | queue row and FPFH sibling risk | all-pairs pair feature math | 可能减少 O(k^2) math cost | `acos`/`atan2` helper、scatter、pair order、fallback 复杂 | component bench、asm、board repeated、Doctor | positive_diagnostic_candidate | `020-production-integration-plan` |
| `pfh-production-staged-probe` | Phase 010 positive diagnostic | `computePointPFHSignature` production detail helper | 若 direct AoS 生产探针失败，可作为备选 family | staging 成本、模板点类型 gate、fallback、inline/asm 归属、public search 稀释 | 仅在 direct AoS production evidence 负向或不可维护时恢复 PI1 | deferred | after direct AoS PI5 |
| `pfh-direct-point-load-rvv` | Phase 010 reflection and common point-load API | all-pairs pair feature math without SoA staging | Phase 030 repeated 稳定强于 staged，已作为首个生产探针 family | 只覆盖 `PointNormal` exact layout；public path 可能被 search 稀释 | production direct correctness/fallback、asm、board repeated、Doctor、PI5 user checkpoint | promoted_to_production_probe | `040-direct-aos-production-probe` |
| `pfh-direct-aos-production-rvv` | Phase 040 production probe | exact `PointNormal -> PointNormal` production `computePointPFHSignature` | 接入后 component 5-run mean `2.004x`，public 5-run mean `1.926x` | exact gate 覆盖面窄；未覆盖 `PointXYZ + Normal` | Phase 050 长期文档 closeout；后续点型扩展独立补证据 | adopted_after_user_confirmation | `050-production-closeout-doc-rvv` |
| `pfh-pointxyz-normal-production-expansion` | Phase 040 point type expansion queue | common `PointXYZ + Normal` production combo | 扩大实际覆盖面，source cloud 可只读 xyz、normal cloud 可只读 normal | exact gate 仍不覆盖泛型 traits | Phase 060 fallback tests、production direct correctness、asm、board repeated、Doctor | adopted | no next phase inside this exact combo |
| `pfh-generic-xyz-normal-traits` | Phase 060 reflection | PointXYZ-like source + Normal-like normals traits 集合 | 进一步扩大模板覆盖面 | 需要公共 normal AoS gate、更多点型编译 / 运行证据、fallback 矩阵更宽 | 新 phase：traits audit、compile matrix、correctness、asm、board repeated、Doctor | deferred | separate broader-scope phase |
| `pfh-histogram-copy-rvv` | source shape scan | 125-bin output copy in `computeFeature` | 小规模固定 copy 可能有轻微收益 | 可能被 search 和 pair math 稀释，收益预计远小于 pair math | component ablation and public bench | not_recommended_now | resume only if profile shows output copy hot |
| `pfh-family-shared-helper` | `features/src/pfh.cpp` shared helper | PFH/FPFH/VFH/PFHRGB callers | 若 pair math helper 成立可跨 family 复用 | 文件自身无 batch API，生产 API 风险高 | caller-shaped tests per topic | deferred | separate follow-up after PFH evidence |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | Staged pair-feature RVV candidate | Baseline repeated 是 neutral，说明需要真实 candidate；已有 `acos_RVV_f32m2` / `atan2_RVV_f32m2` 可做 test-only 数学链路验证。 | candidate correctness、asm、board repeated、Doctor | high |
| 010 | Production staged probe | Staged candidate 在板卡 5-run repeated 为 `2.33x-2.35x`，且 Doctor 无 Error。 | PI1 fallback/gate plan；用户授权后补 production direct tests、asm、board repeated、Doctor | high |
| 030 | Direct AoS family comparison | direct AoS 5-run `2.86x-2.90x`，稳定强于 staged `2.33x-2.35x`，Doctor `0E/0W/10S`。 | production direct correctness/fallback、asm、board repeated、Doctor、PI5 user checkpoint | high |
| 040 | Point type expansion queue | exact `PointNormal` production probe 正向并已被用户采纳，但常见 `PointXYZ + Normal` 未覆盖。 | 下一 PI phase 补 traits / layout / fallback / board evidence | high |
| 060 | Stop same-turn expansion | `PointXYZ + Normal` 已在 production-detail/public 两条边界 positive；剩余泛型 traits、cache、OMP 和 histogram scatter 都需要更宽语义或 profile 证据。 | 新 phase 或新 topic 先补 traits/profile/target-scope 计划 | low for same turn |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| `production-pfh-rvv-dispatch` | Phase 040 production direct board evidence 已正向，用户已确认采纳；Phase 050 负责长期文档收口。 | 若后续发现 production direct 证据矛盾，按新 phase 重新降级或请求用户判断。 |
| `pfh-production-staged-probe` | Direct AoS production evidence 已强于 staged diagnostic 预期；当前不建议再把 staged SoA 接入生产。 | 只有 direct AoS patch 被回滚或后续点型扩展证明 direct AoS 不适用时恢复。 |
| `pfh-cache-path-rvv` | cache path 有 pair feature cache 状态和 map key 成本，没有 profile 证明它是当前热点。 | profile 指向 `use_cache_ == true` 的 PFH 工作负载后再开窄 phase。 |
| `pfh-omp-path-rvv` | OpenMP/RVV 嵌套调度和线程成本会扩大到 OMP topic，不适合同轮追加。 | 以 OMP path 为目标另开 topic，并提供并行 workload/profile。 |

## 默认恢复动作

`next_phase_default`: none for current exact-scope topic。

当前同轮不建议继续追加生产优化。若用户后续明确要扩大范围，优先新建 `070-generic-xyz-normal-traits`
或 cache / OMP 专项 phase；这些方向需要新的 phase plan 和独立证据，不能复用 Phase 040 / 060 的 exact
点型结果直接关闭。
