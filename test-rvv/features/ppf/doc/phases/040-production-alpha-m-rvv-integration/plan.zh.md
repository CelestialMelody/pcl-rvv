# Phase 040 Production Alpha-M RVV Integration Plan

## 阶段意图和边界

本阶段进入 production integration loop（生产接入闭环）。目标是在真实
`pcl::PPFEstimation<PointXYZ, Normal, PPFSignature>::computeFeature` 公开入口中，
有界接入 Phase 030 已证明正向的 `alpha_m` 后段 RVV（RISC-V Vector，可伸缩向量）
实现，并用 production direct（真实生产路径直连）证据重新判断是否值得保留当前补丁。

本阶段只证明以下 `validated_scope`：

| dimension | scope |
| --- | --- |
| production entry | `features/include/pcl/features/impl/ppf.hpp` 的 `PPFEstimation::computeFeature` |
| row source policy | `indices_ × input_` 的 ordered all-pairs 输出顺序 |
| point type / output type | exact `pcl::PointXYZ + pcl::Normal + pcl::PPFSignature` |
| Scalar / layout | float 字段、AoS（Array of Structures，结构数组）点云 |
| RVV stage | 仅 `alpha_m` 旋转后段公式；`f1..f4` 继续调用现有 `pcl::computePairFeatures` |
| build gate | 仅 `__RVV10__` 构建；非 RVV 构建保持原标量路径 |
| evidence role | production-public Std/RVV A/B，用来判断当前 public RVV path 是否快于 public scalar path |

本阶段明确不证明：

- 不替换为 `pcl::computePPFPairFeature`，因为当前 production 实际调用的是
  `pcl::computePairFeatures`。
- 不接入 Phase 010 的 pair-feature batch RVV 候选，该候选在板卡 repeated benchmark 中为
  negative。
- 不证明泛型 `PointInT` / `PointNT` / `PointOutT`、其它 normal 字段布局、`double`、
  correspondence row source 或双索引 row source。
- 不把 production-public Std/RVV positive 写成 RVV-family clean adoption；当前 production
  没有其它已采用 RVV family，因此本阶段最多停在 PI5 用户检查点，由用户确认保留或回滚。

## 当前状态清单

| item | current state |
| --- | --- |
| scalar reference | `test-rvv/features/ppf/include/impl/ppf_reference.hpp` 已对齐当前 production `computePairFeatures` 路径。 |
| Phase 010 | `candidate_ppf_pair_feature_batch_rvv` correctness 通过，但 5-run board speedup 为 `0.80, 0.79, 0.79, 0.79, 0.81`，拒绝进入 production。 |
| Phase 020 | `computeAlphaMClosedForm` 与 Eigen reference 对拍通过，形成 alpha_m 后段公式基础。 |
| Phase 030 | `candidate_ppf_alpha_m_batch_rvv` 5-run board speedup 为 `1.57, 1.57, 1.56, 1.57, 1.57`，作为 production probe 候选。 |
| production source | `features/include/pcl/features/impl/ppf.hpp` 当前无 RVV dispatch。 |
| production doc | `doc-rvv/features/ppf-RVV.zh.md` 尚不适用，需等 production direct 证据后再创建或更新。 |

## 假设与候选族

候选族为 `production alpha_m batch RVV`。实现假设是：当前 production 标量路径里，
每个有效点对仍需先计算 `f1..f4`，而 `alpha_m` 的 Eigen `AngleAxisf` / `Affine3f`
对象构造成本可由 SoA staging + RVV 闭式公式降低。由于 Phase 030 已在相同数据构造下显示
alpha-only 诊断候选有稳定板卡收益，本阶段的决策问题从“是否存在局部收益”转为
“真实 public entry 接入后是否仍有收益，并且 fallback 是否可维护”。

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production alpha_m batch RVV | ordered `indices_ × input_` | exact `PointXYZ + Normal + PPFSignature` / float / AoS | public `PPFEstimation::computeFeature` | add production-direct gtest; `run_test_compare` Std/RVV | `public_ppf_compute` case only | 5-run board repeated with public case | production helper / public template symbol contains RVV ops | repeated manifest + doctor | planned |
| fallback scalar path | same public entry | non-RVV build and all non-covered template instantiations | public `computeFeature` fallback | Std build correctness; optional fallback compile smoke | not_applicable | not_applicable | no RVV expected in Std binary | not_applicable | planned |

## 实现和测试动作

1. 新增 production-direct RED test：RVV 构建中要求公开 `PPFEstimation<PointXYZ, Normal, PPFSignature>::compute`
   与 reference 输出一致，并通过 test-only trace（测试专用路径命中计数）证明命中 production RVV
   分流；当前无 production dispatch 时应失败。
2. 新增必要的 test-only trace 宏，只在 production-direct 测试翻译单元里定义，不暴露 public API，也不进入 bench 二进制。
3. 将 `computeFeature` 原标量主体抽为邻近 `computePPFFeatureStd` helper，公开入口只保留 dispatch
   与 fallback。
4. 在 `__RVV10__` 下新增 exact-type gated production RVV helper：先复用标量 `computePairFeatures`
   计算 `f1..f4` 和 failure / identity 行，再批量 RVV 计算成功点对的 `alpha_m`。
5. 更新 bench manifest，使 `public_ppf_compute` 在 RVV 构建中作为 production-public 证据；历史
   diagnostic case 保持原角色。
6. 执行 `run_test_compare`、QEMU public smoke、反汇编归属、板卡 5-run repeated 和 Evidence Doctor。
7. 根据 production-side 证据更新 phase result、optimization matrix、roadmap、evaluation 和
   `doc-rvv/features/ppf-RVV.zh.md`；若证据不支持保留，则只写 PI5 回滚检查点，不自行回滚。

## Evidence Doctor 和 registry 规则

本阶段使用 topic-local manifest：

- `test-rvv/features/ppf/log/board/repeated/evidence_manifest.json`
- `test-rvv/features/ppf/log/board/repeated/evidence_doctor.md`
- `test-rvv/features/ppf/log/board/repeated/evidence_doctor.json`

Evidence Doctor Error 必须解释、修复重跑、降级或阻塞。旧 `candidate_ppf_pair_feature_batch_rvv`
退化 Error 若仍被全量 manifest 纳入，只能影响该历史候选，不得污染 `public_ppf_compute`
production-public 结论；本阶段板卡命令优先用 `--case-filter public_ppf_compute` 隔离 production
证据。

## 板卡复跑预算和决策桶

| item | value |
| --- | --- |
| board availability | 用户已说明当前板卡可用；若 ssh/rsync/target 失败才转为工具阻塞。 |
| repeated budget | 5 runs；若脚本失败可修复命令后重跑一次 5-run 批次。 |
| bench shape | `--side 28 --index-count 64 --repeat 8 --iterations 8 --warmup 2 --case-filter public_ppf_compute` |
| positive | 5-run mean speedup >= 1.20 且 checksum 一致，且 Evidence Doctor 无影响 public case 的 Error。 |
| weak-positive | 1.05 <= mean speedup < 1.20，需实现小、fallback 简单、doctor 无关键 Error。 |
| neutral | 0.95 <= mean speedup < 1.05。 |
| negative | mean speedup < 0.95 或 3/5 以上 run 低于 0.95。 |
| unstable | decision bucket 在重跑预算内摇摆，或 doctor 指出长尾 / 缺口无法解释。 |

## Diagnostic-to-Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | Phase 030 是 diagnostic；Phase 040 必须生成 production-public 证据。 |
| A/B boundary | Phase 030 A/B 是 test helper；Phase 040 A/B 是 public overload 的 Std/RVV 构建。 |
| 当前决策问题 | 当前 public RVV path 是否快于当前 public scalar path，以及是否值得停在 PI5 让用户确认保留。 |
| diagnostic 是否可外推到 production | 不直接外推；只用于授权 bounded production probe。 |
| comparison-boundary / baseline mismatch 风险 | 有。Phase 030 的 helper 边界和 production public dispatch 不同，所以 production side 数据优先。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | Phase 030 实际为 positive；若 Phase 040 变弱或负，以 Phase 040 为准。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前无已采用 RVV family；本阶段仍必须停在 PI5 用户检查点，不能自行 clean-adopt。 |

## 阶段完成条件

本阶段完成到 PI5 的条件：

- production diff 存在且公开入口保留清晰 Std fallback。
- production-direct correctness 和 trace 命中测试在 RVV 构建通过，Std 构建保持标量正确性。
- QEMU public smoke 只作为可运行 / 日志形状证据。
- 反汇编可在 production helper 或 public template 归属范围内看到 RVV 指令。
- 板卡 repeated public case 完成并生成 Evidence Doctor 报告。
- phase result、roadmap、matrix、evaluation 和适用的 `doc-rvv` 已按 production-side 证据刷新。
- 最终状态写成 `PI5 pending_user_confirmation_adopt_or_rollback`，等待用户确认保留或回滚当前 production patch。

## 继续 / 停止条件

本阶段默认持续推进到 PI5。合法停止条件只有：

- production patch 需要扩大到本计划之外的 public API、其它 topic、泛型点类型 traits 或其它 row source。
- 板卡、工具链或远端运行实际不可用。
- correctness / checksum / Evidence Doctor 出现无法解释的 Error。
- dirty isolation 显示当前 production 或 topic 文件有用户并发修改且会被覆盖。

若 PI5 证据为 positive，本轮仍停止等待用户确认采纳；若为 neutral / negative，本轮停止等待用户确认是否回滚生产补丁。

## 文档更新清单

- 本 phase `result.zh.md`：记录 PI1-PI5 实际结果。
- `test-rvv/features/ppf/doc/phases/optimization-matrix.zh.md`：新增 production-public 行。
- `test-rvv/features/ppf/doc/optimization-roadmap.zh.md`：将 production probe 从授权阻塞改为 Phase 040 执行中 / PI5 状态。
- `test-rvv/features/ppf/doc/ppf-evaluation.zh.md`：同步 production integration evidence。
- `doc-rvv/features/ppf-RVV.zh.md`：仅在 production patch 与 production-side 证据完成后创建或刷新。
- `test-rvv/features/ppf/README.zh.md`：同步当前恢复入口、常用命令和 production patch 状态。

## Roadmap 同步动作

本阶段若 production-public 为 positive，后续扩展队列为：

- `PointXYZ-like + Normal-like` traits gate 泛型扩展：需要读取 generic point type strategy 并新增
  fallback / point type matrix。
- direct-AoS 或 alpha staging buffer 轻量化：只有 production-public positive 且需要进一步压榨收益时再开。
- 其它 row source policy：需要另开 phase 或 topic，不由本阶段关闭。

若 production-public 为 neutral / negative，当前生产补丁不建议保留，topic 回到 diagnostic
资产和 no-production closeout。
