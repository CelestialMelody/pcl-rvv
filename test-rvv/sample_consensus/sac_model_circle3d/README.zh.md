# sac_model_circle3d RVV Topic

本 topic 覆盖 `SampleConsensusModelCircle3D<PointT>` 的 3D 圆投影距离核。当前默认入口是 `doc/phases/010-selectwithin-production-probe/result.zh.md` 和 `doc/phases/020-select-point-type-expansion/result.zh.md`，因为生产接入后的板卡证据已经覆盖早期 diagnostic（诊断）结论。

## 当前状态

| 项目 | 状态 |
| --- | --- |
| production 源码 | `selectWithinDistance` RVV 补丁已回滚；当前源码恢复为标量路径。 |
| 首阶段目标 | count/select 的投影距离核 test-only candidate（仅测试使用候选）已完成诊断；count 负向，select 进入过 production probe。 |
| 当前 EvidenceDecision | `rollback/no-production`；`selectWithinDistance` production patch 已撤回，`PointXYZI/RGB/RGBA` 扩展均 rejected。 |
| 默认恢复动作 | 当前 topic 已收口；若未来重启，先写新的 phase plan，不再沿用本轮 production patch。 |
| `doc-rvv` 长期主题文档 | 当前不适用；没有 adopted production behavior（已采用生产行为）或用户确认保留的生产补丁。 |

## 常用命令

```bash
make -C test-rvv/sample_consensus/sac_model_circle3d run_circle3d_phase000_tests
make -C test-rvv/sample_consensus/sac_model_circle3d run_test_compare
make -C test-rvv/sample_consensus/sac_model_circle3d check_projection_asm
make -C test-rvv/sample_consensus/sac_model_circle3d record_projection_board_evidence_state
make -C test-rvv/sample_consensus/sac_model_circle3d projection_evidence_status
```

QEMU 只证明 correctness（正确性）和路径，不证明性能。当前性能摘要主路径：

- `test-rvv/sample_consensus/sac_model_circle3d/doc/phases/000-circle3d-projection-component-ablation/projection-repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_circle3d/doc/phases/000-circle3d-projection-component-ablation/projection-repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_circle3d/doc/phases/000-circle3d-projection-component-ablation/projection-repeated-evidence-doctor.json`
- `test-rvv/sample_consensus/sac_model_circle3d/doc/phases/010-selectwithin-production-probe/select-production-repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_circle3d/doc/phases/010-selectwithin-production-probe/select-production-repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_circle3d/doc/phases/020-select-point-type-expansion/select-*-repeated-evidence-manifest.json`
