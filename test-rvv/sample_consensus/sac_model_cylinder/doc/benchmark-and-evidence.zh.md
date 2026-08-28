# sac_model_cylinder benchmark 与证据

## 本文职责

本文记录 `sac_model_cylinder` topic 的 bench（性能测试）输出合同、QEMU / board（仿真器 / 板卡）边界、
Evidence Doctor（证据体检）、manifest（证据清单）和 evidence registry（证据登记表）。性能结论只来自板卡或目标硬件；
QEMU timing（仿真计时）只用于日志形状调试。

## Bench 输出格式和 CLI

bench binary 由 `src/bench_sac_model_cylinder.cpp` 提供，默认参数为：

```bash
BENCH_ARGS='65536 200 shuffled'
```

| 参数 | 含义 | 当前默认 |
| --- | --- | --- |
| points | 合成圆柱点数。 | `65536` |
| iterations | 计时迭代次数。 | `200` |
| index mode | `identity` 或 `shuffled`。 | `shuffled`，相邻 pair 交换。 |

bench 输出包含五条基础 timing 行，并在 Phase 040 增加三组代表点型的 public timing 行：

| label | 证据角色 | 计时边界 |
| --- | --- | --- |
| `public countWithinDistance` | production direct | 真实公开计数入口。 |
| `diagnostic candidate countWithinDistance` | historical production-shaped diagnostic | Phase 000 测试专用候选交叉检查。 |
| `public selectWithinDistance` | production direct | 真实公开选择入口和 `error_sqr_dists_` 写回。 |
| `diagnostic candidate selectWithinDistance` | historical production-shaped diagnostic | Phase 000 测试专用候选交叉检查。 |
| `public getDistancesToModel` | production direct | 真实公开 dense distance vector（连续距离数组）输出入口。 |
| `public PointXYZI+Normal count/select/getDistancesToModel` | production direct | 验证 source 点型 stride 扩展后的真实公开入口。 |
| `public PointXYZRGB+Normal count/select/getDistancesToModel` | production direct | 验证常见 RGB source 点型的真实公开入口。 |
| `public PointXYZ+PointNormal count/select/getDistancesToModel` | production direct | 验证 normal cloud 字段 offset 扩展后的真实公开入口。 |

点云、法线、indices 和模型系数构造不在计时边界内。Checksum（校验和）只保护输出规模、顺序和聚合指纹；
逐项误差一致性由 gtest 覆盖。

## 当前生产证据

| 证据 | 路径 / 命令 | 当前结果 |
| --- | --- | --- |
| QEMU correctness | `make -C test-rvv/sample_consensus/sac_model_cylinder run_test_compare` | Std/RVV 各 11 个 gtest 通过。 |
| QEMU bench smoke | `BENCH_ARGS='128 2 shuffled' ALLOW_QEMU_BENCH_COMPARE=1 make -C ... run_bench_compare` | 只证明日志格式和 checksum 行可运行，不作为性能证据。 |
| 反汇编归属 | `make -C test-rvv/sample_consensus/sac_model_cylinder clean_bench_rvv check_production_asm` | count 306 条、select 338 条、getDistances 298 条 RVV 指令；新增点型实例化后仍归属到三条 production helper。 |
| board repeated | `make -C test-rvv/sample_consensus/sac_model_cylinder collect_production_repeated_board_evidence` | 5-run 完成；每轮 11 个 gtest 通过。 |
| manifest | `doc/phases/020-cylinder-production-integration/production-repeated-evidence-manifest.json` | 12 个 production-public comparison，含基础点型和三组代表点型 5-run B/A。 |
| Evidence Doctor | `doc/phases/020-cylinder-production-integration/production-repeated-evidence-doctor.md` / `.json` | Errors=0、Warnings=0、Suggestions=0。 |
| registry | `log/evidence_registry.json` | run label `cylinder-phase020-production-repeated-board` 已登记并 fresh。 |

## Board repeated summary

| point type | entry | Std mean ms | RVV mean ms | median | min | max | decision bucket |
| --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| `PointXYZ + Normal` | `countWithinDistance` | `11.656673` | `2.167619` | `5.3124x` | `5.1606x` | `5.6082x` | positive |
| `PointXYZ + Normal` | `selectWithinDistance` | `12.588158` | `2.726701` | `4.6445x` | `4.5348x` | `4.6579x` | positive |
| `PointXYZ + Normal` | `getDistancesToModel` | `15.146606` | `2.182286` | `6.9318x` | `6.8120x` | `7.0688x` | positive |
| `PointXYZI + Normal` | `countWithinDistance` | `11.715698` | `2.138036` | `5.5145x` | `5.3724x` | `5.5245x` | positive |
| `PointXYZI + Normal` | `selectWithinDistance` | `12.648999` | `2.826342` | `4.4819x` | `4.3949x` | `4.5514x` | positive |
| `PointXYZI + Normal` | `getDistancesToModel` | `15.144458` | `2.244163` | `6.7851x` | `6.5743x` | `6.8318x` | positive |
| `PointXYZRGB + Normal` | `countWithinDistance` | `11.698597` | `2.176810` | `5.3986x` | `5.2755x` | `5.4519x` | positive |
| `PointXYZRGB + Normal` | `selectWithinDistance` | `12.637544` | `2.851038` | `4.4373x` | `4.3543x` | `4.5229x` | positive |
| `PointXYZRGB + Normal` | `getDistancesToModel` | `15.145902` | `2.081939` | `7.3186x` | `7.1422x` | `7.3352x` | positive |
| `PointXYZ + PointNormal` | `countWithinDistance` | `11.715236` | `2.167212` | `5.4105x` | `5.3094x` | `5.4691x` | positive |
| `PointXYZ + PointNormal` | `selectWithinDistance` | `12.645315` | `2.977867` | `4.3314x` | `4.0728x` | `4.3635x` | positive |
| `PointXYZ + PointNormal` | `getDistancesToModel` | `15.195919` | `2.443617` | `6.2453x` | `6.0669x` | `6.2818x` | positive |

12 项 public comparison 都远高于 positive 阈值 `1.20x`，且 5/5 run 均大于 `1.0x`。板卡日志中出现远端
make clock skew（时钟偏移）warning，但没有导致编译、测试、bench、manifest 或 Doctor 异常。

## Evidence Doctor 和异常解释

当前 production Evidence Doctor 为 0/0/0。Phase 030 曾出现一次 getDistances checksum mismatch（校验和不一致）
历史 run：原因是 bench 直接 hash 浮点距离值，而 RVV float 中间量与标量 double 中间量允许 `1e-5` 级差异。
该 run 已降级为 historical contaminated run（历史污染运行），不参与性能结论。修复后 bench checksum 只保护
dense vector 输出规模，逐项数值等价由 `GetDistancesBenchShapedPublicEntryMatchesStandardHelper` 承担。

## 复现和提交边界

推荐复现命令：

```bash
make -C test-rvv/sample_consensus/sac_model_cylinder run_test_compare
make -C test-rvv/sample_consensus/sac_model_cylinder clean_bench_rvv check_production_asm
make -C test-rvv/sample_consensus/sac_model_cylinder record_production_board_evidence_state
make -C test-rvv/sample_consensus/sac_model_cylinder production_evidence_status
```

可提交候选：topic-local 源码、文档、production manifest、Evidence Doctor 摘要和 `log/evidence_registry.json`。
默认排除：`log/board/` raw logs、`log/qemu/`、`build/`、`script/__pycache__/`、本机配置和私有板卡路径。
