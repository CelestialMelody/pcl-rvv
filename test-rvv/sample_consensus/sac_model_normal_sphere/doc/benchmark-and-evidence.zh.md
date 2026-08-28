# sac_model_normal_sphere benchmark 与证据

## 本文职责

本文记录 bench CLI、case label、board summary、manifest、Evidence Doctor 和 evidence registry 的证据边界。raw log（原始日志）默认本地保留，不作为提交候选。

## Bench 输出格式

`bench_sac_model_normal_sphere` 输出固定字段：

- `Dataset`：点型、规模和 row source（行来源）。
- `Iterations` / `Warmup Iterations`：计时迭代与 warmup（预热）次数。
- `Build`：`Std` 或 `RVV`。
- `Checksum`：由 public 和 candidate case 的返回值聚合而来，只用于同输入日志一致性检查。
- timing label：public entries、diagnostic candidates 和 `vcompress` candidate。

## CLI 参数

```bash
bench_sac_model_normal_sphere <points> <iterations> <PointXYZ|PointXYZI|PointXYZRGB|PointXYZRGBA>
```

`points` 控制 synthetic cloud 规模，`iterations` 控制计时循环次数，第三个参数控制 source 点型。所有点型都使用独立 `pcl::Normal` cloud；颜色和 intensity 字段只作为 source layout 负载。

## Case label 字典

| label | 证据角色 | 计时边界 | 不能证明 |
| --- | --- | --- | --- |
| `public selectWithinDistance` | Phase060 为 production-public（真实公开入口）证据；Phase000-030 为 mixed-boundary cross-check（混合边界交叉检查）。 | public overload 调用，不含输入构造。 | 不能外推到未测点型、其它硬件或真实 workload。 |
| `public countWithinDistance` | Phase060 为 production-public 证据；Phase000-030 为 mixed-boundary cross-check。 | public overload 调用，不含输入构造。 | 同上。 |
| `public getDistancesToModel` | Phase060 为 production-public 证据；Phase000-030 为 mixed-boundary cross-check。 | public overload 调用，不含输入构造。 | 同上。 |
| `diagnostic candidate selectWithinDistance` | historical production-shaped diagnostic（历史生产形态诊断）。 | 测试专用 select helper。 | 不作为当前生产性能 truth。 |
| `diagnostic candidate vcompress selectWithinDistance` | historical component ablation（历史组件消融）。 | 测试专用 `vcompress` 写回 helper。 | 不替代 Phase060。 |
| `diagnostic candidate countWithinDistance` | historical production-shaped diagnostic。 | 测试专用 count helper。 | 不作为当前生产性能 truth。 |
| `diagnostic candidate getDistancesToModel` | historical production-shaped diagnostic。 | 测试专用 dense double store helper。 | 不作为当前生产性能 truth。 |

## 当前 board 证据

| phase | summary | manifest | doctor | run label | 结论边界 |
| --- | --- | --- | --- | --- | --- |
| 000 | `doc/phases/000-normal-sphere-count-select-diagnostic/board-evidence-summary.md` | `doc/phases/000-normal-sphere-count-select-diagnostic/board-evidence-manifest.json` | `doc/phases/000-normal-sphere-count-select-diagnostic/board-evidence-doctor.md` | `normal-sphere-phase000-board-smoke` | count/select 初筛正向。 |
| 010 | `doc/phases/010-vcompress-select-ablation/board-evidence-summary.md` | `doc/phases/010-vcompress-select-ablation/board-evidence-manifest.json` | `doc/phases/010-vcompress-select-ablation/board-evidence-doctor.md` | `normal-sphere-phase010-vcompress-board-smoke` | `vcompress` 写回族正向。 |
| 020 | `doc/phases/020-getdistances-dense-store-audit/board-evidence-summary.md` | `doc/phases/020-getdistances-dense-store-audit/board-evidence-manifest.json` | `doc/phases/020-getdistances-dense-store-audit/board-evidence-doctor.md` | `normal-sphere-phase020-getdistances-board-smoke` | getDistances dense store 正向。 |
| 030 | `doc/phases/030-rgb-rgba-point-type-expansion/board-evidence-summary.md` | `doc/phases/030-rgb-rgba-point-type-expansion/board-evidence-manifest.json` | `doc/phases/030-rgb-rgba-point-type-expansion/board-evidence-doctor.md` | `normal-sphere-phase030-rgb-rgba-smoke` | RGB/RGBA source layout 正向。 |
| 060 | `doc/phases/060-production-integration-execution/board-evidence-summary.md` | `doc/phases/060-production-integration-execution/board-evidence-manifest.json` | `doc/phases/060-production-integration-execution/board-evidence-doctor.md` | `normal-sphere-phase060-production-public` | 接入后四种 source 点型 × 三入口 production-public 5-run 全部正向。 |

Phase 000-030 都是 single board smoke（单次板卡小型验证），只支撑 historical diagnostic 初筛。当前 production
性能结论只引用 Phase060 5-run repeated board summary；该阶段 Evidence Doctor 为 0/0/0。

## 复现命令

```bash
make -C test-rvv/sample_consensus/sac_model_normal_sphere board_smoke OUTPUT_DIR_BOARD=log/board-phase030-pointxyzrgb REMOTE_BOARD_OUTPUT_DIR=<REMOTE_BOARD_OUTPUT_DIR>/log/board-phase030-pointxyzrgb BENCH_ARGS='65536 200 PointXYZRGB'
make -C test-rvv/sample_consensus/sac_model_normal_sphere board_smoke OUTPUT_DIR_BOARD=log/board-phase030-pointxyzrgba REMOTE_BOARD_OUTPUT_DIR=<REMOTE_BOARD_OUTPUT_DIR>/log/board-phase030-pointxyzrgba BENCH_ARGS='65536 200 PointXYZRGBA'
make -C test-rvv/sample_consensus/sac_model_normal_sphere record_phase030_evidence_state
make -C test-rvv/sample_consensus/sac_model_normal_sphere collect_phase060_repeated_board PHASE060_REPEATED_RUNS=5 BENCH_ARGS='65536 200 PointXYZ'
make -C test-rvv/sample_consensus/sac_model_normal_sphere collect_phase060_repeated_board PHASE060_REPEATED_RUNS=5 BENCH_ARGS='65536 200 PointXYZI'
make -C test-rvv/sample_consensus/sac_model_normal_sphere collect_phase060_repeated_board PHASE060_REPEATED_RUNS=5 BENCH_ARGS='65536 200 PointXYZRGB'
make -C test-rvv/sample_consensus/sac_model_normal_sphere collect_phase060_repeated_board PHASE060_REPEATED_RUNS=5 BENCH_ARGS='65536 200 PointXYZRGBA'
make -C test-rvv/sample_consensus/sac_model_normal_sphere run_phase060_evidence_doctor
make -C test-rvv/sample_consensus/sac_model_normal_sphere record_phase060_evidence_state
make -C test-rvv/sample_consensus/sac_model_normal_sphere evidence_status
```

## ASM Attribution 口径

反汇编入口是：

```bash
make -C test-rvv/sample_consensus/sac_model_normal_sphere clean_bench_rvv dump_bench_rvv
```

当前反汇编归属到 RVV bench binary（性能测试二进制）中的 production helper 和历史测试专用 helper，可用于确认
`computeNormalSphereDistanceRVV`、`vfsqrt.v`、`vcpop.m`、`vcompress.vm`、`vfwcvt.f.f.v` 和 `vse64.v` 等关键 RVV 指令存在。
它证明路径和指令归属，不证明性能。

## 提交边界

可审查 / 可提交候选：`doc/phases/*/board-evidence-summary.md`、`board-evidence-manifest.json`、`board-evidence-doctor.md` / `.json`、`log/evidence_registry.json`。默认排除：`build/`、`log/board*/*.log`、`log/qemu/*.log`、本机 `config.mk` 和 raw board logs。
