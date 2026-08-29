# recognition/hough_3d RVV 主题

本目录保存 `recognition/include/pcl/recognition/impl/cg/hough_3d.hpp` 的 RVV
优化工作。当前 topic token（主题短标识）为 `hough_3d`。

## 当前状态

- `000-current-state-and-vote-generation-diagnostic` 已完成。
- `010-production-direct-vote-generation-probe` 已完成。
- `020-accumulator-scatter-audit` 已完成，结果只得到接近阈值的弱信号，未形成新的可继续推进的 unblocked phase。
- `030-default-distance-weight-production-direct` 已完成，默认 `use_distance_weight=false`
  production direct 仍为 `neutral`。
- 当前判断：vote generation helper 的诊断证据正向，但 production direct
  repeated board 为 `neutral`，不支持把当前 patch 采纳为 adopted；no-interpolation
  消融和默认 distance weight 配置也没有把结论推到可采纳区间。
- 当前 topic 先暂停，等待用户决定是否开启新的候选方向；目前不建议继续在
  `voteInt()` / voter tracking 上直接扩生产实现。

## 常用命令

```bash
make -C test-rvv/recognition/hough_3d run_test_compare
make -C test-rvv/recognition/hough_3d run_qemu_smoke
make -C test-rvv/recognition/hough_3d dump_bench_rvv
make -C test-rvv/recognition/hough_3d board_repeated
make -C test-rvv/recognition/hough_3d board_production_repeated
make -C test-rvv/recognition/hough_3d board_phase020_no_interpolation_repeated
make -C test-rvv/recognition/hough_3d board_phase030_default_distance_weight_repeated
make -C test-rvv/recognition/hough_3d evidence_doctor_repeated
make -C test-rvv/recognition/hough_3d evidence_doctor_production_repeated
make -C test-rvv/recognition/hough_3d evidence_doctor_phase020_no_interpolation
make -C test-rvv/recognition/hough_3d evidence_doctor_phase030_default_distance_weight
make -C test-rvv/recognition/hough_3d record_evidence_state_phase020_no_interpolation
make -C test-rvv/recognition/hough_3d record_evidence_state_phase030_default_distance_weight
make -C test-rvv/recognition/hough_3d check_evidence_freshness
```

QEMU（仿真器）只用于 correctness（正确性）、构建和日志形状；性能结论只来自
board（板卡）或目标硬件。

## 文档入口

- 函数级评估：`doc/hough_3d-evaluation.zh.md`
- 测试总览：`doc/testing-overview.zh.md`
- 正确性测试字典：`doc/correctness-tests.zh.md`
- bench 与证据：`doc/benchmark-and-evidence.zh.md`
- 优化证据索引：`doc/optimization-evidence.zh.md`
- 测试支撑代码地图：`doc/test-support-code-map.zh.md`
- 优化路线图：`doc/optimization-roadmap.zh.md`
- 阶段索引：`doc/phases/README.zh.md`
- 优化矩阵：`doc/phases/optimization-matrix.zh.md`
- phase 000 result：`doc/phases/000-current-state-and-vote-generation-diagnostic/result.zh.md`
- phase 010 result：`doc/phases/010-production-direct-vote-generation-probe/result.zh.md`
- phase 020 result：`doc/phases/020-accumulator-scatter-audit/result.zh.md`
- phase 030 result：`doc/phases/030-default-distance-weight-production-direct/result.zh.md`
