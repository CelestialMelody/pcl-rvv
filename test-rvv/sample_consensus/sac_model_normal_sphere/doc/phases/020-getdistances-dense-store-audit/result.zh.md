# Phase 020: getDistances dense-store 审计结果

## 阶段范围

| 项 | 内容 |
| --- | --- |
| validated_scope | `PointXYZ + Normal`、`PointXYZI + Normal`，direct indexed source/normal，测试专用 `getDistancesToModelCandidate`，float 输入字段和 `std::vector<double>` dense output（稠密输出）。 |
| unvalidated_scope | production dispatch（生产分流）、其它点型 / normal layout、`Scalar=double`、非 indexed 入口、真实 public overload（公开入口）的 RVV 命中和 production fallback（生产回退）。 |
| phase_closeout_boundary | 本阶段只关闭 `getDistancesToModelCandidate` 的 dense-store diagnostic（稠密写回诊断）审计，不关闭 production adoption（生产采纳）。 |

## 实现结果

本阶段没有新增候选算法；复用已存在的 `getDistancesToModelCandidateRVV`。该 helper 在 RVV 构建下按 `indices_` gather source xyz 与 normal xyz，复用 `distancesRVV` 计算 float distance，再用 `vfwcvt.f.f.v` 转成 double 并通过 `vse64.v` 写入 `std::vector<double>`。非 RVV 构建或 layout gate（布局验收条件）不满足时回退到 `getDistancesToModelScalarReference`。

本阶段新增的是 Phase 020 专用证据闭环：`Makefile` 增加 Phase 020 manifest / Evidence Doctor / registry target，`script/generate_normal_sphere_evidence_manifest.py` 增加 `--phase 020`，只把 `diagnostic candidate getDistancesToModel` 放入候选比较，避免 Phase 000/010 的 select/count 数值混入本阶段决策。

production 文件 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp` 未修改。

## 执行结果

| 证据类型 | 命令 / 路径 | 结果 | 边界 |
| --- | --- | --- | --- |
| Python 语法 | `python3 -m py_compile test-rvv/sample_consensus/sac_model_normal_sphere/script/generate_normal_sphere_evidence_manifest.py` | 通过。 | 只证明生成器语法可解析。 |
| QEMU correctness（QEMU 正确性） | `make -C test-rvv/sample_consensus/sac_model_normal_sphere run_test_compare` | Std/RVV 各 4 个 gtest 通过。既有 `expectSameNormalSphereOutputs` 覆盖 candidate dense output 与标量参考对拍。 | QEMU 不证明真实性能。 |
| 反汇编归属 | `make -C test-rvv/sample_consensus/sac_model_normal_sphere clean_bench_rvv dump_bench_rvv`；`rg getDistancesToModelCandidateRVV\|vfwcvt\|vse64 ...full.asm` | `PointXYZ` 和 `PointXYZI` 两个 `getDistancesToModelCandidateRVV` 符号范围内均可见 `vfwcvt.f.f.v` 与 `vse64.v`。 | 归属到测试专用 bench binary，不是 production helper。 |
| Board PointXYZ | `make -C test-rvv/sample_consensus/sac_model_normal_sphere board_smoke OUTPUT_DIR_BOARD=log/board-phase020-pointxyz REMOTE_BOARD_OUTPUT_DIR=<REMOTE_BOARD_OUTPUT_DIR>/log/board-phase020-pointxyz BENCH_ARGS='65536 200 PointXYZ'` | 板卡 gtest 4/4 通过；checksum `32012`；candidate Std/RVV 为 `14.2753 ms` / `2.7685 ms`，speedup `5.156x`。 | 单次 board smoke（板卡小型验证），不支撑 repeated board 稳定性。 |
| Board PointXYZI | `make -C test-rvv/sample_consensus/sac_model_normal_sphere board_smoke OUTPUT_DIR_BOARD=log/board-phase020-pointxyzi REMOTE_BOARD_OUTPUT_DIR=<REMOTE_BOARD_OUTPUT_DIR>/log/board-phase020-pointxyzi BENCH_ARGS='65536 200 PointXYZI'` | 板卡 gtest 4/4 通过；checksum `32012`；candidate Std/RVV 为 `14.1455 ms` / `3.1257 ms`，speedup `4.526x`。 | 代表 `PointXYZI` source layout，不代表所有点型。 |
| Evidence Doctor（证据体检） | `doc/phases/020-getdistances-dense-store-audit/board-evidence-doctor.md` | `Errors=0`、`Warnings=2`、`Suggestions=0`；Warning 全部是 `low_run_count`。 | 允许 positive diagnostic，但不能写成 production performance（生产性能）。 |
| Evidence registry（证据登记表） | `make -C test-rvv/sample_consensus/sac_model_normal_sphere record_phase020_evidence_state && make -C ... evidence_status` | registry check 为 `fresh`。 | Phase 000 / 010 / 020 summary、manifest、doctor 均已登记。 |

## 板卡摘要

| case | point type | baseline ms | candidate ms | B/A speedup | bucket | evidence role |
| --- | --- | ---: | ---: | ---: | --- | --- |
| `diagnostic candidate getDistancesToModel` | `PointXYZ + Normal` | 14.2753 | 2.7685 | 5.156x | positive | production-shaped diagnostic |
| `diagnostic candidate getDistancesToModel` | `PointXYZI + Normal` | 14.1455 | 3.1257 | 4.526x | positive | production-shaped diagnostic |

同一日志中的 `public getDistancesToModel` Std/RVV speedup 约为 `1.220x` / `1.225x`，但公开入口当前没有 production RVV dispatch。该 public 行只能作为 mixed-boundary cross-check（混合边界交叉检查）上下文，不进入本阶段候选 B/A 决策。

## Evidence Doctor 解释

当前 Evidence Doctor 没有 Error，因此 Phase 020 可用于 dense-store 候选筛选。2 个 Warning 都是 `low_run_count`：本阶段只跑单次 board smoke，没有异常频率、温度 / governor / freq 或 repeated-board 分布。由于两个点型的 speedup 都远高于 `1.20x` positive 阈值，本阶段不触发自动复跑；生产接入前仍必须在 PI4 production evidence rerun（生产证据重跑）中重新跑真实 production boundary。

## Diagnostic 到 Production 错配审计

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic。 |
| A/B boundary | board Std build fallback vs board RVV build test-only `getDistancesToModelCandidate`。 |
| 当前决策问题 | RVV-vs-scalar diagnostic：dense double store 候选是否从 deferred 改成 PI1-adjacent candidate。 |
| 是否可外推到 production | 不能直接外推。production 源码没有 RVV dispatch、fallback gate、生产直连测试或 production asm。 |
| baseline mismatch 风险 | 存在。candidate 经测试专用派生类计时；public overload 的 Std/RVV 行只是混合边界上下文。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段结果为 positive；若后续 repeated board 降为弱 / 中性，也只能降级 diagnostic，不得直接推出 no-production。 |
| clean adoption 是否需要 production boundary 内证据 | 需要。必须完成 PI1-PI5，并在 PI5 后等待用户明确确认采纳或回滚。 |

## 阶段结论

Phase 020 决策为 `getDistances-dense-store-positive-diagnostic / PI1-adjacent candidate`。这修正了早期“getDistances 只作为附带证据”的状态：在当前测试专用 direct indexed source/normal 边界内，full-RVV `vfsqrt + normal angle + vfwcvt + vse64` dense output 是正向候选。

本结论不改变 production 状态。`countWithinDistance` 的 Phase 000 mask count、`selectWithinDistance` 的 Phase 010 `vcompress` 写回和本阶段 `getDistancesToModel` dense store 都可以进入后续 PI1 候选范围讨论；实际修改 production 源码仍需要用户明确授权进入 production integration loop（生产接入闭环）。

## 继续 / 停止判断

当前 phase 已闭合，registry 为 fresh。未命中板卡不可用、Evidence Doctor Error、证据矛盾或 dirty isolation 不安全。未授权 production 修改前，下一步仍不能改 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp`。若继续停留在 test-rvv 边界，可做点型扩展 phase；若用户授权 production integration loop，默认下一阶段是 PI1，冻结 count/select/getDistances 三入口的生产候选范围、fallback、点型 / layout gate 和 PI2-PI5 验证计划。
