# recognition/occlusion_reasoning RVV 主题

本目录保存 `recognition/include/pcl/recognition/impl/hv/occlusion_reasoning.hpp`
中 `ZBuffering<ModelT, SceneT>` 路径的 RVV（RISC-V Vector，可变长度向量扩展）
专项测试资产。当前 topic token（主题短标识）为 `occlusion_reasoning`。

## 当前状态

- 当前 phase：`020-inline-filter-production-integration` 已完成。
- production 状态：已采纳 public inline `filter(scene, model, f, threshold)` 与
  `getOccludedCloud(scene, model, f, threshold)` 的 production direct 分流。`__RVV10__`
  构建且 `ModelT` 满足 `RVVXYZAoSFloatLayout<ModelT>` 时进入 RVV helper；非 RVV 构建、
  非 dense 输入、非 xyz AoS 布局、空 `depth_` 或超出 32-bit lane index（向量通道下标）
  表达范围时回退标量。
- 当前证据：`log/board/repeated_phase020_inline_filter_production_direct/summary.md`
  显示 5-run board median `1.250x`、min/max `1.240x/1.260x`、`B/A < 1 = 0/5`，
  checksum 一致。Evidence Doctor（证据体检）为 `Errors=0 / Warnings=0 / Suggestions=1`。
- `doc-rvv` 状态：正式 production 长期文档为
  `doc-rvv/recognition/occlusion_reasoning-RVV.zh.md`。

## 常用命令

```bash
make -C test-rvv/recognition/occlusion_reasoning run_test_compare
make -C test-rvv/recognition/occlusion_reasoning run_qemu_smoke
make -C test-rvv/recognition/occlusion_reasoning check_inline_filter_rvv_asm
make -C test-rvv/recognition/occlusion_reasoning check_occlusion_filter_rvv_asm
make -C test-rvv/recognition/occlusion_reasoning inline_board_repeated
make -C test-rvv/recognition/occlusion_reasoning record_inline_evidence_state_repeated
make -C test-rvv/recognition/occlusion_reasoning board_repeated
make -C test-rvv/recognition/occlusion_reasoning evidence_doctor_repeated
make -C test-rvv/recognition/occlusion_reasoning record_evidence_state_repeated
make -C test-rvv/recognition/occlusion_reasoning check_evidence_freshness
```

QEMU（仿真器）只用于 correctness（正确性）、构建和日志形状；性能结论只来自
board（板卡）或目标硬件。若当前 shell 没有继承 ssh-agent，可用本机有效
`SSH_AUTH_SOCK` 注入板卡命令。

## 文档入口

- 函数级评估：`doc/occlusion_reasoning-evaluation.zh.md`
- 测试总览：`doc/testing-overview.zh.md`
- 正确性测试字典：`doc/correctness-tests.zh.md`
- bench 与证据：`doc/benchmark-and-evidence.zh.md`
- 优化证据索引：`doc/optimization-evidence.zh.md`
- 测试支撑代码地图：`doc/test-support-code-map.zh.md`
- 优化路线图：`doc/optimization-roadmap.zh.md`
- 阶段索引：`doc/phases/README.zh.md`
- 优化矩阵：`doc/phases/optimization-matrix.zh.md`
- Phase 020 result：`doc/phases/020-inline-filter-production-integration/result.zh.md`
- Phase 010 result：`doc/phases/010-production-integration/result.zh.md`
- production 长期文档：`doc-rvv/recognition/occlusion_reasoning-RVV.zh.md`
