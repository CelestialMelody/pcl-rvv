# Phase 030 Result: direct AoS pair-feature diagnostic

## 当前结论

本阶段完成 `pfh-direct-point-load-rvv` test-only diagnostic（测试专用诊断）闭环。Direct AoS（直接结构数组离散加载）候选在同一板卡 repeated（重复板卡测试）中稳定强于 Phase 010 的 staged SoA（先暂存为分字段数组）候选：

| case | evidence role | 5-run B/A | mean / median | decision |
| --- | --- | --- | --- | --- |
| `candidate_pfh_direct_aos_rvv` | diagnostic | `2.86, 2.86, 2.90, 2.87, 2.87` | `2.872x / 2.87x` | `positive_diagnostic_candidate`，生产探针首选 family |
| `candidate_pfh_pair_batch_rvv` | diagnostic | `2.34, 2.34, 2.35, 2.34, 2.33` | `2.34x / 2.34x` | 保留为 historical diagnostic 和 fallback comparison |
| `component_pfh_signature` | production-shaped diagnostic | `1.00, 1.00, 1.01, 1.00, 1.01` | `1.004x / 1.00x` | 未接 production 前只是 baseline |
| `public_pfh_k` | production-public baseline | `1.00, 1.00, 1.01, 1.00, 1.01` | `1.004x / 1.00x` | 未接 production 前只是 dilution check |

## 执行动作回填

| action | status | evidence |
| --- | --- | --- |
| RED-030 / GREEN-030 | done | `make -B -C test-rvv/features/pfh run_test_rvv`：3/3 pass；新增 direct AoS candidate same-chain 对拍。 |
| correctness compare | done | `make -B -C test-rvv/features/pfh run_test_compare`：Std/RVV 均 3/3 pass。 |
| asm attribution | done | `make -B -C test-rvv/features/pfh dump_bench_rvv`；full asm 中可见 direct helper，候选区域包含 `vluxei32.v` gather 指令。 |
| board smoke | done | `make -C test-rvv/features/pfh board_smoke BENCH_ARGS='--side 32 --k 32 --iterations 3 --warmup 1'`；direct AoS 约 `2.88x`，checksum 匹配。 |
| board repeated | done | `make -C test-rvv/features/pfh board_repeated BENCH_ARGS='--side 32 --k 32 --iterations 8 --warmup 2'`；5-run 数值见上表。 |
| Evidence Doctor | done | `make -C test-rvv/features/pfh evidence_doctor_repeated`；`Errors=0, Warnings=0, Suggestions=10`。 |

## Evidence Doctor 解释

Doctor 输入为 `test-rvv/features/pfh/log/board/repeated/evidence_manifest.json`，报告为 `test-rvv/features/pfh/log/board/repeated/evidence_doctor.md`。

- Error：无。当前 repeated 数据可用于 diagnostic family selection（诊断实现族排序）。
- Warning：无。没有 checksum、边界或方向异常。
- Suggestions：10 项，主要是 taskset/governor/freq/temperature 和 binary hash 缺失；`component_pfh_signature` 与 `public_pfh_k` 另有 near-threshold 提示。处理方式：不阻塞 direct-vs-staged 诊断排序；进入 production probe 后必须用生产接入后的 board repeated 与 Doctor 数据作为采纳判断来源。

## Diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | direct/staged 两项均为 diagnostic；`component_pfh_signature` 是 production-shaped diagnostic；`public_pfh_k` 是未接 RVV production 前的 baseline。 |
| A/B boundary | direct/staged 是 test helper；不是 production detail helper。 |
| 当前决策问题 | 当前只回答 RVV-family-selection 前的候选排序：direct AoS 是否比 staged SoA 更值得接入生产探针。 |
| diagnostic 是否可外推到 production | 只能外推为 PI2 候选优先级。采纳必须看接入 `features/include/pcl/features/impl/pfh.hpp` 后的 production-detail / production-public 证据。 |
| comparison-boundary / baseline mismatch 风险 | 有。当前 direct candidate 使用 `PointNormal` exact layout，未覆盖常见 `PointXYZ + Normal` 预编译组合、cache path、泛型 PointNormal-like traits 或 `Scalar=double`。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本轮 direct 是稳定正向，因此允许进入有界 production probe。若生产证据中性或负向，PI5 必须停下等待用户决定保留或回滚。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前 production 没有已有 RVV family；direct AoS 相对 staged 的 test-only 优势足以决定首个 production probe family。最终是否保留看 production RVV-vs-scalar。 |

## 继续 / 停止决策

`continue_stop_decision`: continue。用户已授权接入后用板卡收益判断是否值得采纳，且 Phase 030 暴露的 direct AoS family 明显优于 staged family。

`next_phase_default`: `040-direct-aos-production-probe`。Phase 040 应只接入 direct AoS exact `PointNormal -> PointNormal` 窄范围生产探针，跑 production direct correctness、fallback、asm 和 board repeated。PI5 后无论正负，都保留当前 production diff 并等待用户确认采纳或回滚。
