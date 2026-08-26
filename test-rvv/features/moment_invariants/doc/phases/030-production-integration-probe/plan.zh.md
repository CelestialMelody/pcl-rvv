# Phase 030 production integration probe plan

## 阶段意图和边界

本阶段把前两轮 diagnostic（诊断）结果推进到 production integration loop（生产接入闭环）。目标不是扩大整个模板入口，而是在真实 `MomentInvariantsEstimation::computeFeature` public entry（公开入口）内做一个有界 production direct（真实生产路径证据）探针：当 `__RVV10__` 开启、`PointInT` 满足 xyz 单 `float` AoS（结构数组）布局、输出类型为 `pcl::MomentInvariants`、邻域 indices 可用 32-bit byte offset 表达且邻域规模达到门槛时，indexed `computePointMomentInvariants` 的 centroid 之后六个中心矩累加走 RVV；其它路径自然回退原标量语义。

本阶段不修改 public API（公开接口），不扩大到 `Scalar=double`，不承诺所有 `PCL_XYZ_POINT_TYPES` 都有性能收益，不接 full-cloud overload 到生产分流，不修改 `compute3DCentroid`、`searchForNeighbors` 或 KdTree。若真实生产证据不成立，保留 patch 等待用户决定是否 rollback（回滚）；若证据成立，PI5 仍停在 `pending_user_confirmation_adopt_production`，等待用户确认后再把它写成 adopted production behavior（已采用生产行为）并创建 `doc-rvv/features/moment_invariants-RVV.zh.md`。

## 当前状态清单

| 项目 | 当前状态 | 路径 |
| --- | --- | --- |
| production source | 尚无 RVV dispatch；两个 `computePointMomentInvariants` overload 和 `computeFeature` 都是原标量。 | `features/include/pcl/features/impl/moment_invariants.hpp` |
| helper-only diagnostic | 5-run board median 1.141x，min 1.095x，Evidence Doctor 0E/0W/2S。 | `log/board/repeated_phase000_moment_accumulation_diagnostic/summary.md` |
| public-search-shaped diagnostic | 5-run board median 1.031x，min 1.026x，Evidence Doctor 0E/0W/3S，含 `near_threshold_ba` suggestion。 | `log/board/repeated_phase010_public_search_shape_diagnostic/summary.md` |
| correctness | Std/RVV helper 3/3 pass；只证明 test-only helper 与标量参考容差一致。 | `make run_test_compare` |
| asm | bench binary 出现 `vluxei32`、`vlse32`、`vfredusum`、`vfsub`、`vfmul`。 | `build/asm/riscv/bench_moment_invariants_rvv.asm` |
| registry | 已登记 Phase 000/010 summary、manifest、doctor；恢复检查 fresh。 | `log/evidence_registry.json` |
| 旧停止位 | Phase 020 把 production probe 写成需用户确认的 `turn_stop_deferred`。本轮用户已明确要求继续推进且板卡可用，因此该停止位在 Phase 030 降级为 stale stop decision（过期停止决策）。 | `doc/phases/README.zh.md`、`doc/optimization-roadmap.zh.md` |

## Phase scope 与扩展队列

| 字段 | 本阶段冻结 |
| --- | --- |
| `validated_scope` | `MomentInvariantsEstimation::computeFeature` 调用的 indexed neighbor path（索引邻域路径）；`PointXYZ` 作为主生产 bench case，同时允许 `RVVXYZAoSFloatLayout<PointInT>` 编译期 gate 命中的 xyz 单 `float` AoS 点型进入正确性回退审计；输出类型只批准 `pcl::MomentInvariants`；邻域规模至少 `kMomentInvariantsMinRVVPoints`；QEMU 只做 correctness / asm，不做性能结论；板卡做 repeated production public Std/RVV。 |
| `unvalidated_scope` | full-cloud `computePointMomentInvariants(cloud, j1, j2, j3)` production RVV；`Scalar=double`；非 xyz 单 `float` 点型；非 AoS / POD layout；非 dense 查询点语义扩展；search/KdTree 优化；其它输出点类型。 |
| `point_type_expansion_queue` | 若 PI5 证据支持采纳，后续单独 phase 评估 `PointXYZI`、`PointXYZRGB(A)` 和自定义 xyz traits 点型的 correctness、fallback、asm、board 和 Evidence Doctor；本阶段不把 `PointXYZ` 结果外推成完整泛型性能结论。 |
| `phase_closeout_boundary` | 只关闭 `production-public / public overload / RVV-vs-scalar` 的窄范围生产探针；不能关闭 RVV-family-selection（实现族选择）或泛型点类型扩展。 |

## 候选实现

| candidate family | 设计 | 预期收益 | 风险 / 未知 |
| --- | --- | --- | --- |
| production indexed moment accumulation RVV | 抽出 indexed 标量 helper，再在 `__RVV10__` 下新增 indexed RVV helper；centroid 保持原 `compute3DCentroid` 标量，中心矩循环用 `indexed_load3_f32m2` + `vfredusum`。 | 复用 Phase 000/010 的弱正向信号，在真实 `computeFeature` 中确认是否仍快。 | 收益只有近阈值；vector reduction（向量规约）改变累加树；生产符号可能内联导致 asm 归属要按 public/helper 边界查找；泛型点型性能未证明。 |

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | Phase 000 是 `diagnostic`，Phase 010 是 `production-shaped diagnostic`，Phase 030 将生成 `production-public`。 |
| A/B boundary | 旧证据分别是 `test helper` 和 `production-shaped helper`；本阶段目标是 `public overload`。 |
| 当前决策问题 | `RVV-vs-scalar`：当前 public RVV path 是否快于当前 public scalar path。不是 RVV-family-selection。 |
| diagnostic 是否可外推到 production | 不能直接外推。Phase 010 计入了 KdTree search，但仍通过 test-only helper replacement，不证明真实 dispatch、fallback 或 production asm。 |
| comparison-boundary / baseline mismatch 风险 | 有。旧 bench 的 candidate wrapper 与真实 production public overload 不同，且 checksum 是 test wrapper 摘要；本阶段必须重跑 production direct bench。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 允许，因为用户明确要求继续推进、板卡可用、候选实现小、fallback 简单、public API 不变；若 fallback 或生产测试无法隔离则暂停。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 本阶段没有已有 production RVV family，因此 production-public Std/RVV positive 可以支持有界采纳候选；若后续新增其它实现族，再补 production-detail RVV-vs-RVV A/B。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production indexed moment accumulation RVV | source-indexed neighbor list from KdTree | `PointXYZ / float / AoS`，traits gate 命中 | public `computeFeature` | 新增 production direct gtest；新增 fallback gtest | 新增 `mi_production_compute_feature` | 新增 repeated production summary，5-run 初始预算 | production helper 或 public overload 中的 `vluxei32` / `vfredusum` | summary manifest + doctor | planned |
| non-RVV / unsupported types fallback | same public entry | 非 RVV build、非覆盖输出类型、小邻域或布局不满足 | public `computeFeature` / helper overload | Std build 通过；小邻域与非 RVV 编译保持标量 | not_applicable | not_applicable | not_applicable | not_applicable | planned |
| generic point type expansion | source-indexed neighbor list | `PointXYZI`、`PointXYZRGB(A)`、自定义 xyz traits | public `computeFeature` | 后续 phase | 后续 phase | 后续 phase | 后续 phase | 后续 phase | deferred |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| PI1 plan | 本文件 | 范围、fallback、bench、asm、board、doctor 和 PI5 暂停条件已冻结。 |
| RED test / bench | `src/test_moment_invariants.cpp`、`src/bench_moment_invariants.cpp`、`Makefile` | 生产补丁前，production RVV asm gate 或 production path marker 检查失败，证明测试能观察到缺失的生产 RVV 分流。 |
| PI2 production patch | `features/include/pcl/features/impl/moment_invariants.hpp` | public entry 保持短分流；原标量主体抽到 `*_Std`；RVV helper 只在 `__RVV10__` 下存在；fallback 清楚。 |
| PI3 production direct tests | topic gtest | Std/RVV correctness 通过；fallback case 单独覆盖小邻域 / 非 RVV build 或 unsupported output。 |
| PI4 evidence rerun | QEMU logs、asm、board repeated summary、manifest、doctor、registry | QEMU correctness、production asm 归属、板卡 repeated 和 Evidence Doctor 全部生成。 |
| PI5 decision | `result.zh.md`、matrix、roadmap、evaluation、Handoff | 根据 production evidence 写 `pending_user_confirmation_adopt_production` 或 `pending_user_confirmation_rollback`。 |

## Evidence Doctor 与 registry

生产 repeated summary 使用同一个 `script/generate_mi_repeated_summary.py`，但 metadata 必须覆盖：

- `evidence_role=production-public`
- `A/B boundary=public overload`
- `timer_boundary=public_compute_feature_with_kdtree_search_and_output_write`
- `row_source=kd_tree_k_neighbor_query`
- `case_kind=production-direct`
- `gate=__RVV10__ + RVVXYZAoSFloatLayout<PointInT> + PointOutT MomentInvariants + indexed neighbor size`

summary 后运行 `../../script/evidence_doctor.py --manifest ... --fail-on never`，再运行 `../../script/evidence_registry.py record`。若出现 Error，禁止使用该 summary 做生产结论；Warning 必须解释或降级。

## 板卡复跑预算和决策桶

初始预算为 5 run、每 run warm-up 2 次、计时 iteration 8 次，case 为 `mi_production_compute_feature`，输入 `points=4096`，与 Phase 010 的 public-search-shaped 规模一致。若 median 落在 `1.03x` 附近并触发 near-threshold、但方向稳定，最多再做一次同边界 5-run 确认；若两批 decision bucket 摇摆则标 `unstable`。`positive` 需要 median 和 min 均明显高于 1.0；`weak_positive` 允许近阈值但要写维护成本和人工确认风险；`neutral/negative` 不建议采纳。

## 继续 / 停止条件

本阶段默认同轮推进 PI1-PI5。只有以下情况停止：生产补丁需要改 public API 或公共 helper；fallback gate 无法隔离；QEMU correctness 失败且不能快速定位；asm 无法证明生产 RVV 指令归属；板卡不可达或 repeated summary 出现矛盾；PI5 到达用户确认点。

## 文档更新清单

本阶段结束必须更新：

- `doc/phases/030-production-integration-probe/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/moment_invariants-evaluation.zh.md`
- `doc/testing-overview.zh.md`
- `doc/correctness-tests.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/test-support-code-map.zh.md`
- 若 PI5 证据支持且用户确认采纳后，才创建 `doc-rvv/features/moment_invariants-RVV.zh.md` 并同步筛选状态表。

## roadmap 同步动作

Phase 030 会把旧 `production integration readiness audit` 从 `turn_stop_deferred` 改成本阶段 planned / attempted，并把后续 point type expansion 和 full-cloud overload 保留为 deferred。若 production direct 结果低于 Phase 010，roadmap 记录 production wrapper / output 写回 / helper inline 作为后续 profile 或 rollback 解释候选。
