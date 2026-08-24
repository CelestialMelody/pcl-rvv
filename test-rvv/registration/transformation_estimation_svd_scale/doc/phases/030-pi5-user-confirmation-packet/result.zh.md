# Phase 030 结果：PI5 user confirmation packet

## 执行摘要

本阶段只整理 PI5 用户确认包，没有修改 production 源码，没有把当前 patch 标为 adopted，也没有创建 `artifact_layout.topic_doc_template` 解析出的 production 长期主题文档。当前 EvidenceDecision 仍是：

`production_patch_positive_pending_user_confirmation`

Phase 010 的生产证据支持“建议采纳当前有界 production patch”。本阶段又完成了一次预采纳源码审计和当前 QEMU correctness rerun（正确性重跑）：Std/RVV 两侧 7 tests 均通过，registry 已重新登记为 fresh。由于 PI5 是人工确认点，下一步仍需要用户明确选择采纳、回滚，或授权在 pending 状态下继续 diagnostic 候选。

## 当前 production patch 范围

| 文件 | 当前 production 变化 | 审查重点 |
| --- | --- | --- |
| `registration/include/pcl/registration/transformation_estimation_svd_scale.h` | `TransformationEstimationSVDScale` 新增 ordered cloud-pair public overload override，并用 `using TransformationEstimationSVD<...>::estimateRigidTransformation` 保留父类 overload set。 | 不隐藏 source-indexed、dual-indexed 或 correspondence 入口；公开函数形状仍是已有 override。 |
| `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` | RVV 构建下新增 fused accumulation helper、scale solve helper 和 ordered public overload dispatch。 | RVV 分支只覆盖窄范围；失败显式回父类 ordered overload。 |

当前 RVV production path 的命中条件：

- RVV 构建，即 `__RVV10__` 可用。
- `Scalar=float`。
- source / target 都满足 layout-gated xyz AoS float layout。
- ordered source cloud 和 target cloud 等长。
- source / target 都是 dense。
- `nr_points >= 16`。
- source variance 非退化且 scale 解有限。

明确保持 fallback 的范围：

- 非 RVV 构建。
- `Scalar=double` 或其它非 float 实例。
- 小规模、非 dense、等长检查失败或 source variance 退化。
- source-indexed、dual-indexed 和 correspondence 公开入口。
- 当前没有 direct evidence（直接证据）的泛型点型组合。

## 可复现证据

| evidence | command / path | 当前结果 | 证据角色 |
| --- | --- | --- | --- |
| correctness（正确性） | `make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare` | 本阶段重新执行，Std/RVV 7 tests passed。 | 覆盖 public scale、fallback boundaries、diagnostic candidate 和 matrix-local equivalence。 |
| QEMU public-scale smoke（小型验证） | Phase 010 result；historical registered public-scale doctor | `Errors=0`、`Warnings=0`；当前裸 `log/qemu/evidence_doctor.md` 已被 Phase 020 matrix-local smoke 覆盖。 | 只证明日志形状 / manifest / 路径，不作为性能结论。 |
| ASM attribution（反汇编归属） | `build/asm/riscv/bench_transformation_estimation_svd_scale_rvv.full.asm` | production ordered overload 符号内可见 `vlsseg3e32.v`、`vfmacc.vv`、`vfredosum.vs`、`vsetvli`，并调用 `solveTransformationEstimationSVDScaleF32`。 | 证明 public production path 中存在 RVV load / FMA / reduction 指令。 |
| board production repeated（板卡重复采集） | `log/board/production_public_scale_ordered_cloud_pair_repeated/summary.md` | 4K/64K/256K median B/A = `26.123x` / `33.860x` / `33.066x`，overall positive。 | production direct performance（真实生产路径性能）。 |
| Evidence Doctor | `log/board/production_public_scale_ordered_cloud_pair_repeated/evidence_doctor.md` | `Errors=0`、`Warnings=1`、`Suggestions=0`。 | warning 为 4K group_outlier；当前按 size 分开报告，不把其它 size 的收益外推到 4K。 |
| registry freshness | `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_correctness_state`；`make -C test-rvv/registration/transformation_estimation_svd_scale evidence_status` | QEMU correctness run label 为 `qemu-tesvd-scale-correctness-current`；registry fresh。 | 确认已引用 evidence 没有 stale / unregistered change。 |

## 预采纳源码审计

| check | result |
| --- | --- |
| public overload visibility | pass；scale 子类新增 ordered override，同时用 `using TransformationEstimationSVD<...>::estimateRigidTransformation` 保留父类 indexed / correspondence overload set。 |
| production dispatch shape | pass；公开入口只做一次 RVV helper 尝试，失败后显式回父类 ordered overload，入口没有混入大段标量主体。 |
| layout / traits gate | pass；source 和 target 分别使用 `pcl::rvv::RVVXYZAoSFloatLayout`，不是 exact `PointXYZ` gate。当前 direct evidence 仍只覆盖 `PointXYZ -> PointXYZ`，泛型点型扩展保持独立队列。 |
| load wrapper | pass；RVV 主路径复用 `pcl::rvv_load::strided_load3_f32m2`，由公共 wrapper 选择 segment / fields load。 |
| fallback gate | pass；非 RVV、非 float、不满足 xyz AoS layout、size mismatch、非 dense、小规模和退化 source variance 都回父类路径。 |
| uncovered public entries | pass；source-indexed、dual-indexed 和 correspondence 入口未 override，当前 correctness 测试保持这些入口正确。 |
| blocking issue | none found in this audit；仍保留 dense-but-invalid-input 语义为 PCL `is_dense` 合同范围，不额外扫描 NaN / Inf。 |

## 风险和边界解释

- 4K board warning 是组内偏离，不是 correctness error。4K 自身 5 次 B/A 最小值仍为 `25.902x`，因此当前窄范围仍是 positive，但文档不能把 64K / 256K 收益外推到 4K。
- Std/RVV checksum 在 1e-6 量化下 mismatch，符合 RVV reduction tree（规约树）改变后的预期；correctness 由 gtest 和误差阈值承担。
- 当前 public production evidence 只回答“这个 bounded RVV path 是否快于当前 public scalar path”，不关闭 generic point type、indexed row source、correspondence 或 double。
- Phase 020 的 `matrix-local-scale-simplification` 是 implementation-shape diagnostic（实现形态诊断），不是 RVV intrinsic 或 production direct 证据，不改变本 PI5 判断。

## 用户选项

| option | 需要用户表达 | worker 下一步 |
| --- | --- | --- |
| 采纳 / 保留当前 production patch | 明确说“采纳”“保留当前 production patch”或等价表达。 | 进入 adoption closeout：把 decision 改为 adopted，创建 / 更新 `artifact_layout.topic_doc_template` 解析出的 production 长期主题文档，同步 README、evaluation、matrix、roadmap，并准备提交边界。 |
| 不采纳 / 回滚 production patch | 明确说“不采纳”“回滚当前 production patch”或等价表达。 | 另建 rollback/no-production phase；只回滚 `transformation_estimation_svd_scale.h/.hpp` 的 production patch，保留 topic-local tests / docs / evidence 作为 probe 记录。 |
| 暂不决定，但继续 diagnostic | 明确授权“PI5 pending 下继续某个 diagnostic 候选”。 | 不改变 production patch 状态；按 roadmap 新建独立 phase，例如 generic point type diagnostic、row-source diagnostic 或 matrix-local production-probe plan。 |

## 文档同步

| area | result |
| --- | --- |
| phase result | 本文件提供 PI5 确认包，作为当前默认恢复入口。 |
| phase index | 已更新为 Phase 030 确认包。 |
| optimization roadmap | 默认恢复队列仍停在 PI5；确认采纳后进入 production long-term doc closeout。 |
| optimization matrix | 不改变候选状态；`direct-fused-scale-accum` 仍是 `production_patch_positive_pending_user_confirmation`。 |
| production_topic_doc | `blocked_by_user_confirmation`；确认采纳前不创建 / 更新长期 production 文档。 |

## 下一步

当前合法停止点仍是 `PI5_user_confirmation_boundary`。建议用户采纳当前有界 production patch，范围限定为 ordered-cloud-pair / `PointXYZ -> PointXYZ` / `float` / dense xyz AoS；采纳后再进入 production closeout 和后续扩展 phase。
