# Phase 050 Plan: production closeout and doc-rvv adoption

## 阶段意图和边界

本阶段执行 PI5 后的 S11 production closeout（生产收口）：用户已经确认采纳 Phase 040 的
exact `PointNormal -> PointNormal` direct AoS RVV production patch（生产补丁），因此可以把该
patch 写成 adopted production behavior（已采纳生产行为），并创建
`doc-rvv/features/pfh-RVV.zh.md`。

本阶段只关闭以下范围：

- 入口：`pcl::PFHEstimation<PointInT, PointNT, PointOutT>::computePointPFHSignature`。
- 点类型：exact `pcl::PointNormal -> pcl::PointNormal`。
- 输出：`pcl::PFHSignature125` 语义下的 5x5x5 histogram。
- `Scalar`：float 字段；本函数接口内部 histogram 为 `Eigen::VectorXf`。
- 布局：source 和 normal cloud 都满足 `RVVXYZNormalFloatLayout` 的 AoS xyz+normal 布局。
- 运行条件：`__RVV10__`、`use_cache_ == false`、`nr_split == 5`、`indices.size() >= 4`、
  cloud / normals 的 32-bit byte offset gate 可满足。

本阶段不扩大到 `PointXYZ + Normal`、PointXYZ-like / Normal-like 泛型 traits 集合、
`PointXYZINormal`、自定义点类型、cache path、OMP path 或 `Scalar=double`。这些只能进入新的
point-type expansion phase（点类型扩展阶段），并需要独立测试和板卡证据。

## 当前状态清单

| 项 | 当前状态 | 路径 / 证据 |
| --- | --- | --- |
| 生产补丁 | 已存在，等待用户确认后可采纳；本轮用户已确认采纳。 | `features/include/pcl/features/impl/pfh.hpp` |
| correctness（正确性） | Std 3/3、RVV 4/4 通过。 | `make -B -C test-rvv/features/pfh run_test_compare` |
| asm（反汇编） | production helper 符号含 `vluxei32.v`、`vfmacc.vv`、`vfsqrt.v`、`vfdiv.vv`。 | `make -B -C test-rvv/features/pfh dump_bench_rvv` |
| board evidence（板卡证据） | 5-run positive；component mean `2.004x`，public mean `1.926x`。 | `test-rvv/features/pfh/log/board/pi2-production-direct-aos/repeated` |
| Evidence Doctor（证据体检） | `0E/0W/8S`；建议项仅为环境 metadata / binary hash。 | `test-rvv/features/pfh/log/board/pi2-production-direct-aos/repeated/evidence_doctor.md` |
| registry（证据登记表） | Phase 040 repeated summary 登记 fresh。 | `test-rvv/features/pfh/log/evidence_registry.json` |
| 长期 `doc-rvv` | 采纳前不适用；本阶段创建。 | `doc-rvv/features/pfh-RVV.zh.md` |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 040 `component_pfh_signature` 是 production-detail；`public_pfh_k` 是 production-public。 |
| A/B boundary | Std build 原标量 production path vs RVV build 同一 helper / public overload 下的 RVV dispatch。 |
| 当前决策问题 | `RVV-vs-scalar`：是否保留当前 production patch。 |
| diagnostic 是否可外推到 production | 不需要外推。采纳依据使用 Phase 040 接入后的 production direct 板卡证据；早期 diagnostic 只解释候选来源。 |
| comparison-boundary / baseline mismatch 风险 | aggregate bench 中同时含 diagnostic case；本阶段文档只用 production-detail / production-public case 做采纳结论。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不适用为拒绝条件；当前 production direct repeated 为 positive。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前 PFH 没有既有 adopted RVV family；首个 RVV-vs-scalar patch 不需要 family-selection A/B。后续若比较 staged/direct production family 需要补。 |

## 实现和文档动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| A1 更新 Phase 040 状态 | `040-direct-aos-production-probe/result.zh.md` | PI5 由 pending 改为 user-confirmed adopted，并保留采纳前后边界。 |
| A2 创建长期主题文档 | `doc-rvv/features/pfh-RVV.zh.md` | 文档包含当前采用方式、fallback 矩阵、VL chunk 数值算例、Traceability Map、证据链和后续方向。 |
| A3 更新阶段索引、矩阵、roadmap、队列表 | phase README、optimization matrix、roadmap、features queue | 状态从 “PI5 pending” 同步为 adopted；`PointXYZ + Normal` 保持下一 phase。 |
| A4 写 Phase 050 result | 本目录 `result.zh.md` | 回填动作、doc-suite 审计、Evidence Doctor / registry freshness 和继续决策。 |
| A5 准备下一 phase | `060-pointxyz-normal-production-expansion/plan.zh.md` | 如果未命中停止条件，下一阶段独立验证 `PointXYZ + Normal`。 |

## Evidence Doctor、registry 和复跑预算

本阶段不生成新的板卡数值，只消费 Phase 040 接入后的 repeated summary。freshness check 必须重新跑
registry check，确认文档引用的 Phase 040 evidence 仍登记 fresh。若 registry 或 doctor 发现 Error /
Warning，`doc-rvv` 不能写成 adopted，必须回到 Phase 040 或降级为 blocked。

本阶段不重新消耗板卡复跑预算；下一 Phase 060 若改代码，必须重新定义 `PointXYZ + Normal` 的有界板卡预算。

## 继续 / 停止条件

完成 A1-A4 后，exact `PointNormal -> PointNormal` production closeout 可以关闭。由于 roadmap 中
`pfh-pointxyz-normal-production-expansion` 仍在当前 topic 授权范围内、板卡可用、且不需要改 public API，
本轮不能以 050 完成为停止理由。默认继续进入 Phase 060。
