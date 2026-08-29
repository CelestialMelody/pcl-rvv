# recognition/quantizable_modality RVV 主题

本目录保存 `recognition/src/quantizable_modality.cpp` 的 RVV 专项测试资产。当前主题短标识为 `qm`，对应完整 topic `quantizable_modality`。

## 当前状态

- 当前 phase：`000-shared-spread-rvv`
- 当前状态：公共 `QuantizedMap::spreadQuantizedMap()` 在 `__RVV10__` 构建、`spreading_size == 8` 且图像尺寸足够时走 RVV 链路，其它情况回退标量。
- 当前证据角色：production-detail（生产细节 helper）证据。
- production 状态：`recognition/src/quantizable_modality.cpp` 已接入 RVV / scalar 分流。
- `doc-rvv` 状态：已创建 `doc-rvv/recognition/quantizable_modality-RVV.zh.md`。

## 常用命令

```bash
make -C test-rvv/recognition/quantizable_modality run_test_compare
make -C test-rvv/recognition/quantizable_modality run_qemu_smoke
make -C test-rvv/recognition/quantizable_modality dump_bench_rvv
make -C test-rvv/recognition/quantizable_modality check_qm_rvv_asm
SSH_AUTH_SOCK=<agent-forwarded-sock> make -C test-rvv/recognition/quantizable_modality board_repeated record_evidence_state_repeated
make -C test-rvv/recognition/quantizable_modality check_evidence_freshness
```

QEMU（仿真器）只用于 correctness（正确性）、构建和日志形状；性能结论必须来自 board（板卡）或目标硬件。

## 文档入口

- 函数级评估：`doc/quantizable_modality-evaluation.zh.md`
- 阶段索引：`doc/phases/README.zh.md`
- 当前 phase result：`doc/phases/000-shared-spread-rvv/result.zh.md`
- 优化矩阵：`doc/phases/optimization-matrix.zh.md`
- 优化路线图：`doc/optimization-roadmap.zh.md`
- 正式 production 长期文档：`doc-rvv/recognition/quantizable_modality-RVV.zh.md`
- 当前 board summary：`log/board/repeated_phase000_shared_spread_rvv/summary.md`

## 证据提交边界

当前文档引用的 summary / manifest / Evidence Doctor（证据体检）和 registry 可作为 summary-only（只提交摘要）证据候选。`log/board/repeated_phase000_shared_spread_rvv/run_*/` raw logs（原始日志）、`log/qemu/`、`build/` 和远端板卡目录不属于默认提交边界。
