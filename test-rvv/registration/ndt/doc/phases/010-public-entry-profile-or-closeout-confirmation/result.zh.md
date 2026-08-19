# Phase 010 Result: Public Entry Profile Or Closeout Confirmation

## 执行摘要

本阶段补了真实 `NormalDistributionsTransform::align` public entry（公开入口）bench，并完成 QEMU smoke、board repeated profile、gprof（函数级剖析）probe 和 evidence registry 刷新。结果表明：当前 NDT public entry 仍没有给现有 RVV build 带来稳定收益，但内部热点主要集中在 `computeTransformation` / `computeStepLengthMT` / `computeDerivatives` / `updateDerivatives` / `updateHessian`，不是邻域搜索或单纯 SVD。

## 变化范围

| 类别 | 路径 |
| --- | --- |
| topic bench / profile | `src/bench_ndt.cpp` |
| topic Makefile | `Makefile` |
| topic scripts | `script/generate_ndt_board_repeated_summary.py`、`script/generate_ndt_qemu_evidence_manifest.py` |
| phase docs | `doc/phases/010-public-entry-profile-or-closeout-confirmation/plan.zh.md`、`result.zh.md`、`doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md` |
| roadmap / eval | `doc/optimization-roadmap.zh.md`、`doc/optimization-evidence.zh.md`、`doc/ndt-evaluation.zh.md` |
| production | 未修改 |

## 结果

| evidence | command | result |
| --- | --- | --- |
| QEMU public smoke | `make run_bench_public_smoke` | 1 case pass，checksum 可解析；日志 `log/qemu/run_bench_public_rvv.log` |
| correctness regression | `make run_test_compare` | Std/RVV 各 4 TEST pass |
| board public repeated | `make run_board_bench_ndt_public_repeated` | 3/3 runs collected；median speedup 1.003x；1/3 below 1；decision bucket=`neutral` |
| board gprof probe | `make collect_board_ndt_public_gprof_probe` | `log/board/public_entry_gprof_probe/gprof.txt` 生成；内部热点可见 |

## 解释和边界

public-entry bench 的端到端 speedup 只有约 1.00x，`Evidence Doctor` 给出 `ba_degradation_frequency`、`low_run_count` 和 `near_threshold_ba`。这说明当前 public entry 不是稳定正收益候选，不能直接进入 production。

public-entry checksum 在 Std/RVV 之间不同，表明它不能作为 strict bitwise A/B correctness（严格逐位正确性对拍）结论；本阶段把它视为跨 build 的 profile 辅助值，而不是生产接入门禁。

gprof 结果显示：

- `updateDerivatives` 自身占比最高，约 52.63%；
- `updateHessian` 约 27.63%；
- `computePointDerivatives` 约 5.26%；
- `computeHessian` / `computeStepLengthMT` 的累计路径很重；
- `flann::KdTreeFLANN::nearestKSearch`、`Eigen::SVD`、`computeAngleDerivatives` 在这组样本里不是主要自耗时点。

这与 phase 000 的数学 helper 审计一致：如果后续继续优化，优先方向应是 `updateDerivatives` / `updateHessian` 的逐样本数学核，尤其是 double `std::exp` 和减少 staging / reduction 成本，而不是先去碰 neighbor search 或 SVD。

## 生产接入判断

仍不建议把当前 RVV diagnostic 接入 `ndt.hpp`。但现在已有更强的继续方向：

1. `updateDerivatives` / `updateHessian` 是 public entry 的主热点。
2. 当前 `rvv_math.hpp` 只覆盖 float helper，double `std::exp` 仍是显著 gap。
3. 如果继续推进，下一 phase 应优先围绕 double exp helper 或 fused per-sample formula，先完成语义合同与证据链，再考虑 RVV 候选。

## 下一步

默认恢复动作改为：继续向 `rvv-math-vectorization` 路线探测 double `exp` helper 的可实现性，或者围绕 `updateDerivatives` 设计更少 staging 的 fused formula 候选。若用户选择停在此处，则当前 topic 仍应保持 no-production 结论。
