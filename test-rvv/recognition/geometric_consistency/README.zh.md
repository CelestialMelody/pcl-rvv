# recognition/geometric_consistency RVV 主题

本目录保存 `recognition/include/pcl/recognition/impl/cg/geometric_consistency.hpp`
的 RVV 专项测试资产。当前 topic token 为 `gc`，完整主题名为
`geometric_consistency`。

## 当前状态

- 当前已完成 phase：`010-cluster-growth-diagnostic`
- 当前已完成 phase：`020-cluster-growth-production-probe`
- 当前证据角色：diagnostic（诊断）、production direct correctness（生产直连正确性）和 board proxy（板卡代理）
- production 状态：已接入窄范围 production RVV，`recognize()` 入口仍保留标量回退
- `doc-rvv` 状态：applicable；当前已有 adopted production behavior

## 常用命令

```bash
make -C test-rvv/recognition/geometric_consistency run_test_compare
make -C test-rvv/recognition/geometric_consistency run_qemu_smoke
make -C test-rvv/recognition/geometric_consistency run_upstream_test_compare
make -C test-rvv/recognition/geometric_consistency check_gc_rvv_asm
make -C test-rvv/recognition/geometric_consistency board_repeated
make -C test-rvv/recognition/geometric_consistency board_repeated_growth BENCH_ARGS='4096 200 5 0.03 growth'
make -C test-rvv/recognition/geometric_consistency record_evidence_state_repeated
make -C test-rvv/recognition/geometric_consistency record_evidence_state_growth BENCH_ARGS='4096 200 5 0.03 growth'
make -C test-rvv/recognition/geometric_consistency check_evidence_freshness
```

QEMU 只用于正确性、构建和日志形状；性能结论仍以 board（板卡）或目标硬件为准。

## 文档入口

- 函数级评估：`doc/geometric_consistency-evaluation.zh.md`
- 测试总览：`doc/testing-overview.zh.md`
- 正确性测试：`doc/correctness-tests.zh.md`
- bench 与证据：`doc/benchmark-and-evidence.zh.md`
- 优化证据：`doc/optimization-evidence.zh.md`
- 测试支撑代码地图：`doc/test-support-code-map.zh.md`
- 优化路线图：`doc/optimization-roadmap.zh.md`
- 阶段索引：`doc/phases/README.zh.md`
- 优化矩阵：`doc/phases/optimization-matrix.zh.md`
- Phase 000 plan：`doc/phases/000-current-state-and-gaps/plan.zh.md`
- Phase 000 result：`doc/phases/000-current-state-and-gaps/result.zh.md`
- Phase 010 plan：`doc/phases/010-cluster-growth-diagnostic/plan.zh.md`
- Phase 010 result：`doc/phases/010-cluster-growth-diagnostic/result.zh.md`
- Phase 020 plan：`doc/phases/020-cluster-growth-production-probe/plan.zh.md`
- Phase 020 result：`doc/phases/020-cluster-growth-production-probe/result.zh.md`
