# Phase 050 result: interpolation geometry staging diagnostic

## 阶段结论

本阶段完成 test-only interpolation geometry staging（测试专用插值几何暂存）诊断，没有修改 `features/include/pcl/features/impl/shot.hpp`。新增 helper 覆盖 indexed surface gather（按索引读取 surface 点）、中心点差值、local reference frame（局部参考系）三轴 dot、distance 和 valid lane mask（有效通道掩码），但不实现 histogram scatter（直方图离散写入）、`acos` / `atan2` 或完整 interpolation。

EvidenceDecision（证据决策）：`attempted / not recommended for production probe`。修正前首轮板卡为 0.73x；去掉 RVV helper 中重复 scalar sqrt（标量平方根）后，当前板卡为 0.97x，仍未达到正向阈值。该结果只说明“把 geometry 投影暂存成多组数组”这个 code shape（代码组织形态）当前不值得进入 production probe（生产探针），不能拒绝其它 interpolation 子候选。

## 计划动作回填

| action | status | command / evidence | result |
| --- | --- | --- | --- |
| F1 red test | done | `make -C test-rvv/features/shot run_test_compare` | 缺少 `computeInterpolationGeometryIndexedScalar/RVV` 时 Std 编译失败，红灯符合预期。 |
| F2 helper implementation | done | `include/impl/shot_interpolate.hpp`、`include/shot.h` | 新增 scalar reference 和 RVV candidate；第一次 green 后发现 RVV valid mask 重复 scalar sqrt，已修正为只用 `sqr_dists > 0` 生成 valid。 |
| F3 component bench | done | `src/bench_shot.cpp`、`script/generate_shot_evidence_manifest.py` | 新增 `interpolation_geometry_component` case 和 Evidence Doctor metadata。 |
| F4 asm | done | `make -C test-rvv/features/shot dump_bench_rvv` | asm 可见 `vluxei32.v`、`vfsub.vf`、`vfsqrt.v`、`vfmacc.vf`、`vse64.v` 和 `vse8.v`。 |
| F5 board evidence | done | targeted `run_board_bench_compare fetch_board_logs` | 首轮 0.73x 暴露实现重复工作；修正后 0.97x，按计划为 neutral / negative bucket，不追加 positive rerun。 |
| F6 docs | done | 本 result、phase README、matrix、roadmap、evaluation、README 和队列表 | 当前结论、证据边界和后续路线已同步。 |

## Correctness（正确性）

TDD red / green 已闭合。`run_test_compare` 在新增 helper 后通过 Std / RVV 两侧各 9 个 gtest。新增测试 `ShotInterpolationGeometryComponent.RvvMatchesScalarReferenceForIndexedSurfaceCloud` 覆盖：

- indexed surface point order；
- zero-distance lane（距离为 0 的通道）；
- NaN `binDistance` lane；
- 三轴 projection（投影）和 distance output；
- valid mask 与标量参考一致。

该测试只证明 geometry staging helper 的 same-chain（同构链路）正确性，不证明完整 `interpolateSingleChannel` 语义。

## 反汇编归属

`dump_bench_rvv` 通过。`build/asm/riscv/bench_shot_rvv.asm` 中可见：

| 指令 | 证据含义 |
| --- | --- |
| `vluxei32.v` | indexed AoS `PointXYZ` 字段 gather。 |
| `vfsub.vf` | 批量减中心点坐标。 |
| `vfsqrt.v` | 批量 distance staging。 |
| `vfmacc.vf` | 三轴 dot product（点积）使用 FMA 形态。 |
| `vse64.v` / `vse8.v` | 写回 double staging arrays 和 valid mask。 |

该归属限定在 test-only helper / bench callsite；production interpolation 未命中 RVV。

## Board evidence（板卡证据）

| run label | helper shape | Std avg | RVV avg | speedup | doctor |
| --- | --- | ---: | ---: | ---: | --- |
| `phase050_interpolation_geometry_initial_negative` | RVV helper 内重复 scalar sqrt 生成 valid mask | 6.3139 ms | 8.6899 ms | 0.73x | historical run |
| `phase050_interpolation_geometry_after_valid_fix` | valid mask 改为 `sqr_dists > 0`，distance 只由 RVV `vfsqrt` 生成 | 7.2672 ms | 7.4894 ms | 0.97x | Errors=1, Warnings=1, Suggestions=0 |

当前 doctor Error 是 `ba_degradation_frequency`：1/1 comparison 低于 1。处理动作是把该 candidate family 标为 attempted / neutral-negative，不写 production-ready，也不把它用于拒绝其它 interpolation 形态。Warning 是 `low_run_count`；因为当前桶已经不正向且不接 production，本阶段不扩大到 5-run。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | component diagnostic（组件诊断）。 |
| A/B boundary | test helper / interpolation geometry staging component bench。 |
| 当前决策问题 | implementation-shape 和 RVV-vs-scalar component A/B。 |
| diagnostic 是否可外推到 production | 不能直接外推。它没有覆盖 production loop 里的 histogram update、`acos` / `atan2`、descriptor 写入冲突和对象状态。 |
| comparison-boundary / baseline mismatch 风险 | 存在。production 标量路径在同一循环里消费 geometry 结果；本 candidate 额外写出多组 staging arrays，可能引入 production 不会接受的内存流量。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不允许把这个 staging arrays 形态作为 production probe 输入；但仍允许后续另做 bin-selection、direct scalar-tail 或 color 子候选诊断。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。当前证据明显不足以 clean-adopt。 |

## 矩阵更新

`interpolation geometry staging RVV` 从 `planned` 更新为 `attempted / neutral-negative`：

- validated scope（已验证范围）：`pcl::PointXYZ` AoS + `pcl::Indices`、三轴 frame projection、double distance / projection staging、valid mask。
- unvalidated scope（未验证范围）：完整 interpolation、histogram scatter、`acos` / `atan2`、SHOT1344 double-channel、production direct。
- rejected shape（拒绝形态）：多组 double staging arrays 作为独立 production probe 输入。它增加四组 double store 和一组 valid store，在板卡上没有形成正向。

## 继续 / 停止判断

`stop_condition_hit`：本 candidate 不建议进入 production probe；继续做 PI2 会扩大到未授权 production 且证据不支持。

`next_phase_default`：若仍不授权 production，继续 test-only `color-lab-distance-component-diagnostic` 或更窄的 interpolation bin-selection diagnostic。当前更推荐 color LAB distance，因为它属于 SHOT1344 独立 color path（颜色路径），不会复用本阶段已证伪的 staging arrays 形态。
