# Phase 040 result: shape-bin indexed gather diagnostic

## 阶段结论

本阶段完成 test-only indexed gather（测试专用按索引离散加载）组件诊断，没有修改 `features/include/pcl/features/impl/shot.hpp`。`computeShapeBinDistanceIndexedRVV` 在 `pcl::PointCloud<pcl::Normal>` + `pcl::Indices` 输入上与标量参考链路一致，反汇编可见 `vluxei32.v`、`vfwcvt.f.f.v`、`vfmacc.vf`、`vcpop.m` 和 `vse64.v`，三次 targeted board（板卡定向性能测试）结果为 1.65x-1.86x。

EvidenceDecision（证据决策）：`partial-production-candidate`。indexed gather 组件仍为正向，但它只证明 test helper 边界下的 RVV-vs-scalar component A/B（组件候选相对标量对照），不能替代 production direct（真实生产路径证据）。下一步默认进入 `PI1-shape-bin-indexed-production-probe-plan`，只写生产接入计划；PI2 production patch（生产补丁）需要用户明确授权。

## 计划动作回填

| action | status | command / evidence | result |
| --- | --- | --- | --- |
| E1 red test | done | `make -C test-rvv/features/shot run_test_compare` 在 helper 缺失时失败 | 先观察到 `computeShapeBinDistanceIndexedScalar/RVV` 缺符号编译失败，证明测试先行。 |
| E2 helper implementation | done | `include/impl/shot_shape_bin.hpp` | 新增 indexed scalar reference 和 RVV gather helper；返回 NaN normal count（非法法线计数）。 |
| E3 component bench | done | `src/bench_shot.cpp`、`script/generate_shot_evidence_manifest.py` | 新增 `shape_bin_indexed_component` case，checksum（校验和）把输出和 NaN count 纳入同一指纹。 |
| E4 asm | done | `make -C test-rvv/features/shot dump_bench_rvv` | `build/asm/riscv/bench_shot_rvv.asm` 可见 indexed gather、float-to-double widen（浮点扩宽）、FMA（融合乘加）、mask popcount（掩码计数）和 double store。 |
| E5 board evidence | done | `BENCH_ARGS='--case-filter shape_bin_indexed_component' make -C test-rvv/features/shot run_board_bench_compare fetch_board_logs`，共 3 次 | 三次 run 方向稳定，均为 positive bucket（正向桶）。 |
| E6 docs | done | 本 result、phase README、optimization matrix、roadmap、evaluation、README 和队列表 | 当前结论、证据边界和下一步已同步。 |

## Correctness（正确性）

`run_test_compare` 通过 Std / RVV 两侧各 8 个 gtest。新增测试 `ShotShapeBinIndexedComponent.RvvMatchesScalarReferenceForIndexedNormalCloud` 构造乱序和重复 index，验证输出 bin distance 与 NaN count 都和标量参考一致。测试中的 `reference_nan_count=2` 是刻意覆盖同一个 NaN normal 被重复索引两次的 production-like side effect（近似生产副作用）边界。

## 反汇编归属

`dump_bench_rvv` 通过。`bench_shot_rvv.asm` 中可见：

| 指令 | 证据含义 |
| --- | --- |
| `vluxei32.v` | indexed AoS normal 字段按 32-bit byte offset 离散加载。 |
| `vfwcvt.f.f.v` | normal float 字段扩宽到 double 参与 `createBinDistanceShape` 同构公式。 |
| `vfmacc.vf` | dot product（点积）使用 FMA 形态。 |
| `vcpop.m` | RVV finite mask（有限值掩码）统计 NaN normal count。 |
| `vse64.v` | double bin distance 输出。 |

该归属仍限定在 test-only bench helper；production `createBinDistanceShape` 未命中 RVV。

## Board evidence（板卡证据）

| run label | Std avg | RVV avg | speedup | doctor |
| --- | ---: | ---: | ---: | --- |
| `phase040_indexed_component_smoke` | 4.1584 ms | 2.2386 ms | 1.86x | Errors=0, Warnings=1, Suggestions=0 |
| `phase040_indexed_component_rerun1` | 4.1870 ms | 2.3580 ms | 1.78x | Errors=0, Warnings=1, Suggestions=0 |
| `phase040_indexed_component_rerun2` | 4.0572 ms | 2.4573 ms | 1.65x | Errors=0, Warnings=1, Suggestions=0 |

三次 run 使用同一 targeted case-filter。每个 run 的 Evidence Doctor Warning 都是 `low_run_count`，因为 manifest 一次只解析一个 comparison；阶段计划允许用 run-labelled rerun（带标签复跑目录）人工确认 decision bucket。桶稳定为 positive，但由于 run count 仍低于正式 production evidence 口径，本证据不能写成 production performance（生产性能）。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | component diagnostic（组件诊断）。 |
| A/B boundary | test helper / indexed gather component bench。 |
| 当前决策问题 | RVV-vs-scalar component A/B 和 implementation-shape（实现形态）判断。 |
| diagnostic 是否可外推到 production | 不能直接外推。它覆盖 indexed normal gather 和 NaN count，但不覆盖 estimator protected state（受保护对象状态）、`frames_` / `normals_` 生命周期、vector resize、真实 `PCL_WARN` 文本、public entry（公开入口）计时边界和 dispatch / fallback。 |
| comparison-boundary / baseline mismatch 风险 | 存在。production helper 还要从对象状态读取 `frames_` 和 `indices`，维护 warning side effect，并与后续 histogram 插值共享上下文。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段不是弱 / 负 / 中性 / 不稳定；若后续 production direct 变弱或转负，只能按 PI5 停在用户检查点，不能自动采纳或回滚。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。当前只有 Std/RVV component A/B，不能选择或 clean-adopt 新 production family。 |

## 矩阵更新

`shape-bin indexed gather RVV` 从 `planned` 更新为 `partial-production-candidate`：

- validated scope（已验证范围）：`pcl::Normal` AoS + `pcl::Indices`、固定 `frame_z`、`nr_shape_bins=10`、double output、NaN count。
- unvalidated scope（未验证范围）：production public entry、泛型 normal traits、`Scalar=double` fallback、indices 越界防护、真实 `PCL_WARN`、与 interpolation / histogram scatter 的总入口收益。
- point_type_expansion_queue（点类型扩展队列）：若进入 production，先选择 `pcl::Normal` exact gate（精确点型门控）或 traits-gated normal layout（基于字段特征的 normal 布局门控）；任一路径都需要 fallback 测试、production direct bench、asm 归属、board rerun 和 Evidence Doctor。

## 继续 / 停止判断

`stop_condition_hit`：继续到 PI2 production patch 会修改 `features/include/pcl/features/impl/shot.hpp`，当前没有用户对生产源码补丁的明确授权，因此不能进入 PI2。

`next_phase_default`：`PI1-shape-bin-indexed-production-probe-plan`。该阶段只写生产接入计划，冻结候选范围、fallback / dispatch、点类型 gate、warning side effect、production direct tests 和 PI2-PI5 暂停条件。
