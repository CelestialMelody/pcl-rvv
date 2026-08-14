# ICP transformCloud 测试体系总览

## 本文职责

本文说明 `test-rvv/registration/icp` 的测试入口、证据层级、覆盖矩阵和可提交证据边界。具体 gtest
语义放在 `doc/correctness-tests.zh.md`，bench label 和 evidence output 规则放在
`doc/benchmark-and-evidence.zh.md`，优化方式与证据映射放在 `doc/optimization-evidence.zh.md`。

## 文档阅读路径

| 读者问题 | 主文档 |
| --- | --- |
| 先看当前结论、常用命令和证据白名单 | `README.zh.md` |
| 每个 gtest 证明什么 | `doc/correctness-tests.zh.md` |
| bench label、board target、Evidence Doctor 和提交边界 | `doc/benchmark-and-evidence.zh.md` |
| RVV 优化方式、采纳/拒绝原因和证据索引 | `doc/optimization-evidence.zh.md` |
| 测试支撑代码、script 和 production helper 如何对应 | `doc/test-support-code-map.zh.md` |
| EvidenceDecision、Traceability Map 和风险 | `doc/icp-evaluation.zh.md` |
| 长期 production 行为 | `../../../doc-rvv/registration/icp-RVV.zh.md` |

## 测试类型定义

| 类型 | 入口 | 作用 | 证据边界 |
| --- | --- | --- | --- |
| QEMU correctness | `run_test_compare` | Std/RVV 构建各跑 gtest，验证语义对拍。 | 只证明 correctness / build，不证明性能。 |
| board correctness | `run_board_test fetch_board_logs` | 目标硬件运行 RVV gtest。 | 证明板卡可运行 production direct path，不证明稳定性能。 |
| board repeated benchmark | `collect_board_transform_cloud_repeated` | 板卡上重复 Std/RVV benchmark，输出 median/min/max。 | ICP `transformCloud` full-cloud microbench，不等同端到端 ICP。 |
| asm attribution | `dump_bench_rvv` | 把 RVV 指令簇归因到 production `transformCloud` 符号。 | 证明 hot path 归属，不做周期级归因。 |
| Evidence Doctor | `run_board_evidence_doctor` | 检查 repeated summary 的异常模式。 | Errors 阻塞 production evidence；Warnings 必须解释。 |
| evidence registry | `evidence_status` | 检查已登记 evidence 与文档引用关系。 | Registry 只管证据 freshness，不替代语义解释。 |
| QEMU bench smoke | guarded historical only | 历史日志形状和 checksum smoke。 | 不作为性能排序、采纳审计或 EvidenceDecision。 |

## 运行入口分类

QEMU correctness：

```bash
make -C test-rvv/registration/icp run_test_compare record_qemu_correctness_state
```

板卡 correctness：

```bash
make -C test-rvv/registration/icp run_board_test fetch_board_logs
```

板卡 repeated benchmark：

```bash
make -C test-rvv/registration/icp collect_board_transform_cloud_repeated \
  run_board_evidence_doctor record_board_evidence_state
```

反汇编归因：

```bash
make -C test-rvv/registration/icp dump_bench_rvv
```

证据登记状态：

```bash
make -C test-rvv/registration/icp evidence_status
```

QEMU `run_bench_compare` 默认不运行。公共 Makefile 已设置 guard；只有为了历史/窄范围 smoke，
并显式设置 `ALLOW_QEMU_BENCH_COMPARE=1` 且写明 `qemu_smoke_only` 时才可绕过。

## 输入数据总览

| 数据族 | 构造位置 | 规模 | 用途 |
| --- | --- | --- | --- |
| `PointXYZ` | `include/impl/icp_transform_cloud.hpp::makePointXYZCloud` | gtest 4096；bench 64K / 256K | 无 normal 的 XYZ 主分支。 |
| `PointNormal` | `include/impl/icp_transform_cloud.hpp::makePointNormalCloud` | gtest 2048 / 4096；bench 64K / 256K | XYZ + normal 双 mask 主分支。 |
| `PointXYZI` | `src/test_icp.cpp::makePointXYZICloud` | gtest 4096 | generic XYZ layout gate，非 transform 字段保持。 |
| `PointXYZINormal` | `src/test_icp.cpp::makePointXYZINormalCloud` | gtest 4096 | generic XYZ+normal layout gate，intensity / curvature 保持。 |
| 小规模 input | `src/test_icp.cpp` | 17 | 触发 `input.size() < 32` fallback。 |
| 非有限值 input | `src/test_icp.cpp` | 单点注入 NaN / Inf | 保护 XYZ finite gate 和 normal finite gate。 |

## 覆盖矩阵

| 语义 / 边界 | gtest | board correctness | board repeated | 说明 |
| --- | --- | --- | --- | --- |
| `PointXYZ` full-cloud RVV | covered | covered | covered | 代表无 normal 主分支。 |
| `PointNormal` full-cloud RVV | covered | covered | covered | 代表 XYZ + normal 双 mask 主分支。 |
| XYZ finite gate | covered | covered | indirect | XYZ 任一非有限时跳过整个点。 |
| normal finite gate | covered | covered | indirect | normal 非有限时只跳过 normal 写回。 |
| in-place `cloud_in == cloud_out` | covered | covered | not_timed | 保护 production 注释语义。 |
| runtime offset fallback | covered | covered | not_timed | offset 不匹配时不能误走 RVV。 |
| `input.size() < 32` fallback | covered | covered | not_timed | 小规模标量路径零误差对拍。 |
| `Scalar=double` fallback | covered | covered | not_timed | RVV 只覆盖 `Scalar=float`。 |
| generic XYZ layout | covered | covered | representative_only | `PointXYZI` correctness；性能由 `PointXYZ` 代表。 |
| generic XYZ+normal layout | covered | covered | representative_only | `PointXYZINormal` correctness；性能由 `PointNormal` 代表。 |
| `IterativeClosestPointWithNormals` | not_applicable | not_applicable | not_applicable | 该类 override 调用 `pcl::transformPointCloudWithNormals`。 |
| indices / correspondences | not_applicable | not_applicable | not_applicable | `transformCloud` 只处理已 materialized input cloud。 |

## 当前可提交证据

| 路径 | 角色 |
| --- | --- |
| `test-rvv/registration/icp/log/board/transform_cloud_repeated/summary.md` | Milkv-Jupiter 5-run production direct repeated summary。 |
| `test-rvv/registration/icp/log/board/transform_cloud_repeated/evidence_doctor.md` | Board repeated Evidence Doctor：Errors=0，Warnings=2，Suggestions=0。 |
| `test-rvv/registration/icp/log/qemu/evidence_doctor.md` | 历史 QEMU smoke doctor；只说明历史 smoke 边界。 |
| `test-rvv/registration/icp/doc/asm-attribution.zh.md` | production `transformCloud` 符号归因。 |
| `test-rvv/registration/icp/doc/phases/003-production-integration/result.zh.md` | production direct 采纳结果。 |
| `test-rvv/registration/icp/doc/phases/004-structure-parity-doc-suite/result.zh.md` | 文档套件对齐审计结果。 |

## 默认不提交的生成产物

| 产物 | 默认状态 | 说明 |
| --- | --- | --- |
| `build/` 下二进制和 asm raw output | local-only | 可重新生成，不进入默认提交。 |
| `log/qemu/run_test_*.log` | local-only evidence input | README / evaluation 可引用，但默认不作为可提交摘要。 |
| `log/qemu/run_bench_*.log` 和 `log/qemu/analyze_bench_compare.log` | historical smoke only | 不进入性能结论。 |
| `log/board/run_bench_*.log` 和 `log/board/analyze_bench_compare.log` | raw board inputs | repeated summary 承担可审查摘要。 |
| 临时 repeated raw compare logs | local-only | collector 默认用临时目录，脚本结束后清理。 |

## 当前结论边界

当前 EvidenceDecision 是 `production_direct_positive`。结论只覆盖
`IterativeClosestPoint::transformCloud` 的 full-cloud `Scalar=float` RVV path；不声称
`IterativeClosestPointWithNormals`、nearest-neighbor search、correspondence rejector、SVD 求解或 ICP
端到端整体加速同等幅度。
