# pcd_io_templated_writer RVV topic

本 topic 针对 `io/include/pcl/io/impl/pcd_io.hpp` 中
`PCDWriter::writeBinaryCompressed<PointT>` 的压缩前置布局转换，并补充
`writeBinary<PointT>` packed output loop 的 component ablation（组件消融）和 production-public
（真实公开入口）接入验证。当前 EvidenceDecision（证据决策）是
`compressed-writer adopted production behavior / binary-writer tuple-segment adopted production behavior`：
Phase 060 已把 compressed writer 的 4 字节字段 RVV pack 接入真实 public overload，并在接入后完成
production direct（真实生产入口直连）正确性、fallback、反汇编和 Milkv-Jupiter 板卡
production-public（真实公开入口）5-run repeated bench。正式长期文档为
`doc-rvv/io/pcd_io_templated_writer-RVV.zh.md`。Phase 080 已把 binary writer 的 field-outer RVV
实现族作为独立 production boundary 接入验证，但板卡 production-public 5-run mean 为
`0.9842x / 0.9727x / 0.9810x`，Evidence Doctor Errors=3，用户确认负收益可回滚后已回滚。
随后 Phase 090/100 尝试 tuple / segment output path：Phase 100 接入后板卡 compact mean
`1.3046x`，padding mean `1.3240x`，small smoke mean `1.1082x`，Doctor Errors=0 Warnings=1；
该新实现族当前已采纳。

当前没有默认继续的高优先级 RVV production 动作。后续若继续扩大，只应另开新 phase，先冻结
arbitrary compact payload memcpy、non-4-byte fields、更多 point type / layout 或 ASCII profile
等具体范围。

## 阅读路径

1. `doc/pcd_io_templated_writer-evaluation.zh.md`：函数级评估、Traceability Map（可追踪性地图）和诊断证据链要求。
2. `doc/testing-overview.zh.md`：测试入口总览、target 粒度和 QEMU / board / production direct 边界。
3. `doc/correctness-tests.zh.md`：每个 gtest 的输入、断言、证明范围和不能证明的范围。
4. `doc/benchmark-and-evidence.zh.md`：bench case、summary、manifest、Evidence Doctor（证据体检）和 registry。
5. `doc/optimization-evidence.zh.md`：candidate family（候选族）到测试、bench、板卡和决策的索引。
6. `doc/test-support-code-map.zh.md`：测试支撑代码、script、output 和 production 对照位置。
7. `doc/phases/README.zh.md`：phase index（阶段索引）和默认恢复动作。
8. `doc/phases/optimization-matrix.zh.md`：候选与证据矩阵。
9. `doc/optimization-roadmap.zh.md`：后续候选和恢复动作。

## 目录分工

| role | 主归属 |
| --- | --- |
| topic navigation（主题导航） | 本 README |
| evaluation diagnostic（诊断评估） | `doc/pcd_io_templated_writer-evaluation.zh.md` |
| testing overview（测试总览） | `doc/testing-overview.zh.md` |
| correctness tests（正确性测试说明） | `doc/correctness-tests.zh.md` |
| benchmark and evidence（性能测试与证据） | `doc/benchmark-and-evidence.zh.md` |
| optimization evidence（优化证据索引） | `doc/optimization-evidence.zh.md` |
| test support code map（测试支撑代码地图） | `doc/test-support-code-map.zh.md` |
| phase suite（阶段文档套件） | `doc/phases/**` |
| production topic doc（生产长期主题文档） | `doc-rvv/io/pcd_io_templated_writer-RVV.zh.md` |

## 常用命令

```bash
make -C test-rvv/io/pcd_io_templated_writer run_test_compare
make -C test-rvv/io/pcd_io_templated_writer run_qemu_smoke BENCH_ARGS="--case-filter compressed_* --iterations 2 --warmup-iterations 1"
make -C test-rvv/io/pcd_io_templated_writer dump_bench_rvv
make -C test-rvv/io/pcd_io_templated_writer run_board_pcdtw_repeated
make -C test-rvv/io/pcd_io_templated_writer run_board_pcdtw_shaped_repeated
make -C test-rvv/io/pcd_io_templated_writer run_board_pcdtw_binary_repeated
make -C test-rvv/io/pcd_io_templated_writer run_board_pcdtw_production_compressed_repeated
make -C test-rvv/io/pcd_io_templated_writer run_board_pcdtw_production_binary_tuple_repeated
```

QEMU 只用于 correctness（正确性）、构建和日志形状；性能结论必须来自板卡或目标硬件。

## 当前可提交证据边界

summary-only（只提交摘要）证据候选：

- `log/board/component_ablation_repeat_5/summary.md`
- `log/board/component_ablation_repeat_5/evidence_manifest.json`
- `log/board/component_ablation_repeat_5/evidence_doctor.md`
- `log/board/production_shaped_repeat_5/summary.md`
- `log/board/production_shaped_repeat_5/evidence_manifest.json`
- `log/board/production_shaped_repeat_5/evidence_doctor.md`
- `log/board/binary_component_repeat_5/summary.md`
- `log/board/binary_component_repeat_5/evidence_manifest.json`
- `log/board/binary_component_repeat_5/evidence_doctor.md`
- `log/board/production_compressed_repeat_5/summary.md`
- `log/board/production_compressed_repeat_5/evidence_manifest.json`
- `log/board/production_compressed_repeat_5/evidence_doctor.md`
- `log/board/production_binary_repeat_5/summary.md`
- `log/board/production_binary_repeat_5/evidence_manifest.json`
- `log/board/production_binary_repeat_5/evidence_doctor.md`
- `log/board/binary_tuple_segment_repeat_5/summary.md`
- `log/board/binary_tuple_segment_repeat_5/evidence_manifest.json`
- `log/board/binary_tuple_segment_repeat_5/evidence_doctor.md`
- `log/board/production_binary_tuple_repeat_5/summary.md`
- `log/board/production_binary_tuple_repeat_5/evidence_manifest.json`
- `log/board/production_binary_tuple_repeat_5/evidence_doctor.md`
- `log/evidence_registry.json`

默认不提交：`build/`、raw board / QEMU logs、`script/__pycache__/`、本机 `config.mk`、私有板卡地址和远端绝对路径。

## 默认恢复动作

compressed writer production closeout 已完成。binary writer Phase 080 field-outer path 已回滚为历史负向证据；
Phase 100 tuple / segment path 已完成 production closeout。默认恢复动作是停在当前 topic closeout：
不继续自动扩大 production；如用户要求继续，再为新的具体范围创建下一 phase。
