# ICP RVV topic 总览

本目录是 `registration/include/pcl/registration/impl/icp.hpp` 的 RVV 专项测试工程。当前 production
优化只覆盖 `IterativeClosestPoint::transformCloud`：`Scalar=float`、输入规模不少于 32、点类型通过
RVV AoS layout traits 且运行期 field offset 匹配时走 RVV，否则调用
`pcl::registration::detail::transformCloudStandard` 标量 fallback。

生产文件：

```text
registration/include/pcl/registration/impl/icp.hpp
```

## 先读哪份文档

| 问题 | 文档 |
| --- | --- |
| 当前测试体系、运行入口和覆盖矩阵 | `doc/testing-overview.zh.md` |
| 每个 gtest 的输入、被测路径、断言和证明范围 | `doc/correctness-tests.zh.md` |
| bench label、board repeated、Evidence Doctor 和提交边界 | `doc/benchmark-and-evidence.zh.md` |
| 优化方式、采纳/拒绝原因和证据索引 | `doc/optimization-evidence.zh.md` |
| test support、script、output 和 production helper 调用地图 | `doc/test-support-code-map.zh.md` |
| EvidenceDecision、Traceability Map 和 accepted risks | `doc/icp-evaluation.zh.md` |
| 反汇编归因 | `doc/asm-attribution.zh.md` |
| phase plan/result 和 optimization matrix | `doc/phases/` |
| 跨阶段候选路线和恢复条件 | `doc/optimization-roadmap.zh.md` |
| 长期 production 行为和证据链 | `../../../doc-rvv/registration/icp-RVV.zh.md` |

## 目录分工

| 路径 | 职责 |
| --- | --- |
| `src/test_icp.cpp` | gtest case 和 production direct exposed wrapper。 |
| `src/bench_icp.cpp` | bench 薄入口。 |
| `include/icp.h` | topic-level test support 聚合入口。 |
| `include/test_icp.h` | gtest 聚合入口。 |
| `include/bench_icp.h` | bench harness、case registry 和 checksum 输出。 |
| `include/impl/icp_transform_cloud.hpp` | test-only reference、diagnostic candidate、fixtures 和 checksum helper。 |
| `script/collect_icp_board_repeated.py` | board repeated Std/RVV compare collector。 |
| `script/generate_icp_board_evidence_manifest.py` | board repeated Evidence Doctor manifest。 |
| `doc/` | topic-local 测试、bench、优化证据、代码地图和 evaluation。 |
| `../../../doc-rvv/registration/icp-RVV.zh.md` | adopted production 行为的长期说明。 |

## 常用命令

QEMU correctness，只用于正确性：

```bash
make -C test-rvv/registration/icp run_test_compare record_qemu_correctness_state
```

板卡 correctness：

```bash
make -C test-rvv/registration/icp run_board_test fetch_board_logs
```

板卡 repeated benchmark，性能结论只看 board / target hardware：

```bash
make -C test-rvv/registration/icp collect_board_transform_cloud_repeated \
  run_board_evidence_doctor record_board_evidence_state
```

RVV bench 反汇编：

```bash
make -C test-rvv/registration/icp dump_bench_rvv
```

证据链和 registry 状态：

```bash
make -C test-rvv/registration/icp evidence_status
```

QEMU `run_bench_compare` 已被公共 Makefile 默认 guard 拦住。只有为了历史/窄范围日志形状 smoke，
并明确写成 `qemu_smoke_only` 时，才可显式加 `ALLOW_QEMU_BENCH_COMPARE=1`；QEMU bench compare
不能进入性能排序、采纳审计或 EvidenceDecision。

## 当前可提交证据

| 路径 | 角色 |
| --- | --- |
| `test-rvv/registration/icp/log/board/transform_cloud_repeated/summary.md` | Milkv-Jupiter 5-run production direct repeated summary。 |
| `test-rvv/registration/icp/log/board/transform_cloud_repeated/evidence_doctor.md` | Board Evidence Doctor：Errors=0，Warnings=2，Suggestions=0。 |
| `test-rvv/registration/icp/log/qemu/evidence_doctor.md` | 历史 QEMU smoke Doctor，只说明该 evidence 不参与性能结论。 |
| `test-rvv/registration/icp/doc/asm-attribution.zh.md` | production `transformCloud` 符号反汇编归因。 |
| `test-rvv/registration/icp/doc/phases/003-production-integration/result.zh.md` | production direct 采纳结果。 |
| `test-rvv/registration/icp/doc/phases/004-structure-parity-doc-suite/result.zh.md` | 文档套件对齐审计结果。 |

当前 board repeated median：

| case | median speedup |
| --- | ---: |
| `icp transform-cloud xyz 64K` | 5.68x |
| `icp transform-cloud xyz 256K` | 5.30x |
| `icp transform-cloud xyz-normal 64K` | 3.76x |
| `icp transform-cloud xyz-normal 256K` | 3.93x |

## 默认不提交的生成产物

| 产物 | 默认状态 |
| --- | --- |
| `build/` 下的二进制、对象文件和 raw asm output | local-only |
| `log/qemu/run_test_*.log` | local-only correctness input |
| `log/qemu/run_bench_*.log`、`log/qemu/analyze_bench_compare.log` | historical smoke only |
| `log/board/run_bench_*.log`、`log/board/analyze_bench_compare.log` | raw board input |
| repeated collector 的临时 per-run raw compare logs | local-only |
| `log/evidence_registry.json` | 本机 freshness registry；默认按任务边界决定是否提交 |

## 当前结果

当前 EvidenceDecision 为 `production_direct_positive`。覆盖：

- `PointXYZ`、`PointNormal`、`PointXYZI`、`PointXYZINormal` 的 production direct correctness。
- 小规模 fallback、`Scalar=double` fallback、in-place、XYZ/normal finite gate 和运行期 offset gate。
- `PointXYZ` / `PointNormal` production direct board repeated benchmark。
- production `transformCloud` 符号的 RVV 指令归因。

不覆盖：

- `IterativeClosestPointWithNormals`，它直接调用 `pcl::transformPointCloudWithNormals`。
- indices / correspondences 路径；`transformCloud` 自身只扫描已经 materialized 的 input cloud。
- ICP 端到端整体加速幅度；nearest-neighbor search 和 SVD 求解仍是独立成本。
