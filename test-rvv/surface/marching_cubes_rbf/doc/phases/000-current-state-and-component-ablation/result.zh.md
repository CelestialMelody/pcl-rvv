# Phase 000 Result: current state and component ablation

## 当前结论

本阶段完成 `marching_cubes_rbf` 的首轮 component ablation（组件消融）诊断。结论是
`partial-production-candidate`：建议让用户确认是否进入 bounded production probe（有界生产探针），
但当前不建议直接把 RVV 实现视为 adopted production behavior（已采用生产行为）。

## 实际产物

| 类型 | 路径 | 状态 |
| --- | --- | --- |
| test support 聚合入口 | `test-rvv/surface/marching_cubes_rbf/include/marching_cubes_rbf.h` | created |
| test support 内部 helper | `test-rvv/surface/marching_cubes_rbf/include/impl/marching_cubes_rbf_core.hpp` | created |
| correctness test | `test-rvv/surface/marching_cubes_rbf/src/test_marching_cubes_rbf.cpp` | created |
| component bench | `test-rvv/surface/marching_cubes_rbf/src/bench_marching_cubes_rbf.cpp` | created |
| board / QEMU harness | `test-rvv/surface/marching_cubes_rbf/Makefile`, `board.mk` | created |
| manifest wrapper | `test-rvv/surface/marching_cubes_rbf/script/generate_marching_cubes_rbf_evidence_manifest.py` | created |
| evaluation / roadmap / matrix | `test-rvv/surface/marching_cubes_rbf/doc/` | created / updated |

Production 源码未修改；`doc-rvv/surface/marching_cubes_rbf-RVV.zh.md` 仍为 not_applicable。

## 命令和证据

| 证据 | 命令 | 结果 / 路径 |
| --- | --- | --- |
| QEMU correctness（QEMU 正确性） | `make -C test-rvv/surface/marching_cubes_rbf run_test_compare` | Std/RVV 各 3 个 gtest 通过；`log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| QEMU RVV smoke（日志形状） | `make -C test-rvv/surface/marching_cubes_rbf run_bench_rvv BENCH_ARGS="--case-filter mcrbf_full_pipeline_n24_r18 --iterations 1 --warmup-iterations 0"` | RVV bench 可运行；不作为性能结论 |
| asm attribution（反汇编归属） | `make -C test-rvv/surface/marching_cubes_rbf dump_bench_rvv` | `build/asm/riscv/bench_marching_cubes_rbf_rvv.full.asm`，可见 `vle64`、`vse64`、`vfmacc.vv`、`vfsqrt.v`、`vfredusum.vs` |
| board smoke（板卡性能初筛） | `make -C test-rvv/surface/marching_cubes_rbf run_board_component_smoke fetch_board_logs` | `log/board/analyze_bench_compare.log` |
| Evidence Doctor（证据体检） | `make -C test-rvv/surface/marching_cubes_rbf run_board_evidence_doctor` | `log/board/evidence_doctor.md`：Errors=0，Warnings=5 |

## 板卡结果

| case | Std ms/iter | RVV ms/iter | speedup | 证据边界 |
| --- | ---: | ---: | ---: | --- |
| `mcrbf_matrix_fill_n24` | 0.0693 | 0.0297 | 2.33x | matrix-only diagnostic |
| `mcrbf_matrix_fill_n40` | 0.1970 | 0.0864 | 2.28x | matrix-only diagnostic |
| `mcrbf_full_pipeline_n24_r18` | 5.4621 | 4.2655 | 1.28x | production-shaped diagnostic |
| `mcrbf_full_pipeline_n40_r20` | 13.0470 | 10.3703 | 1.26x | production-shaped diagnostic |
| `mcrbf_full_pipeline_n56_r18` | 16.8135 | 13.8875 | 1.21x | production-shaped diagnostic |

Component timing 显示 solve（求解器）没有吞掉全部收益：例如 `n40_r20` 中 solve 约 `1.70 ms`，
voxel evaluation 从约 `10.88 ms` 降到 `8.40 ms`，matrix fill 从约 `0.19 ms` 降到约 `0.08 ms`。

## Evidence Doctor 解释

`log/board/evidence_doctor.md` 报告 `Errors=0`。5 个 Warning 均为 `low_run_count`：当前 manifest
只有一次 board compare summary，不能判断 run-to-run 异常频率、温度 / governor / freq 风险或 long tail。
因此本阶段不能写成 production performance（生产性能）已闭合，只能写成支持进入 bounded production probe。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | component_ablation + production-shaped diagnostic |
| A/B boundary | test helper / bench wrapper，不是真实 production public overload |
| 当前决策问题 | 是否建议进入 production integration loop |
| diagnostic 是否可外推到 production | 部分可外推：公式、solve 边界和 full-pipeline 结构接近；但 PointNT AoS load、fallback 和 production dispatch 未证明 |
| comparison-boundary / baseline mismatch 风险 | 有。test helper 预先物化 SoA centers，并用 column-major matrix write；production 当前从 `PointNT` AoS 读取 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前结果为 positive diagnostic；若后续 repeated board 降为 neutral/negative，应暂停或回到 diagnostic |
| clean adoption 是否需要同一 production boundary 内证据 | 需要。必须补 production direct correctness、fallback、asm、repeated board 和 Evidence Doctor |

## 继续 / 停止决策

默认下一步：等待用户确认是否建议将 RVV 优化实现接入 production 源码。

建议确认的方向是“进入 PI1 production integration plan”，不是直接采纳。PI1 应冻结：

- production patch 只触碰 `surface/include/pcl/surface/impl/marching_cubes_rbf.hpp` 的 `voxelizeData()` 内部 helper / 分流；
- 先以 `PointNormal` 和当前 double RBF kernel 为窄范围，不外推泛型点类型；
- 非 RVV 构建、无法满足 layout / traits / 数值 gate 的路径保持标量；
- PI2-PI5 需要补 production direct correctness、fallback、asm、repeated board summary 和完整 Evidence Doctor。

若用户不确认，当前 topic 可停在 diagnostic closeout，不修改 production 源码。
