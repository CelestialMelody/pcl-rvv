# Phase 060: production integration execution 结果

## 阶段范围

| 项 | 内容 |
| --- | --- |
| validated_scope | 三条 public entry（公开入口）：`selectWithinDistance`、`countWithinDistance`、`getDistancesToModel`；source 点型为 `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`；normal 点型为 `pcl::Normal`；direct indexed `indices_`；float xyz / normal AoS（结构数组）布局；65536 点、200 iteration（迭代）板卡 repeated。 |
| unvalidated_scope | 其它 normal 点型、自定义 registered point type、`Scalar=double`、其它 layout、非法 index、真实 RANSAC workload、identity-index 专门路径、其它硬件和非当前距离三入口。 |
| phase_closeout_boundary | 只关闭 Phase050 冻结的窄 production scope；不外推到完整泛型模板入口或其它 normal-like 点型。 |

## 实现结果

本阶段完成 PI2-PI5 production integration loop（生产接入闭环）：

1. production 文件 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp` 新增 Standard helper 和 RVV helper。
2. 三条 public entry 在 RVV 构建下先尝试 RVV helper；gate 不满足时回退到 Standard helper。
3. test-rvv 侧新增 production direct（真实生产路径）correctness / fallback gtest，并保留历史 diagnostic candidate 作为回归。
4. Phase060 生成 production-public（真实公开入口）board repeated summary、manifest 和 Evidence Doctor 报告。
5. 用户本轮偏好明确为“板卡上的测试结果如果显示有收益即可采纳；正式 `doc-rvv` 数据采用接入后的板卡测试数据”，因此本阶段在 12/12 positive 且 Doctor 0/0/0 后进入 S11 production closeout，并创建正式 `doc-rvv/sample_consensus/sac_model_normal_sphere-RVV.zh.md`。

## Correctness / Fallback

| 命令 | 结果 | 证明范围 |
| --- | --- | --- |
| `make -C test-rvv/sample_consensus/sac_model_normal_sphere run_test_compare` | Std/RVV 两个构建各 8 个 gtest 通过。 | public entry、production detail helper、large input dispatch、四种 source 点型和 fallback 语义与参考链路一致。 |
| `ProductionRVVDetailHelpersMatchPublicReference` | RVV 构建下四种 source 点型的 count/select/getDistances helper 均返回 hit 并与参考一致。 | 证明 production detail helper 可以命中当前窄 gate。 |
| `ProductionFallbacksKeepPublicReferenceSemantics` | 小输入和 normal cloud 覆盖不足时 `canUseRVVNormalSphere` 返回 false，public entry 仍与参考一致。 | 证明关键 fallback gate 不扩大语义。 |

## 反汇编归属

`make -C test-rvv/sample_consensus/sac_model_normal_sphere dump_bench_rvv` 已重新生成 RVV bench 反汇编。
抽查显示 `computeNormalSphereDistanceRVV` 和 production helper 路径附近可见 `vfsqrt.v`、`vcpop.m`、
`vcompress.vm`、`vfwcvt.f.f.v`、`vse64.v` 等关键 RVV 指令。反汇编只证明路径和指令归属，不作为性能结论。

## Board Repeated 结果

Phase060 使用接入后的真实 public entry Std/RVV 板卡对比。rerun budget（复跑预算）为 5/5 used；
所有 comparison 的 decision bucket（决策桶）稳定为 `positive`，没有继续复跑需求。

| case | point type | baseline ms | candidate ms | B/A speedup | bucket |
| --- | --- | ---: | ---: | ---: | --- |
| `public selectWithinDistance` | `PointXYZ + Normal` | 8.2819 | 2.7093 | 3.039x | positive |
| `public countWithinDistance` | `PointXYZ + Normal` | 7.5855 | 2.2804 | 3.332x | positive |
| `public getDistancesToModel` | `PointXYZ + Normal` | 11.0847 | 2.4861 | 4.456x | positive |
| `public selectWithinDistance` | `PointXYZI + Normal` | 8.2453 | 2.8430 | 2.900x | positive |
| `public countWithinDistance` | `PointXYZI + Normal` | 7.6386 | 2.2700 | 3.358x | positive |
| `public getDistancesToModel` | `PointXYZI + Normal` | 11.1768 | 2.6948 | 4.157x | positive |
| `public selectWithinDistance` | `PointXYZRGB + Normal` | 8.2390 | 2.7747 | 2.973x | positive |
| `public countWithinDistance` | `PointXYZRGB + Normal` | 7.5953 | 2.2915 | 3.308x | positive |
| `public getDistancesToModel` | `PointXYZRGB + Normal` | 11.1085 | 2.5904 | 4.290x | positive |
| `public selectWithinDistance` | `PointXYZRGBA + Normal` | 8.2208 | 2.8293 | 2.906x | positive |
| `public countWithinDistance` | `PointXYZRGBA + Normal` | 7.5989 | 2.2367 | 3.411x | positive |
| `public getDistancesToModel` | `PointXYZRGBA + Normal` | 11.0953 | 2.6681 | 4.161x | positive |

证据路径：

- summary：`test-rvv/sample_consensus/sac_model_normal_sphere/doc/phases/060-production-integration-execution/board-evidence-summary.md`
- manifest：`test-rvv/sample_consensus/sac_model_normal_sphere/doc/phases/060-production-integration-execution/board-evidence-manifest.json`
- Evidence Doctor：`test-rvv/sample_consensus/sac_model_normal_sphere/doc/phases/060-production-integration-execution/board-evidence-doctor.md`

## Evidence Doctor 和 Registry

| 项 | 结果 | 处理 |
| --- | --- | --- |
| Evidence Doctor | `Errors=0`、`Warnings=0`、`Suggestions=0`。 | 不需要降级证据边界。 |
| registry | `record_phase060_evidence_state` 已登记；`evidence_status` 检查为 fresh。 | Phase060 summary / manifest / Doctor 可作为 summary-only evidence（只提交摘要证据）候选。 |
| raw logs | 保留在 `log/board-phase060-repeated-*` 本机目录。 | 默认不提交。 |

## Diagnostic 到 Production Mismatch Audit 回填

| question | result |
| --- | --- |
| evidence role | Phase060 的当前证据角色是 `production_public`；Phase000-030 降级为 historical diagnostic（历史诊断）输入。 |
| A/B boundary | board Std build public overload vs board RVV build public overload after production dispatch integration。 |
| 当前决策问题 | 当前 public RVV path 是否快于当前 public scalar path，以及是否按用户偏好采纳本生产 patch。 |
| diagnostic 是否可外推到 production | 不再外推；本阶段已使用 production direct/public 证据重判。 |
| comparison-boundary / baseline mismatch 风险 | Phase060 baseline/candidate 都是真实 public overload，checksum 一致；风险已由 manifest 记录。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不适用；本阶段 12/12 comparison 都是 positive。 |
| clean adoption 是否需要同一 production boundary RVV-vs-RVV detail A/B | 当前不是 RVV-family-selection（RVV 实现族选择）；用户偏好为 public Std/RVV 有收益即可采纳，因此不需要额外 RVV-vs-RVV detail A/B。 |

## Optimization Matrix 更新

| candidate family | row source policy | point type / Scalar / layout | test | board / asm / doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- |
| production RVV count | direct indexed source + normal | `PointXYZ/XYZI/RGB/RGBA + pcl::Normal`，float AoS | Std/RVV 8+8 gtest passed | Phase060 count medians `3.332x` / `3.358x` / `3.308x` / `3.411x`；asm RVV 指令可见；Doctor 0/0/0 | adopted | none in current scope |
| production RVV select | same | same | Std/RVV 8+8 gtest passed | Phase060 select medians `3.039x` / `2.900x` / `2.973x` / `2.906x`；`vcompress.vm` 可见；Doctor 0/0/0 | adopted | none in current scope |
| production RVV getDistances | same | same | Std/RVV 8+8 gtest passed | Phase060 getDistances medians `4.456x` / `4.157x` / `4.290x` / `4.161x`；`vfwcvt` / `vse64.v` 可见；Doctor 0/0/0 | adopted | none in current scope |
| 其它 normal 点型 / 自定义点型 / `Scalar=double` | future scope | unvalidated | not run | not run | deferred outside current scope | 用户指定范围或 profile 后另开 phase/topic |

## Doc Suite Parity Closeout

| area | current shape scan | quality bar | decision | evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | 已列 topic-local docs、常用命令和提交边界。 | production closeout 后必须指向正式 `doc-rvv` 和 Phase060。 | adopted | `README.zh.md` 已刷新。 | none |
| testing overview | 已列 correctness、bench、board repeated、doctor / registry 分类。 | 必须区分 QEMU correctness 和 board performance。 | adopted | `doc/testing-overview.zh.md` 已刷新。 | none |
| correctness tests | 已列 gtest 字典，包含 production direct 与 fallback。 | 每个 TEST 说明输入、断言和证明范围。 | adopted | `doc/correctness-tests.zh.md`。 | none |
| benchmark/evidence | 已列 Phase060 summary、manifest、Doctor 和复现命令。 | production 结论必须使用 board repeated。 | adopted | `doc/benchmark-and-evidence.zh.md` 已刷新。 | none |
| optimization evidence | 已按 candidate family 更新 adopted/historical/deferred。 | 候选取舍主归属要和 matrix 一致。 | adopted | `doc/optimization-evidence.zh.md` 已刷新。 | none |
| test support code map | 已列 production helper 与 test-only helper 边界。 | Traceability Map 能定位代码、script、output。 | adopted | `doc/test-support-code-map.zh.md` 已刷新。 | none |
| production topic doc | 新建正式长期文档。 | 只保存 adopted production 行为和证据链。 | adopted | `doc-rvv/sample_consensus/sac_model_normal_sphere-RVV.zh.md`。 | none |
| artifact tracking | 当前 topic 文档和 summary 仍是未跟踪待提交候选。 | Handoff / final 需列清提交边界。 | adopted for review | `git status --short --untracked-files=all -- <topic paths>`。 | commit phase 由用户另行授权。 |

## 阶段结论

Phase060 的 EvidenceDecision（证据决策）为 `production-adopted`。三条公开入口在四种已冻结 source 点型与
`pcl::Normal` normal cloud 上均有接入后的 5-run board positive evidence（板卡正向证据），且 correctness、
asm 和 Evidence Doctor 没有阻塞项。正式 `doc-rvv` 已创建并只采用 Phase060 production-public 数据作为性能结论。

## 继续 / 停止判断

`continue_stop_decision`：stop_for_review / ready_for_review_validity_checked。

`stop_condition_hit`：当前 phase plan、optimization matrix 和 roadmap 在已授权 production scope 内没有未阻塞的高优先级下一动作。继续扩大到其它 normal 点型、自定义点型、`Scalar=double`、真实 workload/profile、identity-index 专门路径或其它硬件都会改变当前范围，需要新的 phase 或用户点名范围。

`next_phase_default`：当前 topic 停在 reviewer / 用户检查点；默认下一步是审查 production diff、Phase060 证据和正式 `doc-rvv`，然后由用户决定是否进入 commit phase（提交阶段）或另开扩展 topic。
