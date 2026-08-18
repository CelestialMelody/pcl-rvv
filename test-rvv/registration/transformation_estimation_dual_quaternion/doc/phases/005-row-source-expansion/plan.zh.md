# Phase 005 Plan：row-source-expansion

## 阶段意图和边界

本阶段把 TEDQ 的 ordered-cloud-pair C1/C2 RVV accumulation candidate（顺序全云 C1/C2 RVV 累加候选）
扩展到 row source policy（行来源策略）诊断层：source-indexed-cloud-pair（源索引点云对）、
dual-indexed-cloud-pair（双索引点云对）和 correspondence-pair（对应关系点对）。范围只限
`test-rvv/registration/transformation_estimation_dual_quaternion` 的 test support、bench、summary
和 topic-local 文档；不修改 production 源码，不重开 production integration loop（生产接入闭环）。

默认实现形态是 staging-to-ordered diagnostic（先暂存成紧凑顺序点云再复用 ordered candidate 的诊断方式）。
它把 index / correspondence 展开成本计入 bench 计时边界，用来回答“如果把非顺序 row source 先整理成连续
ordered clouds，现有 C1/C2 RVV 前端是否还能抵消 staging 成本”。本阶段不直接实现 RVV gather（离散加载）。

## 当前状态清单

| 项目 | 当前状态 | 路径 |
| --- | --- | --- |
| production 源码 | TEDQ 无 diff，保持标量 | `registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp` |
| ordered-cloud-pair helper | board repeated diagnostic positive | `log/board/rvv_accum_full_cloud_repeated/summary.md` |
| C1/C2 component | strict accumulation-only positive | `log/board/component_ablation_repeated/summary.md` |
| production public direct | Phase 002 临时 patch neutral，patch 已撤回 | `log/board/production_public_full_cloud_repeated/summary.md` |
| row source public boundary | identity source-indexed、dual-indexed、correspondence gtest pass | `src/test_tedq.cpp` |
| evidence registry | fresh | `make evidence_status` |

## 假设与候选动作

| hypothesis | 要验证的现象 | 证据动作 | 可能结论 |
| --- | --- | --- | --- |
| H1 source-indexed staging 可接受 | source 侧离散索引展开成紧凑 cloud 后，C1/C2 RVV 前端仍有收益 | source-indexed staged candidate correctness + bench filter + board repeated | 若 positive，可作为后续 production gather / staging PI1 输入；若 neutral/negative，保持 no-production。 |
| H2 双侧 staging 成本过高 | dual-indexed 和 correspondence 需要同时暂存 source/target，可能吞掉 ordered 前端收益 | dual-indexed / correspondence staged candidate correctness + bench context | 若退化，只保留 rejected diagnostic，不做 production。 |
| H3 row source 语义不能只靠 identity | identity public boundary 不能证明非顺序/重复 index 的 row pairing | 构造 deterministic non-identity indices / correspondences，并对拍 staged scalar reference 与 public entry | 若 correctness 不稳，先修 row pairing，不跑 board。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| staged ordered reuse | source-indexed-cloud-pair | `PointXYZ` / `float` / x,y,z AoS | planned gtest：public matches staged scalar；candidate matches staged scalar | planned `row-source-expansion` filter | planned, if QEMU correctness passes | bench binary diagnostic | planned manifest / doctor | planned |
| staged ordered reuse | dual-indexed-cloud-pair | `PointXYZ` / `float` / x,y,z AoS | planned gtest | planned `row-source-expansion` context | optional unless source-indexed positive | bench binary diagnostic | planned if board summary exists | planned |
| staged ordered reuse | correspondence-pair | `PointXYZ` / `float` / query/match pairs | planned gtest | planned `row-source-expansion` context | optional unless source-indexed positive | bench binary diagnostic | planned if board summary exists | planned |
| production dispatch | any non-ordered row source | representative xyz AoS / `Scalar=float` | not_applicable in this phase | not_applicable | not_applicable | not_applicable | not_applicable | deferred until explicit PI1 |

## 实现和测试动作

| action | 内容 | 产物 | 完成判据 |
| --- | --- | --- | --- |
| A1 row source fixtures | 增加 deterministic non-identity source indices、dual indices 和 correspondences；target 按 row source 语义由 source 变换生成 | `include/impl/tedq_candidates.hpp` | public entry 与 staged scalar reference 对拍通过。 |
| A2 staged candidates | 增加 source-indexed、dual-indexed、correspondence 的 staged scalar / staged RVV candidate wrappers；candidate stats 记录 staging 边界 | `include/impl/tedq_candidates.hpp` | RVV 构建命中 ordered candidate；非 RVV 构建 fallback。 |
| A3 correctness tests | 增加非 identity row source correctness gtest | `src/test_tedq.cpp` | `make run_test_compare` Std/RVV 通过。 |
| A4 bench filter | 增加 `row-source-expansion` case-filter，输出 public baseline 与 staged candidate | `src/bench_tedq.cpp`、文档 | QEMU smoke 可生成日志形状；不写 QEMU 性能结论。 |
| A5 evidence wrapper | 若新增 board summary，扩展 summary script / Make target / registry | `script/`、`Makefile` | summary / manifest / doctor / registry fresh。 |
| A6 docs / matrix | 更新 README、testing overview、benchmark/evidence、optimization evidence、evaluation、phase result 和 matrix | topic-local docs | row source 结论与证据边界一致。 |

## Evidence Doctor 和 registry 规则

本阶段若只完成 QEMU correctness 和 bench smoke，Evidence Doctor 只用于日志形状，不产生性能结论。若运行 board repeated：

- 生成 `log/board/row_source_expansion_repeated/summary.md`。
- 生成同目录 `evidence_manifest.json` 和 `evidence_doctor.md`。
- 调用 evidence registry record target。
- 更新 README / benchmark-and-evidence / optimization-evidence 的 summary-only allowlist。

## 板卡复跑预算和决策桶

若进入 board repeated，使用 5-run、20 iterations、5 warm-up 的 bounded rerun budget。source-indexed 是优先对象；dual-indexed / correspondence 只有在 correctness 和 QEMU smoke 稳定后再决定是否上板。decision bucket 仍使用 `positive` / `weak_positive` / `neutral` / `negative` / `unstable`，QEMU timing 不进入 bucket。

## 继续 / 停止条件

若 A1-A4 在 QEMU correctness 下通过，且板卡入口可用，本阶段应继续到 source-indexed repeated board summary / doctor / registry。若 staged candidate 在 correctness 上失败、bench case 语义不清、Evidence Doctor 出现无法解释 Error，或继续需要修改 production，则停在 `turn_stop_deferred` 并输出恢复条件。

若 source-indexed staged candidate positive，只能进入 “row-source diagnostic positive” 状态，不能直接 production-ready；下一步需要新的 PI1 合同。若 dual-indexed / correspondence negative，应把它们记录为 attempted / rejected diagnostic，避免后续从 ordered-cloud-pair 自动外推。

## 文档更新清单

更新：

- `doc/phases/005-row-source-expansion/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/transformation_estimation_dual_quaternion-evaluation.zh.md`
- `doc/testing-overview.zh.md`
- `doc/correctness-tests.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/test-support-code-map.zh.md`

不创建 `doc-rvv/registration/transformation_estimation_dual_quaternion-RVV.zh.md`，因为本阶段没有 adopted production behavior。
