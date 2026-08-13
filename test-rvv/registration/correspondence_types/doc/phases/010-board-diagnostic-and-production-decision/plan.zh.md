# Phase 010 计划：board-diagnostic-and-production-decision

## 阶段意图和边界

本阶段接续 Phase 000，使用 board / target hardware（板卡或目标硬件）验证 `registration/correspondence_types` 的诊断候选是否有真实性能价值，并给出 production integration loop（生产接入闭环）前的判断。

本阶段只做：

- 部署并运行 `test-rvv/registration/correspondence_types` 的 board correctness（板卡正确性）和 board bench（板卡性能测试）。
- 把板卡日志转换成 run-labelled summary / manifest / Evidence Doctor（证据体检）结果。
- 更新 phase result、optimization matrix（优化矩阵）、evaluation（函数级评估）和 topic-local closeout（主题本地收尾）文档；只有证据支持 production 接入后才发布 `doc-rvv` 长期主题文档。

本阶段不修改 `registration/include/pcl/registration/impl/correspondence_types.hpp`。如果板卡结果正向，只输出 PI1 建议或下一阶段计划；production patch（生产补丁）需要后续明确授权。

## 当前状态清单

| 对象 | 状态 | 证据 |
| --- | --- | --- |
| Phase 000 | `completed_diagnostic` | `doc/phases/000-current-state-and-gaps/result.zh.md` |
| QEMU correctness | pass | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| QEMU Evidence Doctor | pass | `log/qemu/evidence_manifest.json`、`log/qemu/evidence_doctor.md` |
| asm smoke | partial pass | `build/asm/riscv/bench_correspondence_types_rvv.asm` |
| board evidence | missing | 本阶段补齐 |
| production source | unchanged | `registration/include/pcl/registration/impl/correspondence_types.hpp` |

## 候选族和测试矩阵

| candidate family | row source policy | scope | board target | decision need |
| --- | --- | --- | --- | --- |
| strided-index-extract | correspondences（对应关系数组顺序扫描） | query/match 字段跨步加载和连续写出 | `run_board_bench_compare` with `--case-filter index-extract --iterations 20 --warmup-iterations 5` | 至少 `weak_positive` 才建议 PI1 |
| distance-stats-reduction | correspondences | distance float-square same-chain 诊断 | `run_board_bench_compare` with `--case-filter distance-stats --iterations 20 --warmup-iterations 5`，仅在综合结果需要解释时运行 | 默认不接 production；只辅助判断是否继续数值候选 |
| production-dispatch | production public helpers | 真实 helper 分流、fallback 和 production direct | not_run | 本阶段不写 production patch |

## 板卡复跑预算和决策桶

默认先跑一次 board compare，bench 程序内部每个 case 使用：

- warm-up：5 iterations
- measured iterations：20
- run count：1 个 board compare 批次

如果单次结果接近阈值、Evidence Doctor 提示异常、checksum 不一致或方向与 Phase 000 直觉冲突，允许同边界补跑 1 次。补跑仍不能无限扩大预算。

决策桶：

| bucket | 口径 |
| --- | --- |
| `positive` | 关键 index extraction case 全部明显 > 1.15x |
| `weak_positive` | 关键 index extraction case 主要在 1.03x-1.15x，且没有 checksum / doctor 阻塞 |
| `neutral` | 关键 case 落在 0.97x-1.03x |
| `negative` | 关键 case 明显 < 0.97x |
| `unstable` | 补跑后跨桶摇摆或 evidence contract 不可解释 |

本阶段可以把 distance stats 写成 diagnostic insight（诊断线索），但不把它单独升级为 production candidate。

## Evidence Doctor 和 Manifest 计划

阶段输出使用 run-labelled board 目录，避免覆盖裸 `log/board/analyze_bench_compare.log` 后失去来源：

```text
log/board/010-board-diagnostic/
  run_bench_std.log
  run_bench_rvv.log
  analyze_bench_compare.log
  evidence_manifest.json
  evidence_doctor.md
```

需要新增或复用 topic-local manifest generator（证据清单生成器），要求：

- 记录 device / iterations / warmup_iterations / run_count / vlen / summary path。
- 每个 case 有 baseline 和 candidate checksum。
- `evidence_role` 使用 `diagnostic`，不写 production direct。
- `case_kind` 使用 `board_diagnostic`。
- 如果只能得到单次 board compare，doctor 结果可作为诊断证据，不写 repeated production performance。

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| B1 board 连接和部署 | `make check_board_ssh`、必要时 `make deploy_board` | 能部署 std/RVV bench 和 RVV test |
| B2 board correctness | `make run_board_test fetch_board_logs` 或 `make board_smoke` 的 test 部分 | board RVV test 6/6 通过 |
| B3 board index bench | `make run_board_bench_compare BENCH_ARGS="--case-filter index-extract --iterations 20 --warmup-iterations 5"` | 生成 board analyze log，Std/RVV checksum 一致 |
| B4 manifest / doctor | topic-local board manifest + `test-rvv/script/evidence_doctor.py` | doctor Errors=0；Warnings 被解释 |
| B5 文档回填 | result、matrix、evaluation、topic doc、roadmap | EvidenceDecision 不超过板卡和诊断边界 |

## 继续 / 停止条件

继续到 PI1 的条件：

- board correctness 通过；
- index extraction board bucket 至少 `weak_positive`；
- Evidence Doctor 没有未处理 Error；
- 用户明确授权 production integration loop。

停止或降级条件：

- board correctness 或 checksum 不一致；
- index extraction board bucket 为 `neutral` / `negative`；
- Evidence Doctor Error 不能修复；
- 继续会修改 production，而用户未授权。

## 文档更新清单

- `doc/phases/010-board-diagnostic-and-production-decision/result.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/correspondence_types-evaluation.zh.md`
- `doc-rvv/registration/correspondence_types-RVV.zh.md`：仅当板卡证据支持 production 接入并进入 production integration loop 后适用；若结论为 no-production，本阶段不创建或更新该文件。
