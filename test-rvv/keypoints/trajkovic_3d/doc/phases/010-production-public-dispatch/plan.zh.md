# Phase 010 Plan: production public dispatch

本阶段把 Phase 000 的 FOUR_CORNERS response map（四邻域响应图）正向板卡信号推进到
production public（真实公开入口）边界。阶段只验证 `TrajkovicKeypoint3D::compute()` 在
`PointInT=pcl::PointXYZ`、`PointOutT=pcl::PointXYZI`、`NormalT=pcl::Normal`、预计算法线、
`method_=FOUR_CORNERS`、`window_size_=3`、organized full cloud（有组织完整点云）条件下的
RVV dispatch（RVV 分流逻辑），不扩大到 EIGHT_CORNERS、normal estimation（法线估计）、
indices subset（索引子集）或其它点类型组合。

## 当前状态

- Phase 000 response-only diagnostic（只测响应图的诊断）板卡 repeated summary 为 positive：
  `four_corners_320x240` 3.596x、`four_corners_invalid_320x240` 2.317x、
  `four_corners_641x481_tail` 3.396x。
- QEMU（仿真器）公开入口 gtest 已覆盖 Std/RVV 两侧各 5 个测试，包含 production gate
  和 public `compute()` 输出强度对拍。
- production patch 已在 `keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp` 中加入窄范围
  RVV response helper 和 `detectKeypoints()` 分流。

## 范围和不覆盖项

| area | 本阶段验证 | 不验证 / 保持 fallback |
| --- | --- | --- |
| public entry | `Detector::compute(output)`，内部命中 `detectKeypoints()` | 直接调用测试 helper 不作为 production 证据 |
| method / window | `FOUR_CORNERS` + `window_size_=3` | `EIGHT_CORNERS`、其它 window size |
| point / normal | `PointXYZ` 输入 + `PointXYZI` 输出 + `pcl::Normal` 法线，float AoS layout（结构数组布局） | 非 float layout、未注册 xyz / normal 字段或 traits gate 不满足的模板实例 |
| data shape | organized full cloud，`indices_->size()==input_->size()` | indices subset、非 organized 输入、correspondence（对应关系）路径 |
| timer boundary | 真实 `compute()`，包含 response map、NMS（非极大值抑制）和输出构造；使用预计算 normals 排除 normal estimation | 不证明 normal estimation 被加速 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | 本阶段必须生成 `production_public`。Phase 000 的 `production_shaped_diagnostic` 只作为进入本阶段的理由。 |
| A/B boundary | `public_compute`，Std/RVV 两个二进制都调用真实 `Detector::compute(output)`。 |
| 当前决策问题 | `RVV-vs-scalar`：当前公开入口 RVV 分流是否快于同边界标量构建。 |
| diagnostic 是否可外推到 production | Phase 000 不直接外推；本阶段用 public bench 重新证明。 |
| comparison-boundary / baseline mismatch 风险 | 仍有 NMS 和输出构造稀释风险，所以本阶段计时必须包含它们。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 若 public board 不 positive，则不能采纳当前 production patch；需要停下整理或另开 EIGHT_CORNERS / NMS phase。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有已有 Trajkovic 3D RVV family，不需要 RVV-vs-RVV；需要 production public Std/RVV repeated board。 |

## 动作和完成判据

| action | artifact / command | completion |
| --- | --- | --- |
| public bench mode | `src/bench_trajkovic_3d.cpp --mode public` | Std/RVV 构建都能输出同名 public case 和 checksum |
| QEMU correctness | `make -C test-rvv/keypoints/trajkovic_3d run_test_compare` | Std/RVV gtest 通过，QEMU 只作为 correctness/log-shape |
| asm gate | `make -C test-rvv/keypoints/trajkovic_3d check_trajkovic_3d_rvv_asm` | RVV 指令仍可归属到当前 helper |
| board repeated | `make -C test-rvv/keypoints/trajkovic_3d board_repeated_production record_evidence_state_production` | 5-run repeated summary、Evidence Doctor 和 registry 刷新 |
| docs | phase result、matrix、roadmap、evaluation、`doc-rvv/keypoints/trajkovic_3d-RVV.zh.md` | public board positive 时按用户授权采纳并写正式长期文档 |

## 决策桶和复跑预算

本阶段默认 5-run repeated board，`median >= 1.20x` 且没有 run 低于 1.0x 判为 positive；
`1.05x <= median < 1.20x` 判为 weak-positive，需要结合实现复杂度；`0.95x <= median < 1.05x`
判为 neutral；`median < 0.95x` 判为 negative；方向摇摆判为 unstable。若 positive 且
Evidence Doctor 无 Error，用户本轮已授权“板卡 production 测试有收益即可采纳”，可进入
production closeout 和 `doc-rvv` 创建。

## 下一阶段默认入口

若 public production evidence positive：更新 Phase 010 result、optimization matrix、roadmap、
topic-local evaluation、README 和长期 `doc-rvv`，再检查是否还有值得继续的 EIGHT_CORNERS
或泛型点类型扩展方向。若 public evidence 非 positive：保留证据并停下说明当前 production patch
不建议采纳的原因；未经用户后续授权不自行回滚。
