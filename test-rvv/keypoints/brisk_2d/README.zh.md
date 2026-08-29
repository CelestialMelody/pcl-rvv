# BRISK 2D RVV Topic

## 当前结论

`keypoints/src/brisk_2d.cpp` 的 `Layer::halfsample()` 和 `Layer::twothirdsample()` 已在 RISC-V
`__RVV10__` 构建下接入 RVV（RISC-V 可变长向量）路径；非 RVV / 非 SSSE3 构建使用 portable scalar
fallback（可移植标量回退路径），不再报 `brisk without SSSE3 support not implemented`。

板卡 repeated board（重复板卡测试）证据为 weak-positive（弱正向）：Phase 000 的
`ScaleSpace::constructPyramid` 640x480 case median speedup 为 `1.175x`，`twothirdsample` median 为
`1.115x`。Phase 010 进一步补了 `BriskKeypoint2D::compute()` public entry（公开入口）证据，synthetic
320x240 public compute median speedup 为 `1.017x`，0/5 反向，结论是 near-neutral（接近中性）。
本轮 prompt 已授权“板卡有收益即可采纳”，因此当前 downsample production patch 视为 adopted production
behavior（已采用生产行为），但完整公开入口不写成显著加速。

## 阅读路径

| 读者问题 | 主入口 |
| --- | --- |
| 当前生产实现如何工作 | `doc-rvv/keypoints/brisk_2d-RVV.zh.md` |
| 为什么采纳、证据覆盖到哪里 | `doc/brisk_2d-evaluation.zh.md` |
| 怎么跑 test / bench / board target | `doc/testing-overview.zh.md` |
| 每个 gtest 验证什么 | `doc/correctness-tests.zh.md` |
| bench label、summary、doctor 和 registry | `doc/benchmark-and-evidence.zh.md` |
| 代码、脚本、输出如何互相定位 | `doc/test-support-code-map.zh.md` |
| 当前采用 / 暂缓的优化方式 | `doc/optimization-evidence.zh.md` |
| phase loop 恢复入口 | `doc/phases/README.zh.md` |

## 常用命令

```bash
make -C test-rvv/keypoints/brisk_2d run_test_compare
make -C test-rvv/keypoints/brisk_2d dump_bench_rvv
SSH_AUTH_SOCK=/tmp/ssh-iEjvVUej0dT1/agent.101441 make -C test-rvv/keypoints/brisk_2d collect_repeated_board_evidence
SSH_AUTH_SOCK=/tmp/ssh-iEjvVUej0dT1/agent.101441 make -C test-rvv/keypoints/brisk_2d collect_public_repeated_board_evidence
make -C test-rvv/keypoints/brisk_2d record_repeated_board_evidence_state
make -C test-rvv/keypoints/brisk_2d record_public_repeated_board_evidence_state
make -C test-rvv/keypoints/brisk_2d repeated_evidence_status
```

QEMU（仿真器）只用于 correctness（正确性）、路径和日志形状；性能结论只来自 board（板卡）。

## 证据白名单

本 topic 默认 `summary-only`（只提交摘要）策略。可引用的摘要证据为：

- `doc/phases/000-current-state-and-downsample-diagnostic/repeated-evidence-summary.md`
- `doc/phases/000-current-state-and-downsample-diagnostic/repeated-evidence-manifest.json`
- `doc/phases/000-current-state-and-downsample-diagnostic/repeated-evidence-doctor.md`
- `doc/phases/000-current-state-and-downsample-diagnostic/repeated-evidence-doctor.json`
- `doc/phases/010-public-compute-end-to-end/repeated-evidence-summary.md`
- `doc/phases/010-public-compute-end-to-end/repeated-evidence-manifest.json`
- `doc/phases/010-public-compute-end-to-end/repeated-evidence-doctor.md`
- `doc/phases/010-public-compute-end-to-end/repeated-evidence-doctor.json`
- `log/evidence_registry.json`

`log/board/**`、`log/qemu/**`、`build/**` 和完整 asm dump 默认是本地生成物，不进入普通提交边界。
