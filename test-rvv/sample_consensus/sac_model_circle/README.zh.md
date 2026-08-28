# sac_model_circle RVV topic 导航

## 当前结论

`SampleConsensusModelCircle2D<PointT>` 当前有两个不同层级的结论：

- `selectWithinDistance` / `countWithinDistance`：Phase 000 已完成第一版 production direct（真实生产路径证据）闭环；Phase 080/090 又把 `selectWithinDistance` 的命中点误差写回推进为 full-RVV error tail（完整 RVV 误差尾段）并完成接入后板卡验证。当前 `selectWithinDistance` 使用 Phase 090 接入后数据作为 adopted production behavior（已采用生产行为）依据；`countWithinDistance` 仍使用 Phase 000/090 回归数据。
- `getDistancesToModel`：Phase 020 的 `RVV sqr distance + scalar sqrt + dense double store` 旧候选已被拒绝；Phase 050 的 `vfsqrt + vfwcvt + vse64` full-RVV（完整 RVV）诊断为正向；Phase 060 已接入 production（生产源码），接入后 public Std/RVV（公开入口标量 / RVV）板卡结果也为正向；Phase 070 已按用户确认采纳为 adopted production behavior。
- identity-index strided load（恒等索引跨步加载）：Phase 040 的 RVV-vs-RVV（两个 RVV 实现族直接比较）strict A/B 结果为负，当前实现族被拒绝，生产源码保持 gather-only RVV。

`doc-rvv/sample_consensus/sac_model_circle-RVV.zh.md` 现已适用，记录当前已采纳的 select/count/getDistances production behavior、fallback、证据链和后续边界。`selectWithinDistance` 的正式数据采用 Phase 090 接入后的板卡测试结果，`getDistancesToModel` 的正式数据采用 Phase 060 接入后的板卡测试结果。

## 先读哪份文档

| 目的 | 文档 |
| --- | --- |
| 函数级结论、Traceability Map（可追踪性地图）和 closeout 边界 | `doc/sac_model_circle-evaluation.zh.md` |
| 长期生产实现、覆盖范围、fallback 和正确性与高效性证据链 | `doc-rvv/sample_consensus/sac_model_circle-RVV.zh.md` |
| 测试 target（目标）分类和覆盖矩阵 | `doc/testing-overview.zh.md` |
| gtest 输入、断言和证明范围 | `doc/correctness-tests.zh.md` |
| bench（性能测试）、manifest（证据清单）、Evidence Doctor（证据体检）和 registry（证据登记表） | `doc/benchmark-and-evidence.zh.md` |
| 优化候选状态和恢复条件 | `doc/optimization-evidence.zh.md`、`doc/optimization-roadmap.zh.md` |
| 测试支撑代码和脚本定位 | `doc/test-support-code-map.zh.md` |
| phase loop（阶段循环）恢复 | `doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md` |

## 目录分工

| 路径 | 主职责 |
| --- | --- |
| `sample_consensus/include/pcl/sample_consensus/sac_model_circle.h` | production helper 声明和 `__RVV10__` 保护。 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` | production public entry（公开入口）、fallback（回退路径）和 RVV helper 实现。 |
| `src/test_sac_model_circle.cpp` | correctness（正确性）gtest、public / Standard / RVV 对拍、test-only getDistances candidate 和 Phase 060 production direct gate。 |
| `src/bench_sac_model_circle.cpp` | public select/count/getDistances、diagnostic getDistances candidate、full-RVV getDistances row 和 select error-tail row 的板卡计时入口。 |
| `script/generate_circle_board_evidence_manifest.py` | 从 repeated board output 生成 Phase 000 / 020 / 050 / 060 / 080 / 090 manifest。 |
| `doc/phases/` | phase plan/result、Evidence Doctor 摘要和 optimization matrix。 |
| `log/`、`build/` | 本机生成日志和构建产物，默认不提交。 |

## 常用命令

```bash
make -C test-rvv/sample_consensus/sac_model_circle run_test_compare
make -C test-rvv/sample_consensus/sac_model_circle run_circle_public_tests
make -C test-rvv/sample_consensus/sac_model_circle run_circle_select_error_tail_candidate_test
make -C test-rvv/sample_consensus/sac_model_circle run_circle_getdistances_candidate_test
make -C test-rvv/sample_consensus/sac_model_circle run_circle_getdistances_full_rvv_candidate_test
make -C test-rvv/sample_consensus/sac_model_circle run_circle_getdistances_production_test
make -C test-rvv/sample_consensus/sac_model_circle dump_bench_rvv
make -C test-rvv/sample_consensus/sac_model_circle check_select_production_error_tail_asm
make -C test-rvv/sample_consensus/sac_model_circle production_evidence_status
make -C test-rvv/sample_consensus/sac_model_circle getdistances_evidence_status
make -C test-rvv/sample_consensus/sac_model_circle getdistances_full_rvv_evidence_status
make -C test-rvv/sample_consensus/sac_model_circle getdistances_production_evidence_status
make -C test-rvv/sample_consensus/sac_model_circle select_error_tail_production_evidence_status
```

板卡 repeated evidence（重复板卡证据）命令需要可用 SSH agent：

```bash
SSH_AUTH_SOCK=/run/user/1001/gcr/.ssh \
  make -C test-rvv/sample_consensus/sac_model_circle collect_production_repeated_board_evidence
SSH_AUTH_SOCK=/run/user/1001/gcr/.ssh \
  make -C test-rvv/sample_consensus/sac_model_circle collect_getdistances_repeated_board_evidence
SSH_AUTH_SOCK=/run/user/1001/gcr/.ssh \
  make -C test-rvv/sample_consensus/sac_model_circle collect_getdistances_full_rvv_repeated_board_evidence
SSH_AUTH_SOCK=/run/user/1001/gcr/.ssh \
  make -C test-rvv/sample_consensus/sac_model_circle collect_getdistances_production_repeated_board_evidence
SSH_AUTH_SOCK=/run/user/1001/gcr/.ssh \
  make -C test-rvv/sample_consensus/sac_model_circle collect_select_error_tail_production_repeated_board_evidence
```

## 当前可提交证据

当前可作为 summary-only（只提交摘要证据）候选的文件是 phase 目录下的 manifest / Evidence Doctor 摘要和 `log/evidence_registry.json`。raw board logs（原始板卡日志）、`build/` 下的二进制和反汇编输出、本机配置、私有地址或设备路径默认不提交。

## 默认恢复动作

默认恢复动作：先看 Phase 090 result、正式 `doc-rvv`、`select_error_tail_production_evidence_status`、`getdistances_production_evidence_status` 和 current handoff。当前 topic 内没有新的同边界高优先级性能候选；更多点型 dedicated board、`Scalar=double`、新的 identity/load 组织或 `circle3d` 需要新 phase / 新 scope。
