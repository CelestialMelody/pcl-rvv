# 010 production-shaped-full-diagnostic 结果

## 实际执行范围

本阶段按 `plan.zh.md` 建立 full-pipeline production-shaped diagnostic（生产形态完整诊断）：新增 full helper、full correctness test、full bench label、label-filtered manifest wrapper，并在板卡上完成 5-run repeated summary 和 Evidence Doctor。生产源码未修改。

## 计划动作回填

| action | status | 命令 / 证据路径 | 结论 |
| --- | --- | --- | --- |
| 写 RED 测试 | done | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter run_test_rvv` | 首次编译失败，缺少 `progressiveFilterStd`、`ProgressiveFilterResult`、`progressiveFilterRVV` |
| 实现 Std full helper | done | `include/impl/apmf_components.hpp` | `progressiveFilterStd` 复刻 z-min、multi-iteration open、`thresholdGroundStd` 和 `A.swap(Zf)` |
| 实现 RVV full helper | done | `include/impl/apmf_components.hpp` | `progressiveFilterRVV` 复用 RVV component helper；ground 过小时按当前 filtered grid 走 scalar tail |
| 扩展 bench | done | `src/bench_apmf.cpp` | 新增 `apmf full pipeline diagnostic` label 和组合 checksum |
| 扩展 manifest wrapper | done | `script/generate_apmf_board_evidence_manifest.py`、`Makefile` | 支持 `APMF_EVIDENCE_INCLUDE_REGEX='full pipeline'` 和 binary hash |
| 板卡复跑 | done | `log/board/full-pipeline-repeated/run1` 到 `run5` | full-pipeline 5-run median 1.31x，decision bucket 为 positive |
| 更新 result / roadmap / matrix | done | 本文件、`doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md` | 当前默认下一阶段为 PI1 production integration plan |

## 正确性和路径证据

| 证据层 | 命令 / 路径 | 结果 | 边界 |
| --- | --- | --- | --- |
| RED gate | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter run_test_rvv` | 按预期因 full helper 缺失失败 | 证明新增测试能抓到“只做组件、缺少完整链路”的破坏 |
| QEMU correctness | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter run_test_compare` | Std / RVV 均通过 5 个测试 | 只证明 test-only helper；不证明 production dispatch |
| QEMU bench smoke | `ALLOW_QEMU_BENCH_COMPARE=1 BENCH_ARGS='--size 4096 --grid-rows 24 --grid-cols 24 --half 2 --iterations 2 --warmup 1' make -C test-rvv/segmentation/approximate_progressive_morphological_filter run_bench_compare` | full-pipeline checksum 两侧一致，label 可解析 | QEMU timing 不作为性能结论 |
| asm attribution | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter dump_bench_rvv` | 诊断二进制包含 floor conversion、load/store、indexed gather、compress 和 popcount 指令 | 尚非 production hot symbol |

## 板卡性能证据

板卡预算为 5 run，参数为 `--size 262144 --grid-rows 160 --grid-cols 160 --half 4 --iterations 8 --warmup 2`。summary-only 证据路径：

- `log/board/full-pipeline-repeated/summary.md`
- `log/board/full-pipeline-repeated/evidence_manifest.json`
- `log/board/full-pipeline-repeated/evidence_doctor.md`

| case | run count | median speedup | min | max | values | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| `apmf full pipeline diagnostic` | 5 | 1.31x | 1.30x | 1.31x | 1.31x, 1.30x, 1.31x, 1.31x, 1.30x | positive |

同批 raw analyze logs 仍显示 `apmf window open component` 为 0.94x 左右，说明 full-pipeline positive 不能被解释成 window-open component 独立正向。

## Evidence Doctor 结果

`make -C test-rvv/segmentation/approximate_progressive_morphological_filter OUTPUT_DIR_BOARD=log/board/full-pipeline-repeated APMF_REPEATED_DIR=log/board/full-pipeline-repeated APMF_REPEATED_SUMMARY=log/board/full-pipeline-repeated/summary.md APMF_EVIDENCE_MANIFEST=log/board/full-pipeline-repeated/evidence_manifest.json APMF_EVIDENCE_DOCTOR_MD=log/board/full-pipeline-repeated/evidence_doctor.md APMF_EVIDENCE_INCLUDE_REGEX='full pipeline' run_board_evidence_doctor`

结果：`Errors=0，Warnings=0，Suggestions=0`。manifest 记录 binary hash：`dc6889a6ede3384d5de61a3c2d24325317bd6bf318e70a535cefa556b7241d54`。

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `production-shaped diagnostic`，full helper 仍只存在于 test-rvv |
| A/B boundary | `production-shaped helper`；Std/RVV 两侧为 `bench_apmf_std` / `bench_apmf_rvv` |
| 当前决策问题 | 是否值得进入 PI1 production integration plan |
| diagnostic 是否可外推到 production | 只能外推为 `partial-production-candidate`。真实 production 仍需 public entry、fallback、泛型点类型、OpenMP 和 production direct evidence |
| comparison-boundary / baseline mismatch 风险 | 有。当前 helper 避开真实类对象生命周期、`initCompute`、`copyPointCloud` 成本细节和 OpenMP 外层 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前 full result 为 positive；若后续 production direct 弱化，必须降级为 bounded probe 或 no-production |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。当前结论不 clean-adopt |

## 优化矩阵更新

| candidate family | decision | 理由 | unblocked next action |
| --- | --- | --- | --- |
| `full-pipeline-rvv-components` | partial-production-candidate | QEMU correctness 通过，full 5-run board median 1.31x，Evidence Doctor 无 finding | 进入 PI1，冻结 production 范围和 fallback，不直接修改 production |
| `hybrid-full-diagnostic` | deferred | full-pipeline 已 positive，暂不需要同轮 hybrid A/B | 若 production direct 因 window-open 退化或维护成本过高，可恢复 |
| `window-open-row-reduction` | attempted-negative in phase-010 batch | 同批 component row 约 0.94x，不能作为独立正向优化 | production 计划不把 window-open 写成独立收益；必要时保留标量 window-open |

## Continue / Stop Decision

current_decision：`partial-production-candidate / PI1-plan-ready`。

stop_condition_hit：`production_integration_requires_user_authorization`。当前证据足以写 PI1 计划，但不授权 worker 自动修改 production 源码或类声明。

next_phase_default：`020-production-integration-plan`。本轮已创建该计划；继续到 PI2 production patch 需要用户确认生产接入范围。
