# sac_model_normal_sphere 优化证据索引

## 本文职责

本文按 candidate family（候选实现族）索引已经发生的证据和取舍。跨 phase 的未来搜索空间归 `doc/optimization-roadmap.zh.md`；阶段事实归 `doc/phases/*/result.zh.md`。

## 当前结论摘要

当前状态是 `production-adopted`。Phase060 在接入后的 production-public（真实公开入口）边界下完成
四种 source 点型 × 三入口 5-run board repeated（重复板卡测试），12/12 comparison 全部 positive，
Evidence Doctor（证据体检）为 `Errors=0`、`Warnings=0`、`Suggestions=0`。Phase000-030 的测试专用
production-shaped diagnostic（生产形态诊断）只作为历史候选输入，不作为当前生产性能 truth。

## 优化方式总表

| candidate family | 代码路径 | 测试路径 | bench / board evidence | asm evidence | decision | 边界 |
| --- | --- | --- | --- | --- | --- | --- |
| production RVV count | production：`sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp`；tests：`include/impl/sac_model_normal_sphere_access.hpp` | `run_test_compare`、`ProductionRVVDetailHelpersMatchPublicReference` | Phase060：PointXYZ `3.332x`，PointXYZI `3.358x`，PointXYZRGB `3.308x`，PointXYZRGBA `3.411x` | production helper 可见 `vfsqrt.v` / `vcpop.m` | adopted | 只覆盖当前 direct indexed source/normal 与 `pcl::Normal`。 |
| production RVV select `vcompress` writeback | 同上 | `run_test_compare`、select 顺序与 error 对拍 | Phase060：PointXYZ `3.039x`，PointXYZI `2.900x`，PointXYZRGB `2.973x`，PointXYZRGBA `2.906x` | `vcompress.vm`、`vfwcvt.f.f.v`、`vse64.v` 可见 | adopted | 输出顺序由 gtest 与 checksum 保护；不外推到其它 normal 点型。 |
| production RVV getDistances dense output | 同上 | `run_test_compare`、dense output 对拍 | Phase060：PointXYZ `4.456x`，PointXYZI `4.157x`，PointXYZRGB `4.290x`，PointXYZRGBA `4.161x` | `vfwcvt.f.f.v` / `vse64.v` 可见 | adopted | 没有 early continue，但接入后板卡仍稳定正向。 |
| Phase000-030 diagnostic helpers | `include/impl/sac_model_normal_sphere_access.hpp` | historical regression in `run_test_compare` | Phase000-030 single board smoke positive but `low_run_count` | 候选 helper 可见 RVV 指令 | historical | 只说明为何进入 production probe，不作为最终收益。 |
| 其它 normal 点型 / 自定义点型 / `Scalar=double` | not implemented | not run | not run | not run | deferred outside current scope | 需要用户指定范围或 profile 后另开 phase。 |

## 标量路径与 RVV 路径差异

标量 public entry 按 `indices_` 单点读取 source 和 normal，先计算球壳距离，再在 select/count 中用 early continue 决定是否计算 normal angle。RVV candidate 在 VL chunk（可变向量长度分块）内 gather source xyz 和 normal xyz，计算 `n_dir`、`sqrt`、normal angle、mask、`vcpop` 或 `vcompress`，再按入口要求写回 count、inliers、error 或 dense distances。

公开入口当前已有 dispatch（分流逻辑）。RVV 构建且 gate 命中时 public 行进入 production RVV helper；Std 构建或
gate 不满足时进入 Standard helper。Phase060 的 public 行是当前生产性能结论来源，diagnostic candidate 行只保留为历史候选回归。

## 代码级证据索引

| 对象 | 层级 | 证据角色 | 路径 |
| --- | --- | --- | --- |
| `*StandardNormalSphere` / `*RVVNormalSphere` helpers | production Std / RVV helper | 当前 adopted production 行为 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp` |
| `SampleConsensusModelNormalSphereAccess` | diagnostic reference / candidate | 标量参考和 RVV 候选主入口 | `include/impl/sac_model_normal_sphere_access.hpp` |
| `makeNormalSphereCloud` / `makeBenchCloud` | fixture / bench input | 点型 layout 覆盖 | `include/test_sac_model_normal_sphere.h`、`include/bench_sac_model_normal_sphere.h` |
| `generate_normal_sphere_evidence_manifest.py` | analysis script | summary / manifest 生成 | `script/generate_normal_sphere_evidence_manifest.py` |
| Phase060 `board-evidence-summary.md` | evidence output summary | production-public repeated board 摘要 | `doc/phases/060-production-integration-execution/board-evidence-summary.md` |

## 当前可提交证据

summary、manifest、Evidence Doctor 和 registry 可以作为 summary-only（只提交摘要）证据候选；raw board logs 默认本地保留。
production 长期文档为 `doc-rvv/sample_consensus/sac_model_normal_sphere-RVV.zh.md`，只保存接入后 adopted production 行为和 Phase060 证据。

## 结论边界

当前已采纳 Phase050 冻结的三入口、点型 / layout、fallback 和 PI2-PI5 evidence plan。被 deferred（暂缓）的范围是
其它 normal 点型、自定义点型、`Scalar=double`、identity-index 专门路径、真实 workload 和其它硬件；这些都需要新 phase 或 follow-up topic。
