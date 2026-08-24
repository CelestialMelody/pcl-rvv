# Phase 002 Result: weighted-spfh-33 production probe

## 结论

本阶段完成 `features/include/pcl/features/impl/fpfh.hpp::weightPointSPFHSignature` 的 PI5
production probe（生产探针）。当前 RVV production patch（生产补丁）只覆盖默认
11+11+11 bin 的 FPFHSignature33（快速点特征直方图 33 维描述子）加权合成：

- `__RVV10__` 构建中新增 `pcl::detail::weightFPFHSignature33RVV`。
- 入口先尝试 RVV helper；输入行数、bin 数、行索引或 layout gate（布局验收条件）不满足时，继续原标量路径。
- helper 支持 production lookup 后的 valid row indices（有效行索引），不要求 dense sequential rows（连续行）。
- 非 RVV 构建、非 11+11+11 bins、行索引越界、matrix row count 不一致等路径保持标量语义。

PI5 证据支持保留当前有界生产补丁。本 topic 的采纳口径是：post-integration board data（接入后板卡数据）
正向时可以采纳并创建正式 `doc-rvv` 文档；正式文档中的性能数据必须采用接入后的板卡结果，而不是 Phase 001
的 diagnostic evidence（诊断证据）。本阶段已创建 `doc-rvv/features/fpfh-RVV.zh.md`。本轮没有创建 commit。

## 计划回填

| action | 状态 | 结果 |
| --- | --- | --- |
| A1 asm attribution investigation | done | `make -B -C test-rvv/features/fpfh dump_bench_rvv` 生成 `build/asm/riscv/bench_fpfh_rvv.full.asm`；`addr2line` 将 RVV load/FMA 和 normalization 指令映射到 `features/include/pcl/features/impl/fpfh.hpp` 的 production helper。 |
| A2 QEMU correctness freshness | done | `make -B -C test-rvv/features/fpfh run_test_compare`：Std 6/6 pass，RVV 6/6 pass。新增 remapped row regression（重映射行回归测试）证明生产 helper 不依赖连续行。 |
| A3 board smoke | done | `make -C test-rvv/features/fpfh board_smoke`：板端 unit test 6/6 pass；single smoke 中 `component_weighted_spfh_33` 约 4.70x，`public_fpfh_k` 约 1.24x。smoke 只证明可运行和方向，不作为最终统计。 |
| A4 board repeated | done | `make -C test-rvv/features/fpfh board_repeated REPEATED_BOARD_OUTPUT_DIR=log/board/pi1-production-weighted/repeated` 完成 5 runs。 |
| A5 Evidence Doctor | done | repeated manifest + Doctor 完成，`Errors=0, Warnings=0, Suggestions=9`。Suggestions 为环境 metadata、binary identity 和 `component_spfh_signature` near-threshold，不阻塞本阶段结论。 |
| A6 文档刷新 | done | 已刷新 phase index、optimization matrix、roadmap、evaluation、topic README，并创建正式 `doc-rvv/features/fpfh-RVV.zh.md`。 |

## 正确性证据

命令：

```bash
make -B -C test-rvv/features/fpfh run_test_compare
```

结果摘要：

| build | 测试数 | 结果 | 证据路径 |
| --- | --- | --- | --- |
| Std | 6 | pass | `test-rvv/features/fpfh/log/qemu/run_test_std.log` |
| RVV | 6 | pass | `test-rvv/features/fpfh/log/qemu/run_test_rvv.log` |

新增的 `FPFHReference.WeightsSPFHWithRemappedRowsLikeProductionHelper` 使用非连续 SPFH rows
`{0, 12, 4, 15, 2, 9, 6}`，覆盖 production lookup（生产查表）后的 row remap 形态。它避免把 Phase 001
dense-row candidate 的诊断前提误写成 production gate。

QEMU（仿真器）只作为 correctness（正确性）和路径形状证据，不作为性能结论。

## 反汇编归因

命令：

```bash
make -B -C test-rvv/features/fpfh dump_bench_rvv
riscv64-unknown-linux-gnu-addr2line -Cfipe test-rvv/features/fpfh/build/riscv/bench_fpfh_rvv \
  0x30608 0x307aa 0x307b4 0x307b8 0x3097a 0x30980 0x30984 0x3099e 0x309a4 0x309a8
```

归因摘要：

| 地址 / 指令区域 | 归属 | 含义 |
| --- | --- | --- |
| `0x30608` | `features/include/pcl/features/impl/fpfh.hpp:73`, inlined by `weightPointSPFHSignature` | production RVV helper 被编入真实 helper。 |
| `0x307aa`, `0x307b4`, `0x307b8` | `fpfh.hpp:94-96` | `vlse32`（跨步加载）和 FMA（融合乘加）区域归到 production helper。 |
| `0x3097a` 到 `0x309a8` | `fpfh.hpp:114-115` | 33-bin segment normalization（分段归一化）的 RVV `vfmul` 区域归到 production helper。 |

同时，test-only candidate 的向量指令仍归到 `test-rvv/features/fpfh/include/impl/fpfh_weighted_candidate.hpp`，
不会被写成 production evidence（生产证据）。

## 板卡证据

repeated board（重复板卡测试）命令：

```bash
make -C test-rvv/features/fpfh board_repeated \
  REPEATED_BOARD_OUTPUT_DIR=log/board/pi1-production-weighted/repeated
```

Manifest 由 `test-rvv/features/fpfh/script/generate_fpfh_evidence_manifest.py` 生成。数据集是
`synthetic fpfh point-normal grid side=48 points=2304 k=32`，每次 8 iterations、2 warmup，合计 5 runs。

| case | evidence role（证据角色） | Std avg | RVV avg | speedup values | avg speedup | degradation |
| --- | --- | --- | --- | --- | --- | --- |
| `component_weighted_spfh_33` | `production_detail` | 35.41242 ms | 7.37270 ms | 5.06x, 4.85x, 4.57x, 4.85x, 4.69x | 4.804x | 0/5 |
| `public_fpfh_k` | `production_public` | 146.46720 ms | 116.02340 ms | 1.27x, 1.25x, 1.27x, 1.27x, 1.25x | 1.262x | 0/5 |
| `candidate_weighted_spfh_dense_rows` | `diagnostic` | 12.72412 ms | 6.84142 ms | 1.93x, 1.86x, 1.80x, 1.87x, 1.83x | 1.858x | 0/5 |
| `component_spfh_signature` | `production_shaped_diagnostic` | 4.46396 ms | 4.41596 ms | 1.01x, 1.00x, 1.04x, 1.00x, 1.00x | 1.010x | 0/5 |

采纳判断只使用 `production_detail` 和 `production_public` 两类证据。`candidate_weighted_spfh_dense_rows`
保留为 historical diagnostic（历史诊断）对照；`component_spfh_signature` 是未优化组件的稀释检查。

## Evidence Doctor

命令：

```bash
make -C test-rvv/features/fpfh evidence_doctor_repeated \
  REPEATED_BOARD_OUTPUT_DIR=log/board/pi1-production-weighted/repeated \
  EVIDENCE_MANIFEST_REPEATED=log/board/pi1-production-weighted/repeated/evidence_manifest.json \
  EVIDENCE_DOCTOR_REPEATED_MD=log/board/pi1-production-weighted/repeated/evidence_doctor.md \
  EVIDENCE_DOCTOR_REPEATED_JSON=log/board/pi1-production-weighted/repeated/evidence_doctor.json
```

结果：`Errors=0, Warnings=0, Suggestions=9`。

Suggestions 不阻塞本阶段结论，但边界如下：

- board summary 缺少 taskset、governor、freq、temperature 等环境 metadata（元数据）。
- summary 缺少 binary hash（用于确认二进制身份的哈希）或等价字段。
- `component_spfh_signature` median 接近 1.0，只能说明该未优化组件当前基本中性，不能写成稳定优化。

若后续数值方向反转，优先补齐环境 metadata 和 binary identity 后再重跑。

## EvidenceDecision

| 决策问题 | 结论 |
| --- | --- |
| RVV-vs-scalar production detail | `component_weighted_spfh_33` repeated 平均 4.804x，0/5 degradation，支持保留。 |
| public path dilution | `public_fpfh_k` repeated 平均 1.262x，0/5 degradation，说明 detail 收益在当前公开入口数据集上没有被 search 完全抵消。 |
| correctness / fallback | Std/RVV 6/6 pass；remapped row regression 覆盖 arbitrary valid row indices；gate 不满足时回标量。 |
| asm attribution | RVV 指令归到 `fpfh.hpp` production helper，不再是 include order 下的 stale installed header（旧安装头文件）或 test-only helper。 |
| Doctor | 0 Error / 0 Warning；Suggestions 已解释。 |

最终 decision：`adopted_with_bounded_scope`。

## 覆盖范围和不能外推的边界

已覆盖：

- `FPFHEstimation<PointNormal, PointNormal, FPFHSignature33>` 的 `weightPointSPFHSignature` 生产 helper。
- `float`、33-bin contiguous output、SPFH matrix 三段各 11 bins。
- production lookup 后任意有效 SPFH row indices。
- synthetic KSearch public case：side=48、points=2304、k=32，Milkv-Jupiter board。

未覆盖：

- `computePointSPFHSignature` 的 pair-feature batch（点对特征批处理）。
- OMP FPFH、PFH / VFH sibling family、`Scalar=double`。
- custom bin count、非 `FPFHSignature33` output layout。
- 泛型点类型 traits/layout 扩展和真实工作负载分布。

## 文档和登记状态

| area | 状态 |
| --- | --- |
| phase result | 本文件为本阶段事实主归属。 |
| optimization matrix | 已刷新 `doc/phases/optimization-matrix.zh.md`。 |
| roadmap | 已刷新 `doc/optimization-roadmap.zh.md`，下一候选为 `spfh-pair-feature-batch`。 |
| evaluation | 已刷新 `doc/fpfh-evaluation.zh.md`，记录 production decision。 |
| production topic doc | 已创建 `doc-rvv/features/fpfh-RVV.zh.md`。 |
| evidence registry | 当前 topic 无维护中的 `log/evidence_registry.json`；本阶段人工检查 `log/board/pi1-production-weighted/repeated` 下 manifest、Doctor 和 5 个 run summary。raw logs 默认不提交。 |

## 下一步

当前 phase 内没有必须继续的未阻塞动作。若用户要求继续优化，下一 phase 应先为
`spfh-pair-feature-batch` 写 plan，并审计 `computePairFeatures` 中的 `atan2`、`acos`、normalization
和 histogram scatter 的数值语义与可用 RVV math helper。generic point type、OMP 和 `Scalar=double`
扩展需要单独 phase，不应混入当前 adopted patch。
