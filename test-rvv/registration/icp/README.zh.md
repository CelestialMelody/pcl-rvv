# ICP RVV topic 总览

本目录是 `registration/include/pcl/registration/impl/icp.hpp` 的 RVV 专项测试工程。当前优化已接入
`IterativeClosestPoint::transformCloud` production（生产）路径：`Scalar=float`、输入规模不少于 32、
点类型通过 RVV AoS layout traits（字段布局特征）且运行期 field offset 匹配时走 RVV，否则调用
`pcl::registration::detail::transformCloudStandard` 标量 fallback。

生产文件：

```text
registration/include/pcl/registration/impl/icp.hpp
```

## 先读哪份文档

| 问题 | 文档 |
| --- | --- |
| 函数级评估、生产接入边界和 Traceability Map（可追踪性地图） | `doc/icp-evaluation.zh.md` |
| 反汇编归因 | `doc/asm-attribution.zh.md` |
| 当前 phase plan/result 和 optimization matrix（优化矩阵） | `doc/phases/` |
| 跨阶段候选路线和恢复条件 | `doc/optimization-roadmap.zh.md` |
| 长期主题说明和证据链 | `../../../doc-rvv/registration/icp-RVV.zh.md` |

## 常用命令

QEMU correctness（QEMU 正确性）对拍，只用于正确性：

```bash
make -C test-rvv/registration/icp run_test_compare record_qemu_correctness_state
```

板卡 correctness（板卡正确性）：

```bash
make -C test-rvv/registration/icp run_board_test fetch_board_logs
```

板卡 repeated benchmark（性能结论只看板卡 / target hardware）：

```bash
make -C test-rvv/registration/icp collect_board_transform_cloud_repeated \
  run_board_evidence_doctor record_board_evidence_state
```

RVV bench 反汇编：

```bash
make -C test-rvv/registration/icp dump_bench_rvv
```

证据链和 registry（证据登记）状态：

```bash
make -C test-rvv/registration/icp evidence_status
```

QEMU `run_bench_compare` 已被公共 Makefile 默认 guard 拦住。只有为了历史/窄范围日志形状 smoke，
并明确写成 `qemu_smoke_only` 时，才可显式加 `ALLOW_QEMU_BENCH_COMPARE=1`；QEMU bench compare
不能进入性能排序、采纳审计或 EvidenceDecision。

## 当前证据路径

当前 production direct evidence（生产直连证据）：

| 路径 | 角色 |
| --- | --- |
| `test-rvv/registration/icp/log/qemu/run_test_std.log` | QEMU 标量构建 gtest：12 tests passed。 |
| `test-rvv/registration/icp/log/qemu/run_test_rvv.log` | QEMU RVV 构建 gtest：12 tests passed。 |
| `test-rvv/registration/icp/log/board/run_test.log` | 板卡 RVV 构建 gtest：12 tests passed。 |
| `test-rvv/registration/icp/log/board/transform_cloud_repeated/summary.md` | Milkv-Jupiter 5-run production direct repeated summary。 |
| `test-rvv/registration/icp/log/board/transform_cloud_repeated/evidence_manifest.json` | Evidence Doctor manifest，`evidence_role=production_direct`。 |
| `test-rvv/registration/icp/log/board/transform_cloud_repeated/evidence_doctor.md` | Evidence Doctor：Errors=0，Warnings=2，Suggestions=0。 |
| `test-rvv/registration/icp/log/evidence_registry.json` | evidence registry 当前记录。 |

当前 repeated board median：

| case | median speedup |
| --- | ---: |
| `icp transform-cloud xyz 64K` | 5.68x |
| `icp transform-cloud xyz 256K` | 5.30x |
| `icp transform-cloud xyz-normal 64K` | 3.76x |
| `icp transform-cloud xyz-normal 256K` | 3.93x |

Historical QEMU smoke evidence（历史 QEMU smoke，不是默认动作，不是性能结论）：

| 路径 | 角色 |
| --- | --- |
| `test-rvv/registration/icp/log/qemu/run_bench_std.log` | 历史 QEMU bench raw log，`qemu_smoke_only`。 |
| `test-rvv/registration/icp/log/qemu/run_bench_rvv.log` | 历史 QEMU bench raw log，`qemu_smoke_only`。 |
| `test-rvv/registration/icp/log/qemu/analyze_bench_compare.log` | 历史 QEMU Std/RVV smoke 汇总，不能作为性能结论。 |
| `test-rvv/registration/icp/log/qemu/evidence_manifest.json` | 历史 QEMU smoke manifest。 |
| `test-rvv/registration/icp/log/qemu/evidence_doctor.md` | 历史 QEMU smoke Doctor。 |

## 当前覆盖与不覆盖

- 覆盖：`PointXYZ`、`PointNormal`、`PointXYZI`、`PointXYZINormal` 的 production direct correctness，
  小规模 fallback、`Scalar=double` fallback、in-place、XYZ/normal finite gate 和运行期 offset gate。
- 覆盖：`PointXYZ` / `PointNormal` production direct board repeated benchmark。
- 不覆盖：`IterativeClosestPointWithNormals`，它直接调用 `pcl::transformPointCloudWithNormals`。
- 不覆盖：indices / correspondences 路径；`transformCloud` 自身只扫描已经 materialized 的 input cloud。
