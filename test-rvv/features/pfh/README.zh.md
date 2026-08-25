# PFH RVV Topic

本目录保存 `features/include/pcl/features/impl/pfh.hpp` 的 RVV 测试、benchmark（性能测试）和
phase 文档。当前 production（生产）状态：direct AoS RVV helper 已采纳两个 exact 组合：
`PointNormal -> PointNormal` 和 `PointXYZ -> Normal`，均只覆盖 `PFHSignature125`、`nr_split=5`、
`use_cache_ == false`、邻域规模不少于 4 且 32-bit byte offset 可表达的路径。

## 阅读入口

| 入口 | 作用 |
| --- | --- |
| `doc/pfh-evaluation.zh.md` | 函数级评估、生产接入判断、fallback 矩阵和 EvidenceDecision。 |
| `doc/phases/README.zh.md` | phase loop（阶段循环）索引和默认恢复入口。 |
| `doc/phases/optimization-matrix.zh.md` | candidate family、点型范围、证据和决策矩阵。 |
| `doc/optimization-roadmap.zh.md` | 后续优化方向、暂缓条件和停止理由。 |
| `doc-rvv/features/pfh-RVV.zh.md` | 已采纳 production 行为的长期维护文档。 |

## 常用命令

| 命令 | 证据角色 |
| --- | --- |
| `make -B -C test-rvv/features/pfh run_test_compare` | Std/RVV correctness（正确性）汇总；当前 Std 3/3、RVV 6/6 pass。 |
| `make -B -C test-rvv/features/pfh dump_bench_rvv` | RVV bench binary 与 production helper 反汇编归属检查。 |
| `make -C test-rvv/features/pfh board_smoke BENCH_ARGS='--side 32 --k 32 --iterations 3 --warmup 1'` | 板卡快速冒烟，不作为 repeated 性能结论。 |
| `make -C test-rvv/features/pfh board_repeated BENCH_ARGS='--side 32 --k 32 --iterations 8 --warmup 2' REPEATED_BOARD_OUTPUT_DIR=log/board/<label>/repeated` | 板卡 repeated benchmark；性能结论只来自这类目标硬件证据。 |
| `make -C test-rvv/features/pfh evidence_doctor_repeated REPEATED_BOARD_OUTPUT_DIR=log/board/<label>/repeated` | 为 repeated summary 生成 manifest 和 Evidence Doctor（证据体检）。 |

## 证据提交边界

默认 evidence policy（证据策略）是 `summary-only`。`log/board`、`log/qemu`、`build` 和 raw run log
默认留在本机，不自动提交。若后续需要提交 evidence summary，应只用 `git add -f` 精确选择被文档引用且已脱敏的
summary / manifest / doctor / registry 文件。
