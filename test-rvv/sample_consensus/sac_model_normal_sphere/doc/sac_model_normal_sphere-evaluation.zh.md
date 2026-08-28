# sac_model_normal_sphere 函数级评估

## 当前生产判断

当前 EvidenceDecision（证据决策）是 `production-adopted`。`SampleConsensusModelNormalSphere<PointT, PointNT>` 的
`selectWithinDistance`、`countWithinDistance` 和 `getDistancesToModel` 已接入 production RVV（生产 RVV）
helper。Phase060 使用接入后的真实 public entry（公开入口）Std/RVV 5-run board repeated（重复板卡测试）：
四种 source 点型 × 三入口共 12 项 comparison 全部 positive，Evidence Doctor（证据体检）为 `Errors=0`、
`Warnings=0`、`Suggestions=0`。

当前采纳范围是 direct indexed `indices_`、`PointXYZ` / `PointXYZI` / `PointXYZRGB` / `PointXYZRGBA`
source 点型、独立 `pcl::Normal` normal cloud、float xyz / normal AoS（结构数组）布局、signed 32-bit
`pcl::index_t`、u32 byte offset（32 位字节偏移）规模 gate 和 `indices_.size() >= 16`。其它 normal 点型、
自定义 registered point type（已注册自定义点型）、`Scalar=double`、其它 layout、非法 index、真实 workload（工作负载）
和其它硬件需要单独证据。

## S2 函数级评估

`SampleConsensusModelNormalSphere<PointT, PointNT>` 有三个公开距离入口：`selectWithinDistance`、`countWithinDistance`
和 `getDistancesToModel`。三者都先检查 `normals_`、`isModelValid` 和模型系数，然后按 `indices_` 遍历点云与法线云。
每个样本计算 `n_dir = p - center`，再组合球壳距离和法线夹角：

```text
weighted_euclid = (1 - normal_distance_weight) * abs(norm(n_dir) - radius)
d_normal = min(abs(getAngle3D(normal, n_dir)), pi - abs(getAngle3D(normal, n_dir)))
distance = abs(normal_distance_weight * d_normal + weighted_euclid)
```

`selectWithinDistance` 和 `countWithinDistance` 有 early continue（提前跳过）：当 `weighted_euclid > threshold`
时不再计算法线夹角。`getDistancesToModel` 没有 early continue，会为每个 `indices_` 写回一个 double distance
（双精度距离）。早期评估判断是先做 production-shaped diagnostic（生产形态诊断），因为 sphere 只能证明球壳 mask，
normal-plane 只能证明固定平面法线的角度 helper；normal-sphere 的 `n_dir` 每点变化，必须单独验证。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `SampleConsensusModelNormalSphere::selectWithinDistance` | production public entry | 检查模型后按 RVV gate 分流，写回 inliers 和 `error_sqr_dists_`。 | RANSAC normal-sphere 模型 | `selectWithinDistanceRVVNormalSphere` 或 Standard helper | production boundary（生产边界） | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp` |
| `SampleConsensusModelNormalSphere::countWithinDistance` | production public entry | 检查模型后按 RVV gate 分流并返回内点数。 | RANSAC normal-sphere 模型 | `countWithinDistanceRVVNormalSphere` 或 Standard helper | production boundary | 同上 |
| `SampleConsensusModelNormalSphere::getDistancesToModel` | production public entry | 检查模型后按 RVV gate 分流，输出 dense double distances。 | RANSAC normal-sphere 模型 | `getDistancesToModelRVVNormalSphere` 或 Standard helper | production boundary | 同上 |
| `*StandardNormalSphere` helpers | production Std helper | 保存原标量 fallback（回退路径）语义。 | public entries | 标量 Eigen / `getAngle3D` | fallback coverage（回退覆盖） | 同上 |
| `*RVVNormalSphere` helpers | production RVV helper | 执行 indexed gather、球壳 + normal angle 距离核、mask / compress / dense store。 | public entries | `computeNormalSphereDistanceRVV` | production direct / asm attribution（反汇编归属） | 同上 |
| `RVVNormalSphereLayoutGate` / `canUseRVVNormalSphere` | production dispatch / fallback | 限定 source layout、`pcl::Normal`、index 类型、规模和小输入。 | public entries / gtest | RVV helper 或 Standard fallback | fallback coverage | 同上 |
| `SampleConsensusModelNormalSphereAccess` | diagnostic reference / candidate | 测试专用派生类，暴露 scalar same-chain（同构标量链路）、历史 candidate 和 production helper direct checks。 | gtest 和 bench | topic-local helper / production helper | correctness gate（正确性验收）和 historical diagnostic | `test-rvv/sample_consensus/sac_model_normal_sphere/include/impl/sac_model_normal_sphere_access.hpp` |
| `src/test_sac_model_normal_sphere.cpp` | correctness test | 覆盖边界点、乱序 indices、四种 source 点型、production helper hit 和 fallback。 | `make run_test_compare` | gtest | QEMU correctness（QEMU 正确性） | `test-rvv/sample_consensus/sac_model_normal_sphere/src/test_sac_model_normal_sphere.cpp` |
| `src/bench_sac_model_normal_sphere.cpp` | bench wrapper | 输出 public 和 historical diagnostic candidate 的计时、checksum 与输入标签。 | board / QEMU smoke | `bench_sac_model_normal_sphere.h` | board performance input（板卡性能输入）；QEMU 不作为性能结论 | `test-rvv/sample_consensus/sac_model_normal_sphere/src/bench_sac_model_normal_sphere.cpp` |
| `generate_normal_sphere_evidence_manifest.py` | analysis script | 把 board logs 转成 summary、manifest 和 Evidence Doctor 输入。 | Makefile evidence target | phase docs / registry | evidence summary | `test-rvv/sample_consensus/sac_model_normal_sphere/script/generate_normal_sphere_evidence_manifest.py` |
| Phase060 summary / manifest / Doctor | evidence output summary | 保存 production-public 5-run board repeated 和 0/0/0 体检结果。 | `run_phase060_evidence_doctor` | evaluation / `doc-rvv` / Handoff | EvidenceDecision input | `test-rvv/sample_consensus/sac_model_normal_sphere/doc/phases/060-production-integration-execution/` |
| `doc-rvv/sample_consensus/sac_model_normal_sphere-RVV.zh.md` | production long-term doc | 保存当前 adopted production 行为、fallback、证据链和后续范围。 | README / reviewer | production 源码和 Phase060 evidence | long-term production truth | `doc-rvv/sample_consensus/sac_model_normal_sphere-RVV.zh.md` |

## 文档归属矩阵

| 信息类型 | 主归属 | 当前状态 |
| --- | --- | --- |
| 当前采用的生产优化方式、覆盖范围、fallback 和生产边界 | `doc-rvv/sample_consensus/sac_model_normal_sphere-RVV.zh.md` | 已创建，使用 Phase060 production-public 数据。 |
| 函数职责、标量路径、候选取舍和诊断到生产的决策审计 | 本 evaluation | 已刷新为当前 production-adopted 状态。 |
| 阶段计划、矩阵、继续 / 停止条件 | `doc/phases/` | Phase000-060 均有 plan/result；matrix 已更新。 |
| bench 数值、Evidence Doctor 和 registry | Phase result / manifest / Doctor / registry | Phase060 是当前性能 truth；Phase000-030 是历史诊断。 |
| topic-local doc suite | `README.zh.md` 与 `doc/*.zh.md` role 文档 | 已覆盖 testing overview、correctness、benchmark/evidence、optimization evidence 和 test-support code map。 |

## 生产接入证据链

Phase060 完成 production direct/public 证据闭环。summary 位于
`doc/phases/060-production-integration-execution/board-evidence-summary.md`，manifest 位于
`doc/phases/060-production-integration-execution/board-evidence-manifest.json`，Evidence Doctor 位于
`doc/phases/060-production-integration-execution/board-evidence-doctor.md`。

| point type | select | count | getDistances | 证据角色 |
| --- | ---: | ---: | ---: | --- |
| `PointXYZ + Normal` | `3.039x` | `3.332x` | `4.456x` | production-public |
| `PointXYZI + Normal` | `2.900x` | `3.358x` | `4.157x` | production-public |
| `PointXYZRGB + Normal` | `2.973x` | `3.308x` | `4.290x` | production-public |
| `PointXYZRGBA + Normal` | `2.906x` | `3.411x` | `4.161x` | production-public |

`run_test_compare` 在 Std/RVV 两个构建中各 8 个 gtest 通过；`dump_bench_rvv` 的反汇编抽查可见
`computeNormalSphereDistanceRVV`、`vfsqrt.v`、`vcpop.m`、`vcompress.vm`、`vfwcvt.f.f.v` 和 `vse64.v`。
QEMU（仿真器）只作为 correctness、构建和路径证据，性能结论只来自板卡。

## Diagnostic 到 Production Mismatch Audit

| question | result |
| --- | --- |
| evidence role | 当前结论使用 Phase060 `production_public`。Phase000-030 是 historical diagnostic（历史诊断）。 |
| A/B boundary | board Std build public overload vs board RVV build public overload after production dispatch integration。 |
| 当前决策问题 | 当前 public RVV path 是否快于当前 public scalar path，并按用户偏好是否采纳。 |
| diagnostic 是否可外推到 production | 不再外推；production direct/public 证据已重跑。 |
| comparison-boundary / baseline mismatch 风险 | Phase060 manifest 记录相同 wrapper、row source、checksum policy 和 timer boundary；checksum 一致。 |
| weak / negative / neutral / unstable 时是否允许 bounded production probe | 不适用；Phase060 12/12 positive。 |
| clean adoption 是否需要同一 production boundary RVV-vs-RVV detail A/B | 当前不是 RVV-family-selection（RVV 实现族选择）；用户偏好为 public Std/RVV 有收益即可采纳。 |

## 历史诊断证据链

Phase000-030 的 production-shaped diagnostic 和 component ablation（组件消融）证明候选值得进入 production probe，
但它们都是 single board smoke（单次板卡小型验证）或测试专用边界，不能替代 Phase060。

- Phase000：count/select 初筛正向，Evidence Doctor Warning 为 `low_run_count`。
- Phase010：`vcompress` select 写回实现族正向，Evidence Doctor Warning 为 `low_run_count`。
- Phase020：`getDistancesToModel` dense-store 候选正向，Evidence Doctor Warning 为 `low_run_count`。
- Phase030：RGB/RGBA source layout 诊断正向，Evidence Doctor Warning 为 `low_run_count`。

## 未覆盖范围与后续条件

其它 normal 点型、自定义 source 点型、identity-index 专门路径、`Scalar=double`、真实 workload/profile 和其它硬件均未由当前证据关闭。
这些方向不建议在当前 topic 默认继续推进，因为已采纳范围内收益稳定且文档 / 证据闭环已闭合；继续会改变 scope。
若用户明确指定其中任一方向，应新建窄 phase，重新补 fallback、correctness、asm、board repeated 和 Evidence Doctor。
