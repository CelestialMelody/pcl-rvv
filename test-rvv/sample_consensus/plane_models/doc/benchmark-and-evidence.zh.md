# plane_models normal-plane bench 与证据说明

本文说明 benchmark（性能测试）、board evidence（板卡证据）、Evidence Doctor（证据体检）、manifest（证据清单）和提交边界。correctness 测试语义见 `correctness-tests.zh.md`。

## Bench 输出格式和计时边界

`src/bench_sac_normal_plane.cpp` 直接调用 `SampleConsensusModelNormalPlaneBench` 暴露的 protected helper（受保护 helper）：

- `selectWithinDistanceStandard` / `selectWithinDistanceRVV`
- `countWithinDistanceStandard` / `countWithinDistanceRVV`
- `getDistancesToModelStandard` / `getDistancesToModelRVV`

bench 先 warm up（预热）3 次，再按参数迭代计时。计时边界包括 helper 主循环、gather（离散加载）、mask（掩码）、`vcompress` 写回、距离写回和 count reduction（计数规约），不包括 PCD 读取和模型构造。`selectWithinDistance` case 每次迭代会重置 `inliers` 与 `error_sqr_dists_` 缓冲区，避免复用旧输出污染结果。

输出 item 固定为：

| item | 入口 | 证据角色 |
| --- | --- | --- |
| `selectWithinDistance` | protected helper hot path | production-shaped diagnostic；证明 helper 性能，不单独证明公开入口 dispatch。 |
| `countWithinDistance` | protected helper hot path | production-shaped diagnostic；证明 mask popcount 路径性能。 |
| `getDistancesToModel` | protected helper hot path | production-shaped diagnostic；证明 dense distance write 路径性能。 |

默认模式保持上述三项 label，用于 Phase 030 旧 repeated summary。Phase 050 新增显式
`--representative-aos-sources` 模式，输出 `PointXYZ`、`PointXYZI` 和 `PointXYZINormal`
三种 source 点型的 9 个带后缀 case。该模式只在 Phase 050 target 中使用，避免覆盖旧证据语义。

## CLI 参数和 case-filter

| 可执行文件 | 参数 | 当前用途 |
| --- | --- | --- |
| `bench_sac_normal_plane_std` / `_rvv` | `sac_plane_test.pcd [iters]` | Std/RVV helper compare，默认 board target 使用板卡侧 PCD 路径。 |
| `bench_sac_normal_plane_std` / `_rvv` | `sac_plane_test.pcd [iters] --representative-aos-sources` | Phase 050 代表性 AoS source 点型性能矩阵；输出 9 个独立 case。 |
| `bench_sac_normal_plane_load_compare` | `sac_plane_test.pcd [iters]` | 历史 RVV load strategy probe，只在显式 `run_bench_load` 时运行。 |

当前没有 case-filter 参数；三条 helper 在同一个 bench binary 中顺序输出。若后续新增 RVV-vs-RVV A/B（同一 RVV 二进制内候选对比），应在 phase 020 或独立 phase 增加 case-filter 或 manifest wrapper。

## 推荐 target

```bash
make -C test-rvv/sample_consensus/plane_models run_board_bench_compare fetch_board_logs
make -C test-rvv/sample_consensus/plane_models dump_bench_rvv
make -C test-rvv/sample_consensus/plane_models run_board_evidence_doctor
make -C test-rvv/sample_consensus/plane_models record_board_evidence_state
make -C test-rvv/sample_consensus/plane_models evidence_status
make -C test-rvv/sample_consensus/plane_models run_board_bench_compare_repeated
make -C test-rvv/sample_consensus/plane_models record_repeated_board_evidence_state
make -C test-rvv/sample_consensus/plane_models repeated_evidence_status
make -C test-rvv/sample_consensus/plane_models run_board_bench_compare_phase050
make -C test-rvv/sample_consensus/plane_models record_phase050_evidence_state
make -C test-rvv/sample_consensus/plane_models phase050_evidence_status
```

`make run_bench_compare` 在 QEMU 侧由共享规则 guard（保护条件）阻止作为性能 compare；QEMU 只用于 correctness 和日志形状。真实性能结论只引用 board 或目标硬件。

## 当前 board 证据

当前 board compare summary 位于：

```text
test-rvv/sample_consensus/plane_models/log/board/analyze_bench_compare.log
```

phase 000 manifest 引用的 rerun 结果为：

| item | Std Avg | RVV Avg | speedup |
| --- | ---: | ---: | ---: |
| `selectWithinDistance` | 0.6845 ms | 0.0699 ms | 9.79x |
| `countWithinDistance` | 0.6797 ms | 0.0590 ms | 11.52x |
| `getDistancesToModel` | 0.7702 ms | 0.0732 ms | 10.52x |

run_count=1，decision bucket 稳定为 positive。该结果支持 phase 000 保留当前 production patch；phase 030 已补充 repeated board summary（重复板卡摘要）来降低偶然波动风险。

phase 030 repeated board summary 位于：

```text
test-rvv/sample_consensus/plane_models/log/board/normal-plane-phase030-repeated-board/summary.md
```

| item | runs | median speedup | min speedup | max speedup |
| --- | ---: | ---: | ---: | ---: |
| `selectWithinDistance` | 5 | 9.64x | 9.24x | 10.03x |
| `countWithinDistance` | 5 | 12.72x | 12.56x | 12.87x |
| `getDistancesToModel` | 5 | 12.02x | 11.66x | 12.36x |

三条 helper 的 median speedup 均大于 1.2x，min speedup 均大于 1.0x。按 phase 030 plan 的有界复跑预算和 decision bucket 规则，该 repeated evidence 记为 `positive-stable`。

phase 040 新增 `PointXYZI` / `PointXYZINormal` 代表性 source 点型 public correctness 和 non-AoS source fallback。phase 050 已补 dedicated board performance phase，并把新增 source 点型的 protected helper hot path 性能单独登记。

phase 050 repeated board summary 位于：

```text
test-rvv/sample_consensus/plane_models/log/board/normal-plane-phase050-representative-aos-source-performance/summary.md
```

| item | runs | median speedup | min speedup | max speedup |
| --- | ---: | ---: | ---: | ---: |
| `selectWithinDistance_PointXYZI_Normal` | 5 | 8.04x | 7.40x | 8.46x |
| `countWithinDistance_PointXYZI_Normal` | 5 | 6.10x | 6.05x | 6.13x |
| `getDistancesToModel_PointXYZI_Normal` | 5 | 5.41x | 4.49x | 6.06x |
| `selectWithinDistance_PointXYZINormal_Normal` | 5 | 7.37x | 6.60x | 7.55x |
| `countWithinDistance_PointXYZINormal_Normal` | 5 | 8.52x | 7.88x | 8.94x |
| `getDistancesToModel_PointXYZINormal_Normal` | 5 | 8.44x | 8.22x | 8.63x |

Phase 050 Evidence Doctor 为 Errors=0、Warnings=5、Suggestions=0。Warnings 来自组内收益差异和
`PointXYZI getDistancesToModel` 长尾；当前处理策略是逐点型、逐 helper 报告，不用组均值外推。

## Evidence Doctor / Manifest 边界

| 文件 | 作用 | 当前状态 |
| --- | --- | --- |
| `doc/phases/000-normal-plane-current-state-and-public-entry-boundary/evidence-manifest.json` | 机器可读 Evidence Doctor 输入。 | refreshed；包含三条 comparison、board/qemu/asm 路径和 evidence role。 |
| `doc/phases/000-normal-plane-current-state-and-public-entry-boundary/evidence-doctor.md` | 可提交摘要报告。 | Errors=0、Warnings=0、Suggestions=0。 |
| `doc/phases/000-normal-plane-current-state-and-public-entry-boundary/evidence-doctor.json` | 机器可读 doctor 输出。 | 可供 reviewer 复核。 |
| `log/evidence_registry.json` | topic-local registry（证据登记表）。 | phase 020 已登记 board analyze summary、manifest、doctor md/json；`evidence_status` 输出 fresh。 |
| `log/board/normal-plane-phase030-repeated-board/evidence-manifest.json` | repeated board Evidence Doctor 输入。 | 包含 3 条 comparison，每条 5 个 speedup 值。 |
| `log/board/normal-plane-phase030-repeated-board/evidence-doctor.md` | repeated board Evidence Doctor 摘要。 | Errors=0、Warnings=0、Suggestions=0。 |
| `log/board/normal-plane-phase050-representative-aos-source-performance/evidence-manifest.json` | representative source repeated board Evidence Doctor 输入。 | 包含 9 条 comparison，每条 5 个 speedup 值。 |
| `log/board/normal-plane-phase050-representative-aos-source-performance/evidence-doctor.md` | Phase 050 Evidence Doctor 摘要。 | Errors=0、Warnings=5、Suggestions=0；Warnings 在 phase result 中解释。 |

manifest 把 board compare 标为 `production_shaped_diagnostic`，因为 bench 直接调用 protected helper。公开入口 RVV 命中和 fallback 由 `run_normal_plane_public_tests`、`run_test_compare` 和 `run_board_test` 覆盖。

Phase 040、Phase 060 和 Phase 070 的新增证据是 `production-public correctness / fallback`。它们通过 `run_normal_plane_public_tests` 和 `run_board_normal_plane_public_tests` 证明公开入口分流、代表性交叉组合和回退行为，不进入性能排序，也不改变 repeated Evidence Doctor 的输入。Phase 070 后这两个 alias 覆盖 13 个 normal-plane public/fallback/buffer 用例。

topic-local wrapper 位于 `script/generate_normal_plane_board_evidence_manifest.py`。该脚本只解析当前 normal-plane board bench logs，并把 protected helper bench 保持为 production-shaped diagnostic（生产形态诊断），不把它升级成完整公开入口性能证据。

同一 wrapper 也支持 `--compare-log` 输入，可从 `log/board/normal-plane-phase030-repeated-board/run_*/analyze_bench_compare.log` 生成 repeated manifest；Phase 050 使用 `--case-set representative-aos` 解析带点型后缀的 9 个 case。`repeated_evidence_status` 和 `phase050_evidence_status` 已输出 fresh；raw run logs 不进入默认提交边界。

## ASM Attribution 口径

`dump_bench_rvv` 输出：

- `build/asm/riscv/bench_sac_normal_plane_rvv.full.asm`
- `build/asm/riscv/bench_sac_normal_plane_rvv.asm`

phase 000 统计目标符号内 RVV 指令行：

| symbol | RVV instruction lines |
| --- | ---: |
| `selectWithinDistanceRVV` | 56 |
| `countWithinDistanceRVV` | 77 |
| `getDistancesToModelRVV` | 56 |

这证明目标 helper 符号内存在 RVV 指令；性能方向仍以 board compare 为准。

## 提交边界

可提交候选仅包含被 README、evaluation、phase result 或长期 `doc-rvv` 明确引用的 summary artifact（摘要证据产物）：phase result、manifest、doctor report 和 topic-local role docs。默认排除：

- `build/`
- `output/`
- `log/board/*.log`
- `log/qemu/*.log`
- `log/vec_missed_log/`
- 本机 `config.mk`
- 私有板卡地址、用户名和个人绝对路径

若用户要求提交日志，先运行 sanitize（脱敏）和 check target，并把日志 commit 与 topic 源码 / 文档 commit 分开。
