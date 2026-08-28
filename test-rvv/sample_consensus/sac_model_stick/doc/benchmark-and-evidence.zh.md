# sac_model_stick benchmark and evidence

## 本文职责

本文说明 bench（性能测试）输出、case label（用例标签）、board repeated（板卡重复采集）、manifest（证据清单）、Evidence Doctor（证据体检）、asm attribution（反汇编归属）和 evidence registry（证据登记表）。正确性测试语义见 `doc/correctness-tests.zh.md`。

## Bench 输出格式

`src/bench_sac_model_stick.cpp` 每次运行输出数据集、iteration（迭代次数）、warmup（预热次数）、build 类型、checksum 和三条 public entry（公开入口）计时行：

| 输出行 | 证据角色 | 计时边界 |
| --- | --- | --- |
| `public countWithinDistance` | production direct | 公开入口调用，不含点云、indices、系数构造。 |
| `public selectWithinDistance` | production direct | 公开入口调用，含输出容器写回。 |
| `public getDistancesToModel` | production direct | 公开入口调用，含 dense distance（稠密距离）输出。 |

`BENCH_ARGS` 默认为 `65536 200`，即 65536 点、200 次计时迭代，warmup 固定为 5。checksum 是输出形状和少量代表值的轻量 guard（防误用检查），不是性能或完整正确性证据。

## Target 和脚本

| 入口 | 作用 | 输出 |
| --- | --- | --- |
| `make ... dump_bench_rvv` | 构建 RVV bench binary 并生成反汇编。 | `build/asm/riscv/bench_sac_model_stick_rvv.full.asm` 和 `.asm`。 |
| `make ... check_production_asm` | 检查三个 production RVV helper 中的 RVV 指令归属。 | 成功输出为空；失败时列缺失符号或指令。 |
| `make ... board_smoke` | 部署 Std/RVV bench 与 RVV test 到板卡，运行一次 test 和 bench compare。 | `log/board/` 下的远端拉回日志。 |
| `make ... collect_repeated_board_evidence` | 连续 5 次运行 diagnostic board smoke。 | `log/board/repeated-20260828-phase040/run-01..05` 或命令行覆盖目录。 |
| `make ... collect_production_repeated_board_evidence` | 连续 5 次运行 production board smoke。 | `log/board/repeated-20260828-phase080-production/run-01..05` 或命令行覆盖目录。 |
| `make ... collect_vector_writeback_board_evidence` | 连续 5 次运行 Phase 100 getDistances vector writeback production board smoke。 | `log/board/repeated-20260828-phase100-vector-writeback/run-01..05` 或命令行覆盖目录。 |
| `make ... generate_board_evidence_manifest` | 从 diagnostic repeated logs 和 asm 生成 manifest。 | `doc/phases/040-stick-getdistances-diagnostic/repeated-evidence-manifest.json` 等。 |
| `make ... generate_production_board_evidence_manifest` | 从 Phase 080 production repeated logs 和 asm 生成 manifest。 | `doc/phases/080-stick-production-integration/production-repeated-evidence-manifest.json`。 |
| `make ... generate_vector_writeback_board_evidence_manifest` | 从 Phase 100 repeated logs 和 asm 生成 public getDistances manifest。 | `doc/phases/100-stick-getdistances-vector-writeback/vector-writeback-evidence-manifest.json`。 |
| `make ... run_production_board_evidence_doctor` | 对 production manifest 运行 Evidence Doctor。 | `production-repeated-evidence-doctor.md` 和 `.json`。 |
| `make ... record_production_board_evidence_state` | 运行 production doctor 并登记 registry。 | `log/evidence_registry.json`。 |
| `make ... record_vector_writeback_board_evidence_state` | 运行 Phase 100 doctor 并登记 registry。 | `log/evidence_registry.json`。 |
| `make ... production_evidence_status` | 检查 Phase 080 summary evidence 的登记文件、scan-glob 和文档引用。 | `evidence registry check: fresh` 或问题列表。 |
| `make ... vector_writeback_evidence_status` | 检查 Phase 100 summary evidence 的登记文件、scan-glob 和文档引用。 | `evidence registry check: fresh` 或问题列表。 |

`run_bench_compare` 在 QEMU 侧默认被 guard（保护门）挡住；只有显式设置 `ALLOW_QEMU_BENCH_COMPARE=1` 才能跑，且只能作为 log-shape smoke（日志形状小型验证），不能写成性能结论。

## Manifest focus 字典

`script/generate_stick_board_evidence_manifest.py` 用 `--focus` 选择要进入当前 phase manifest 的计时行：

| focus | 纳入行 | 当前 phase |
| --- | --- | --- |
| `count` | diagnostic count public / candidate 行 | Phase 000 |
| `select` | diagnostic select public / candidate 行 | Phase 020 |
| `getdistances` | diagnostic getDistances public / candidate 行 | Phase 040 |
| `production` | 三条 public entry 行 | Phase 080 |
| `production-getdistances` | public `getDistancesToModel` 行 | Phase 100 |
| `all` | 当前 bench 全部 public 行 | 调试用，不作为 phase closeout 默认输入。 |

## 当前 board summary

| phase | run label | speedup min / median / max | Evidence Doctor | 处理动作 |
| --- | --- | --- | --- | --- |
| Phase 000 count diagnostic | `stick-phase000-repeated-board` | 4.1429x / 4.1767x / 4.2169x | Errors=0、Warnings=1、Suggestions=1 | public weak cross-check 降级；candidate clean。 |
| Phase 020 select diagnostic | `stick-phase020-repeated-board` | 3.4208x / 3.4432x / 3.4761x | Errors=0、Warnings=1、Suggestions=1 | public weak cross-check 降级；candidate clean。 |
| Phase 040 getDistances diagnostic | `stick-phase040-repeated-board` | 2.5416x / 2.5553x / 2.7849x | Errors=1、Warnings=0、Suggestions=0 | public negative cross-check 降级；candidate clean。 |
| Phase 080 public count production | `stick-phase080-production-repeated-board` | 4.1331x / 4.1729x / 4.1756x | Errors=0、Warnings=0、Suggestions=0 | production adopted。 |
| Phase 080 public select production | `stick-phase080-production-repeated-board` | 3.2085x / 3.3023x / 3.3860x | Errors=0、Warnings=0、Suggestions=0 | production adopted。 |
| Phase 080 public getDistances production | `stick-phase080-production-repeated-board` | 2.5722x / 2.5883x / 2.7872x | Errors=0、Warnings=0、Suggestions=0 | production adopted。 |
| Phase 100 public getDistances vector writeback | `stick-phase100-vector-writeback-board` | 3.4122x / 3.6879x / 3.7150x | Errors=0、Warnings=0、Suggestions=0 | production adopted；Phase 080 数字仅作 historical baseline（历史基线）。 |

这些性能结论只来自 board / target hardware（板卡或目标硬件）。QEMU 不参与速度判断。

## ASM Attribution 口径

| helper | 关键指令 / 模式 | 证明内容 |
| --- | --- | --- |
| `countWithinDistanceRVV` | indexed load、`vfmacc.vv`、mask compare、`vmandn.mm`、两次 `vcpop.m` | 生产 RVV 路径实际批量计算平方距离和内外圈计数。 |
| `selectWithinDistanceRVV` | indexed load、`vfmacc.vv`、mask compare、`vcpop.m`、`vcompress.vm` | 生产 RVV 路径实际压缩输出 inliers 和平方距离暂存。 |
| `getDistancesToModelRVV` | indexed load、`vfmacc.vv`、`vfsqrt.v`、`vfwcvt.f.f.v`、`vse64.v` | 生产 RVV 路径实际批量计算距离，用 mask / merge 应用 penalty，并向量写回 double 输出。 |
| `*CandidateRVV` | 同类指令 | 保留为历史 diagnostic（诊断）回归证据。 |

## 可提交证据和排除项

可提交 summary evidence：

- `doc/phases/000-stick-count-diagnostic/repeated-evidence-manifest.json`
- `doc/phases/000-stick-count-diagnostic/repeated-evidence-doctor.md`
- `doc/phases/000-stick-count-diagnostic/repeated-evidence-doctor.json`
- `doc/phases/020-stick-select-diagnostic/repeated-evidence-manifest.json`
- `doc/phases/020-stick-select-diagnostic/repeated-evidence-doctor.md`
- `doc/phases/020-stick-select-diagnostic/repeated-evidence-doctor.json`
- `doc/phases/040-stick-getdistances-diagnostic/repeated-evidence-manifest.json`
- `doc/phases/040-stick-getdistances-diagnostic/repeated-evidence-doctor.md`
- `doc/phases/040-stick-getdistances-diagnostic/repeated-evidence-doctor.json`
- `doc/phases/080-stick-production-integration/production-repeated-evidence-manifest.json`
- `doc/phases/080-stick-production-integration/production-repeated-evidence-doctor.md`
- `doc/phases/080-stick-production-integration/production-repeated-evidence-doctor.json`
- `doc/phases/100-stick-getdistances-vector-writeback/vector-writeback-evidence-manifest.json`
- `doc/phases/100-stick-getdistances-vector-writeback/vector-writeback-evidence-doctor.md`
- `doc/phases/100-stick-getdistances-vector-writeback/vector-writeback-evidence-doctor.json`
- `log/evidence_registry.json`

提交前需要精确 stage（加入暂存区）当前 topic 的允许文件；`log/evidence_registry.json` 被
`test-rvv/.gitignore` 忽略，必须用 `git add -f`。若提交 summary evidence（摘要证据），也应逐项
加入上述 manifest / Evidence Doctor 文件，避免把 raw board logs、QEMU logs 或 `build/` 带入提交。

默认排除：`log/board/repeated-*` raw logs（原始板卡日志）、`log/qemu/*.log`、`build/`、本机 `config.mk` 和远端私有路径。

## 当前结论边界

Phase 080 board repeated 支持三个 public entry 的 initial production-adopted 判断。Phase 100 是当前 public getDistances 实现形态的性能证据，支持 vector writeback patch 保留。当前性能证据覆盖 `PointXYZ` direct indexed 输入；其它满足 traits gate 的点型需要 dedicated point-type board performance phase 后才能写成同等性能结论。
