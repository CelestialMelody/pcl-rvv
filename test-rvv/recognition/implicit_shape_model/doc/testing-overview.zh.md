# Testing Overview

本文说明 `implicit_shape_model` topic 的测试入口和证据边界，不替代 correctness 测试字典、
benchmark/evidence 文档或 phase result。

## 运行入口分类

| target | 类别 | 证明什么 | 不证明什么 |
| --- | --- | --- | --- |
| `run_test_compare` | correctness aggregate（正确性汇总入口） | Std/RVV 局部公式、production helper 和 fallback 形态输出一致 | 完整 production 性能 |
| `run_qemu_smoke` | QEMU smoke（仿真小型验证） | QEMU 下可运行和日志形状 | 真实性能 |
| `run_upstream_test_compare` | production direct correctness（真实生产路径正确性） | 上游 `test_recognition_ism.cpp` 在 Std/RVV 构建下通过 | 板卡性能和所有模板实例 |
| `check_ism_rvv_asm` | asm gate（反汇编验收） | RVV build 出现预期 load / stride-load / reduction 指令 | 指令吞吐或入口占比 |
| `board_repeated` | diagnostic board repeated | 目标硬件上局部公式或 production-shaped case 的 5-run 性能信号 | 不能自动采纳 production |
| `public_entry_board_repeated` | production direct board repeated | 真实 `findObjects()` public-entry bench 的 5-run 性能信号 | 不证明 `trainISM()`、sigma、density |
| `record_evidence_state_repeated` | diagnostic registry | 登记 Phase 000/010 summary、manifest、Doctor | raw log 可提交性 |
| `record_evidence_state_public_entry` | production registry | 登记 Phase 020 production direct summary、manifest、Doctor | raw log 可提交性 |
| `check_evidence_freshness` / `check_public_entry_evidence_freshness` | freshness check（新鲜度检查） | 检查摘要证据和文档引用是否一致 | 不替代重新 benchmark |

## Target 粒度审计

| target 类别 | 当前状态 | evidence |
| --- | --- | --- |
| correctness aggregate | adopted | `run_test_compare` |
| correctness aliases | adopted | `run_upstream_test_compare` 覆盖真实公开入口；gtest 名称逐项区分 helper |
| bench diagnostic aliases | adopted | `--case-filter descriptor_cluster_distance,descriptor_batch_assignment,sigma_pairwise_max_dot,vote_density_gaussian_sum` |
| QEMU smoke aliases | adopted | `run_qemu_smoke` 只是 correctness 别名；public-entry bench 只在极小规模 debug 时用于可运行性 |
| board smoke aliases | not_applicable with evidence | 本 topic 直接使用 repeated 采集；单次 smoke 不作为性能结论 |
| board repeated aliases | adopted | `board_repeated`、`public_entry_board_repeated` |
| doctor / registry aliases | adopted | `evidence_doctor_repeated`、`record_evidence_state_repeated`、`evidence_doctor_public_entry`、`record_evidence_state_public_entry` |
| historical probe guarded aliases | not_applicable with evidence | 当前没有保留的历史 production probe target；Phase 000/010 是历史诊断证据 |

## 测试流程

推荐 production closeout 顺序是：

```bash
make -C test-rvv/recognition/implicit_shape_model run_test_compare
make -C test-rvv/recognition/implicit_shape_model run_upstream_test_compare
make -C test-rvv/recognition/implicit_shape_model check_ism_rvv_asm
SSH_AUTH_SOCK=<agent-forwarded-sock> make -C test-rvv/recognition/implicit_shape_model public_entry_board_repeated
make -C test-rvv/recognition/implicit_shape_model record_evidence_state_public_entry
make -C test-rvv/recognition/implicit_shape_model check_public_entry_evidence_freshness
```

QEMU 只看正确性、构建和日志形状；板卡 repeated 才能支撑性能方向。

## 输入数据总览

Phase 020 public-entry bench 使用 deterministic model、`pcl::PointXYZ` cloud、`pcl::Normal`
normal cloud 和测试专用 `SyntheticIsmFeature`。该 feature estimator 设置 `KSearch=1`，避免
`FeatureFromNormals::initCompute()` 因未配置 K / radius 提前失败。bench case 不包含文件 I/O 和训练计时；
它测量 `findObjects()` 公开入口和其中的 descriptor assignment production helper。
