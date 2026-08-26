# VFH Benchmark 与证据说明

## Bench 输入和计时边界

`test-rvv/features/vfh/src/bench_vfh.cpp` 构造 synthetic dense `PointNormal` cloud（合成 dense 点云），默认
`--side 80` 生成 `6400` 个点，`--iterations 8`，`--warmup 2`。checksum 是 308-bin descriptor 按
`sum(histogram[i] * (i + 1))` 计算的加权和，用于检查 Std/RVV 输出是否一致。

性能结论只引用 board repeated（板卡重复采集）结果。QEMU（仿真器）只用于构建、correctness（正确性）和
反汇编输出，不把 QEMU timing 写成性能结论。

## Bench label 字典

| label | 计时边界 | 证据角色 | 当前用途 |
| --- | --- | --- | --- |
| `component_vfh_reference` | topic-local scalar reference。 | diagnostic baseline（诊断基线）。 | 只辅助观察，不参与 production adoption。 |
| `candidate_vfh_centroid_spfh_rvv` | Phase 000 test-only candidate，失败时 fallback 到 reference。 | diagnostic candidate。 | 历史正向候选来源。 |
| `candidate_vfh_spfh_viewpoint_rvv` | Phase 020 test-only combined candidate。 | diagnostic candidate。 | 证明 viewpoint preparation 值得纳入生产探针。 |
| `candidate_vfh_centroids_spfh_viewpoint_rvv` | Phase 030 test-only reduction-combined candidate。 | diagnostic candidate。 | 证明 normal centroid reduction 值得纳入生产探针。 |
| `production_vfh_compute_default` | 真实 `VFHEstimation::compute()` public path。 | production-public evidence（公开生产入口证据）。 | Phase 060 adopted production behavior 的性能来源。 |

## Phase 060 生产结果

命令：

```bash
make -C test-rvv/features/vfh board_repeated REPEATED_BOARD_OUTPUT_DIR=log/board/repeated-production-phase060-postreview BENCH_ARGS='--side 80 --iterations 8 --warmup 2'
make -C test-rvv/features/vfh evidence_doctor_repeated REPEATED_BOARD_OUTPUT_DIR=log/board/repeated-production-phase060-postreview
```

`production_vfh_compute_default` 的 5-run checksum 均为 Std/RVV `448016`。

| run | Std ms | RVV ms | speedup |
| --- | ---: | ---: | ---: |
| run_01 | 8.86645 | 5.36115 | 1.65383x |
| run_02 | 8.82497 | 5.38891 | 1.63762x |
| run_03 | 8.79738 | 5.37904 | 1.63549x |
| run_04 | 8.76886 | 5.38770 | 1.62757x |
| run_05 | 8.81418 | 5.37161 | 1.64088x |
| mean | 8.81437 | 5.37768 | 1.63906x |

Evidence Doctor 输出路径：

- `log/board/repeated-production-phase060-postreview/evidence_manifest.json`
- `log/board/repeated-production-phase060-postreview/evidence_doctor.md`
- `log/board/repeated-production-phase060-postreview/evidence_doctor.json`

结果为 `Errors=0，Warnings=0，Suggestions=11`。Suggestions 只要求补环境 metadata、binary identity，以及解释历史
`component_vfh_reference` near-threshold baseline；这些不阻塞当前 production case 采纳。

## 证据提交边界

默认 evidence policy（证据策略）为 `summary-only`。topic-local 文档引用 run logs / manifest / Doctor
路径，raw run log、`build/`、远端板卡路径和本机临时输出不默认提交。若后续用户要求提交日志，应先运行
topic 或全局日志脱敏检查，并把 evidence logs（证据日志）与源码 / 文档提交拆开。
