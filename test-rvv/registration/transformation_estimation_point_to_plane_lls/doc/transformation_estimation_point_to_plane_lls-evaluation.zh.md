# registration/transformation_estimation_point_to_plane_lls 函数级 RVV 评估

## 范围

目标源码：`registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls.hpp`。

专项目录：`test-rvv/registration/transformation_estimation_point_to_plane_lls/`。

主归属路径：`test-rvv/registration/transformation_estimation_point_to_plane_lls/doc/transformation_estimation_point_to_plane_lls-evaluation.zh.md`。topic 根目录下的旧同名文件已经删除；当前不保留 legacy pointer（旧路径指针）。

当前 EvidenceDecision：

```text
production-candidate/full-cloud-f32-aos-layout-gated-source-xyz-target-xyznormal-float-rvv-fused-formula-block-dispatch-representative-pointtypes
```

本评估文档只负责决策审计。长期实现说明、算法解释和证据边界见 `doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md`。
跨阶段候选搜索空间和下一 phase 恢复条件见 `test-rvv/registration/transformation_estimation_point_to_plane_lls/doc/optimization-roadmap.zh.md`。

Topic-local doc suite（主题本地文档套件）已在 Phase 040 补齐，用于承接测试、bench、证据白名单和代码地图：

| 读者问题 | 主归属 |
| --- | --- |
| 目录导航、常用命令、可提交证据 | `test-rvv/registration/transformation_estimation_point_to_plane_lls/README.zh.md` |
| 测试类型、运行入口和覆盖矩阵 | `doc/testing-overview.zh.md` |
| 每个 gtest 的输入、断言和证明边界 | `doc/correctness-tests.zh.md` |
| bench label、case-filter、checksum、QEMU/board/asm/registry 边界 | `doc/benchmark-and-evidence.zh.md` |
| 优化方式到代码、target 和证据的索引 | `doc/optimization-evidence.zh.md` |
| 聚合入口、内部头、test/bench 源和 production helper 代码地图 | `doc/test-support-code-map.zh.md` |

## 函数级结论

当前 production candidate 覆盖 `TransformationEstimationPointToPlaneLLS<PointSource, PointTarget, float>` 的 full-cloud 公开 overload，其中 source 侧要求 `x/y/z` 单个 `float` 字段并满足 `RVVXYZAoSFloatLayout`，target 侧要求 `x/y/z/normal_x/normal_y/normal_z` 单个 `float` 字段并满足 `RVVXYZNormalFloatLayout`。点字段 float32 和输出 `Scalar=float` 是两条独立边界；其它标量模板字段访问可编译但不满足 gate 的实例回到原 `ConstCloudIterator` 标量 helper。

EvidenceDecision 名称中的 `representative-pointtypes` 表示当前板卡证据覆盖 gate 空间中的三类代表组合：`PointNormal -> PointNormal`、`PointXYZ -> PointNormal` 和 `PointXYZ -> PointXYZINormal`。其它满足 layout gate 的点型组合会命中同一 production dispatch，但没有逐类型板卡结论；它们的当前依据是 traits/layout gate、QEMU correctness、production tests 和 fallback 审查。

四类公开入口的数据流来源与 `ConstCloudIterator` 统一方式见 `doc-rvv/registration/transformation_estimation_dataflows-RVV.zh.md`。本 topic 的 production 决策按数据流独立批准：

| 数据流 | row 来源 | 当前状态 |
| --- | --- | --- |
| full-cloud | `source[k] + target[k]` | f32 AoS layout-gated `Scalar=float` RVV production candidate。 |
| source-indexed | `source[indices_src[k]] + target[k]` | 标量；test-rvv 仅保留历史诊断。 |
| dual-indices | `source[indices_src[k]] + target[indices_tgt[k]]` | 标量；test-rvv 仅保留历史诊断。 |
| correspondences | `source[index_query] + target[index_match]` | 标量；test-rvv 仅保留历史诊断。 |

RowSourcePolicy（行来源策略）是 test-rvv diagnostic 框架，用来分离 row source 和 shared math pipeline（共享数学流水线），不是 production dispatch。indexed/correspondences 退化不能单因归因为 gather，还涉及 query/match 展开、容器访问、baseline、分布局部性和后段成本。

不在当前 EvidenceDecision 范围：

- source-indexed、dual-indices、correspondences。
- weighted LLS。
- `Scalar=double`。
- 不满足 source xyz / target xyz+normal f32 AoS layout gate 的点型组合。
- 额外性能探索或非 production-dispatch 新 bench 实验。

## 决策链

| 阶段 | 证据 | 结论 |
| --- | --- | --- |
| local fragment | test-rvv RowSourcePolicy、finite mask、staging、block-reduction helper 对拍标量。 | 数学路径可审查，但 diagnostic evidence 不等于 production evidence。 |
| full diagnostic | block-reduction diagnostic direct 5-run 板卡 median 64K/256K 均为 `1.27x`。 | full-cloud block 方向值得进入窄 production dispatch。 |
| public-entry-shaped | std 侧 public overload、RVV 侧 bench-only block shim，5-run median 64K `2.67x`、256K `2.69x`。 | 证明公开入口形态兼容强信号，但 public-entry-shaped 不等于 production dispatch。 |
| production direct | std/RVV 两侧都调用真实 public full-cloud overload。fused-formula 三类 production-dispatch 板卡 5-run median：`pointnormal` 64K/256K `2.80x/2.82x`，`pointxyz-to-pointnormal` `3.13x/3.15x`，`pointxyz-to-pointxyzinormal` `3.11x/3.14x`。 | 支撑 fused-formula 成为默认 generic production-candidate。 |

QEMU timing 不作为性能结论。QEMU 只用于 correctness、checksum、case filter 和日志格式。

## 为什么当前证据足够支撑 Candidate

当前 EvidenceDecision 可以升级到 production candidate，是因为 correctness（正确性）、performance（性能）和 boundary（边界）三层证据同时闭合：

| 层级 | 判据 | 当前证据 | 审计结论 |
| --- | --- | --- | --- |
| correctness | 真实 public overload、finite mask、`accepted_points`、`ATA/ATb`、matrix 和 fallback 都有覆盖。 | `ProductionFullCloud*` tests、generic point tests、invalid-lane tests、scale-stress tests、小规模和 `Scalar=double` fallback tests。 | 当前 full-cloud f32 AoS layout-gated `Scalar=float` path 在预算内对齐标量 reference。 |
| performance | 只使用目标硬件 production-dispatch A/B，不使用 QEMU timing。 | Milkv-Jupiter 三类代表点型 fused 5-run summary；std/RVV 两侧都调用同一 public full-cloud overload。 | 64K/256K median/min 均正向，未见明显低谷；不外推到未上板点型。 |
| boundary | EvidenceDecision 不超过证据覆盖面。 | `representative-pointtypes` 命名、dataflow 表、remaining risks 和 summary-only artifact。 | 不外推到 indexed、dual-indices、correspondences、weighted、`Scalar=double` 或所有 gate-allowed 点型。 |

这个判断允许 fused-formula 接入默认 production hot path，是因为新增 production-facing fused tests 已覆盖可见输出、法方程中间态、无效 lane、计数、fallback、representative generic source/target gate、scale-stress 和 near-cancellation。后续只有在再次改变 RVV hot path 指令逻辑、bench case、dispatch gate 或扩大覆盖范围时，才需要新增专项测试或重新取证。

## 标量流程与 RVV 流程对照

| 流程段 | 标量 production | 当前 RVV production |
| --- | --- | --- |
| 入口 | full-cloud overload 检查 source/target size。 | 同一入口先做窄 gate。 |
| row 读取 | `ConstCloudIterator` 顺序读取 `source[k]`、`target[k]`。 | `vlse32` strided load 分别按 `PointSource` / `PointTarget` 的 layout offset 和 stride 读取字段。 |
| finite 检查 | 逐 row 检查 9 个字段。 | 每个 VL chunk 生成 finite mask，invalid lane 置零。 |
| normal-equation | 按 row 顺序 double 累加上三角 `ATA` 和 `ATb`。 | fused-formula 逐点计算 `a/b/c/d` 后，按 A/B/C/N 四组做 vector partial sums 和横向规约，改变逐点计算树与 reduction tree。 |
| accepted_points | 每个有效 row 加一。 | block A 组用 `vcpop` 统计有效 lane。 |
| 求解和矩阵 | Eigen inverse + matrix construct。 | 复用同一求解和矩阵构造。 |
| fallback | 不适用。 | gate 失败回标量 helper。 |
| debug loss | 标量 helper 在 debug verbosity 下会复算并打印 loss。 | RVV fast path 成功后提前返回，当前不打印该 debug loss。 |

默认 production hot path 现在只有 fused-formula block。它保留 current block 的 A/B/C/N reduction 组织，但把 `a/b/c` 改为 `vfmsac` 形态，把 `d` 改为 `nx*(dx-sx) + ny*(dy-sy) + nz*(dz-sz)` 后用 `vfmacc` 累加，因此改变逐点计算树。current multiply/add/subtract block 只保留在 `include/impl/teptpl_reductions.hpp` 作为 test-rvv diagnostic A/B、benchmark 和历史证据 baseline，不作为 production selector 或长期 production 可配置行为。

`FullCloudBlockFusedFormulaNearCancellationMatchesStdWithinBudget` 已加入 test-rvv。它把 source/target 推到大绝对坐标、保留小相对位移，并叠加 invalid lane 和 scale-stress 条件，以约束 d 公式里的近似抵消区间。production-facing fused tests 还覆盖 `PointNormal -> PointNormal`、`PointXYZ -> PointNormal`、`PointXYZ -> PointXYZINormal`、小规模 fallback、layout gate 失败 fallback 和 `Scalar=double` fallback。

板卡 diagnostic direct 5-run 摘要索引为 `test-rvv/registration/transformation_estimation_point_to_plane_lls/output/board/block_fused_formula_5run_summary.md`。最终 production-dispatch current vs fused 5-run 摘要索引为 `test-rvv/registration/transformation_estimation_point_to_plane_lls/output/board/production_dispatch_generic_representative_5run_summary.md`。fused production-dispatch 三类代表点型 64K/256K median 分别为 `2.80x/2.82x`、`3.13x/3.15x`、`3.11x/3.14x`，min 均稳定正向；current baseline 在 256K generic rows 中出现 `1.33x` 和 `1.23x` 低谷。fused-formula 因此替换 current baseline 成为默认 production hot path。

## 测试 Inventory

| 测试组 | 测试 | 保留理由 |
| --- | --- | --- |
| production direct | `ProductionFullCloudPublicOverloadMatrixMatchesStdWithinBudget`、`ProductionFullCloudNormalEquationMatchesStdWithinBudget`、`ProductionFullCloudInvalidLanesMatchStdWithinBudget`、`ProductionFullCloudScaleStressMatchesStdWithinBudget`、generic `PointXYZ -> PointNormal` 和 `PointXYZ -> PointXYZINormal` case | 默认 fused production correctness 证据，覆盖 public overload、`accepted_points`、`ATA/ATb`、matrix、invalid lane、数值压力和 generic source/target layout gate；expected normal-equation 来自 test-only scalar reference，不再依赖 production detail 标量 helper。 |
| fallback | `ProductionFullCloudSmallInputFallsBackToScalar`、`ProductionFullCloudScalarDoubleFallbackSmoke`、`SmallInputFallsBackForIsolatedSizeGate` | 保护小规模和 `Scalar=double` 不误命中 RVV。 |
| RVV-only diagnostic | `FullCloudBlockReduction*`、`InvalidLaneMaskMatchesStd` | 保护当前 block-reduction math、invalid lane 和 reduction-tree 预算。 |
| fused-formula production-facing | `FullCloudBlockFusedFormulaReductionMatchesStdWithinBudget`、`FullCloudBlockFusedFormulaNearCancellationMatchesStdWithinBudget`、`ProductionFusedFullCloud*` tests | 保护 fused-formula direct helper 与默认 production path 的普通样本、near-cancellation、三类代表点型和 fallback。 |
| historical diagnostic | std/public 对拍、source-indexed、dual-indices、correspondences、trusted-dense、fused、grouped tests | 解释历史方案和未扩展范围，防止未来误把旧诊断收益当 production 证据。 |
| bench-only | `include/impl/teptpl_bench_*.hpp` 中 component-only、public-entry-shaped 和 historical rows；`src/bench_teptpl.cpp` 只是薄入口。 | 用于归因或形态复核，不单独决定 production。 |

## 测试框架布局审计

当前 topic 采用 `src/` 和 `include/` 的测试框架布局，并在 Phase 040 补齐 topic-local doc suite。
它采用 weighted sibling 的结构质量 bar，但不机械复制 weighted 的算法 helper、证据数字或 production 结论：

| 审计项 | 当前决策 | 理由 |
| --- | --- | --- |
| test / bench 源码位置 | adopted：四个 `src/test_teptpl_*.cpp` 与薄 `src/bench_teptpl.cpp`。 | Phase 050 已把单个长 gtest 源拆成 public semantics、candidates、production direct 和 row sources；bench CLI/registry 迁到 `include/impl`，薄入口保留 target 名。 |
| 长 topic 缩写 token | adopted：`teptpl`。 | 该 token 只用于 test-rvv 文件名和聚合入口，避免继续使用超长源码文件名；production 符号和 topic 名不变。 |
| 聚合入口 | adopted：`include/teptpl.h`、`include/test_teptpl.h`、`include/bench_teptpl.h`；旧 `test_support_transformation_estimation_point_to_plane_lls.hpp` 已删除。 | 共用入口、gtest-only 入口和 bench-only 入口分层后，bench 不再引入 gtest helper；恢复检查未发现源码、Make target 或脚本依赖旧头，因此不再保留 compatibility alias（兼容别名）。 |
| topic-local doc suite | adopted：README、testing overview、correctness tests、benchmark/evidence、optimization evidence、test-support code map。 | 当前 topic 有 production dispatch、fallback、多个 row source diagnostic、多个 candidate family、board summary 和 registry；只靠 evaluation 大文档会影响恢复和审查。 |
| `include/impl` 内部布局 | adopted。 | Phase 050 已把常规测试支撑内部头迁到 `include/impl/teptpl_*.hpp`，并新增 gtest-only 与 bench-only 内部 helper；旧 `test_support/` 不再作为当前 include 路径。 |
| sibling 结构经验 | adopted as structure quality bar（作为结构质量 bar 采用）。 | weighted sibling 的 `src/include` 结构证明该模块适合短 token 和聚合入口；source-indexed production 方案、证据数字和算法 helper 不迁移到当前 topic。 |

本轮没有删除、合并或重命名测试。原因是现有测试分别保护 reduction-tree、invalid lane、`accepted_points`、`ATA/ATb`、matrix、fallback 和 public overload；没有发现入口形态、输入构造、断言和 adversarial 条件完全重复的 case。

## Bench 审计

| case 类型 | 代表 case | 审计结论 |
| --- | --- | --- |
| diagnostic direct | `lls normal-equation full-cloud block-reduction pointnormal` | 只证明 test-rvv helper 的目标硬件表现；不能替代 production-dispatch。 |
| public-entry-shaped | `lls public-entry-shaped full-cloud block-reduction pointnormal` | 证明公开入口输入/输出形态下 block helper 有稳定正向；它不是真实 dispatch。 |
| production-dispatch | `lls production-dispatch full-cloud pointnormal`、`pointxyz-to-pointnormal`、`pointxyz-to-pointxyzinormal` | 三类 case 均为真实 public full-cloud overload 的 std/RVV A/B；current baseline 来自历史 production-shaped A/B，最终支撑 fused generic EvidenceDecision。 |
| fused-formula A/B | `lls normal-equation full-cloud block-fused-formula pointnormal`、`lls component full-cloud block-fused-formula no-solve pointnormal` | direct helper 只用于归因；默认接入依据是 production-dispatch fused A/B。 |
| indexed/correspondences historical | source-indexed、dual-indices、correspondences rows | 用于解释不扩展。退化不能单因归因为 gather，还可能来自 query/match 展开、容器访问、baseline、分布局部性和后段成本。 |
| component-only | load/gather、formula、mask/compress、tail、no-solve rows | 只提供局部线索，不等价于端到端 profile。 |

板卡 fused production-dispatch 5-run speedup summary：

| Benchmark Item | runs | 64K speedup median/min/p10 | 256K speedup median/min/p10 | 备注 |
| --- | ---: | --- | --- | --- |
| `pointnormal` | 5 | `2.80x/2.77x/2.78x` | `2.82x/2.81x/2.81x` | exact PointNormal 子集。 |
| `pointxyz-to-pointnormal` | 5 | `3.13x/3.11x/3.12x` | `3.15x/3.14x/3.14x` | generic source xyz gate。 |
| `pointxyz-to-pointxyzinormal` | 5 | `3.11x/3.11x/3.11x` | `3.14x/3.12x/3.12x` | generic target xyz+normal gate；未见明显低谷。 |

稳定 summary-only 证据索引：

```text
test-rvv/registration/transformation_estimation_point_to_plane_lls/output/board/production_dispatch_generic_representative_5run_summary.md
```

该 summary artifact（只提交摘要的证据文件）记录了 current/fused 两份 run1..run5 的命令、case filter、analyzer SHA256、完整 values 和当轮本机临时 archive 路径。长期审计以 summary artifact 内的 values、命令和 analyzer hash 为准，不依赖临时 archive 永久存在；顶层 board 日志仍可能被 runner 覆盖，不作为当前 generic 5-run 的稳定来源。

反汇编归属：

| 层级 | 证据 | 结论 |
| --- | --- | --- |
| helper-level asm | `buildPointToPlaneLLSFullCloudBlockRVVFusedFormula<...>` 三个实例化符号：`PointNormal -> PointNormal`、`PointXYZ -> PointNormal`、`PointXYZ -> PointXYZINormal`。符号内可见逐点 fused `vfmsac.vv` 和 `d` 公式/normal-equation 的 `vfmacc.vv`。 | fused production helper 自身包含预期指令。 |
| production-symbol asm | production-dispatch public overload case 的 call site 跳到上述 fused helper；例如 asm 中有 `jal` 到 `PointNormal, PointNormal`、`PointXYZ, PointNormal`、`PointXYZ, PointXYZINormal` 三个 fused 符号。 | 默认 public full-cloud RVV path 已归属到 fused production helper。 |
| unrelated bench/test helper asm | 直接 grep 到的其它 `vfmsac/vfmacc` 行可能来自 component/direct diagnostic 或其它 Eigen/vectorized helper。 | 这些行不能单独作为 production hot path 证据。 |

## Evidence / Output 审计

当前 evidence policy 是 `summary-only`：

- 不提交大量 raw run 目录。
- 不保留临时 QEMU 子目录作为长期证据。
- 已跟踪顶层 output 日志如果只是被 rerun 覆盖，应恢复为干净状态。
- 文档只记录摘要数字、命令、证据边界和不能证明的范围。

若未来选择提交日志，必须先使用 topic Makefile 的 `sanitize_output_logs` / `check_output_logs_sanitized`，或 `test-rvv/script/sanitize_evidence_logs.py --check <logs>`。

Phase 030 已接入 topic-local evidence registry（证据登记表）：

```text
test-rvv/registration/transformation_estimation_point_to_plane_lls/log/evidence_registry.json
```

registry 当前登记两类证据：被长期文档引用的 board summary-only artifact（板卡摘要证据）和本机
QEMU correctness log。恢复或提交前可用 `test-rvv/script/evidence_registry.py check` 检查 hash、
文档引用和是否有未登记覆盖。registry 不改变性能结论，也不把 QEMU timing 写成性能证据。
bench label、日志提交边界和 registry check 命令的主归属现在是
`test-rvv/registration/transformation_estimation_point_to_plane_lls/doc/benchmark-and-evidence.zh.md`。

## Traceability Map

Traceability Map（可追踪性地图）用于让 reviewer 从决策文档跳到关键源码、测试、bench 和证据摘要。它只列当前 EvidenceDecision 依赖的关键对象，不枚举每个小 helper。

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `TransformationEstimationPointToPlaneLLS::estimateRigidTransformation(cloud_src, cloud_tgt, matrix)` | production public entry | full-cloud public overload，先尝试 RVV gate，失败后回标量。 | registration 上游调用方。 | `estimatePointToPlaneLLSFullCloudRVV` 或 `estimateRigidTransformationFullCloudStd`。 | production boundary（生产边界）和 fallback coverage（回退路径覆盖）。 | `registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls.hpp` |
| `estimatePointToPlaneLLSFullCloudRVV` | production dispatch / fallback | `__RVV10__` 下的窄 RVV 尝试层；只覆盖 `Scalar=float` 和 f32 AoS layout gate。 | full-cloud public overload。 | fused-formula RVV estimate helper。 | dispatch gate（分流验收）和 unsupported-scope boundary（未覆盖范围边界）。 | `registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls.hpp` |
| `buildPointToPlaneLLSFullCloudBlockRVVFusedFormula` | production RVV helper | 以 A/B/C/N block groups 构造 point-to-plane normal-equation（法方程）。 | fused-formula estimate helper。 | Eigen solve 和 matrix construct。 | RVV hot path、asm attribution（反汇编归属）和 numerical budget（数值预算）。 | `registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls.hpp` |
| `estimateRigidTransformationFullCloudStd` | production Std helper | full-cloud fallback wrapper，继续使用原 `ConstCloudIterator` 标量 helper。 | full-cloud public overload。 | 原 scalar transformation estimate helper。 | fallback semantic preservation（回退语义保持）。 | `registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls.hpp` |
| `include/teptpl.h` | test support aggregator | test-rvv 共用聚合入口，汇总 reference、RVV math、row source、reduction 和 candidate helper。 | `include/test_teptpl.h`、`include/bench_teptpl.h`。 | `include/impl/teptpl_candidates.hpp` 等内部头。 | reviewer navigation（审查导航）和 test asset boundary（测试资产边界）。 | `test-rvv/registration/transformation_estimation_point_to_plane_lls/include/teptpl.h` |
| `include/test_teptpl.h` | gtest aggregator | gtest-only 聚合入口，加入 fixtures、assertions 和 production helper bridge。 | 四个 `src/test_teptpl_*.cpp`。 | `include/impl/teptpl_test_helpers.hpp`。 | correctness gate（正确性验收）入口分层。 | `test-rvv/registration/transformation_estimation_point_to_plane_lls/include/test_teptpl.h` |
| `include/bench_teptpl.h` | bench aggregator | bench-only 聚合入口，加入 fixture、component helper 和 case registry。 | `src/bench_teptpl.cpp`。 | `include/impl/teptpl_bench_cases.hpp`。 | bench wrapper boundary（性能测试包装边界）。 | `test-rvv/registration/transformation_estimation_point_to_plane_lls/include/bench_teptpl.h` |
| `support::accumulate_std_full` | diagnostic reference | 测试专用 full-cloud scalar reference，复刻当前 production 公式用于 expected normal-equation。 | production direct tests 和 RVV helper 对拍。 | `support::NormalEquation` assertions。 | correctness reference（正确性参考链路），不属于 production fallback。 | `test-rvv/registration/transformation_estimation_point_to_plane_lls/include/impl/teptpl_common.hpp` |
| `ProductionFullCloud*` tests | production direct tests | 覆盖 public overload、`accepted_points`、`ATA/ATb`、matrix、invalid lane、scale stress 和 fallback。 | `make ... run_test_std` / `run_test_rvv`。 | gtest assertions 和 QEMU logs。 | correctness gate（正确性验收）和 fallback coverage。 | `test-rvv/registration/transformation_estimation_point_to_plane_lls/src/test_teptpl_production_direct.cpp` |
| `production-dispatch full-cloud ...` bench rows | bench wrapper | std/RVV 两侧都调用真实 public full-cloud overload 的 production-dispatch A/B。 | `run_bench_compare --case-filter production-dispatch` 和板卡 target。 | analyze scripts / board summaries。 | board performance（板卡性能）和 checksum/log-shape evidence（校验和 / 日志形状证据）。 | `test-rvv/registration/transformation_estimation_point_to_plane_lls/include/impl/teptpl_bench_cases.hpp` |
| `production_dispatch_generic_representative_5run_summary.md` | evidence output summary | 三类代表点型 repeated board summary，记录命令、case filter、values 和 analyzer hash。 | fetched board analyze logs。 | topic docs、evaluation 和 phase docs。 | current performance truth（当前性能事实）和 summary-only evidence（摘要证据）。 | `test-rvv/registration/transformation_estimation_point_to_plane_lls/output/board/production_dispatch_generic_representative_5run_summary.md` |
| `log/evidence_registry.json` | evidence registry | 记录 summary-only evidence 和 QEMU correctness log 的 size/hash/doc refs，用于恢复和提交前检查 freshness（新鲜度）。 | `test-rvv/script/evidence_registry.py record`。 | `test-rvv/script/evidence_registry.py check`、phase result 和 reviewer。 | freshness guard（新鲜度门禁）；不替代 Evidence Doctor 或性能结论。 | `test-rvv/registration/transformation_estimation_point_to_plane_lls/log/evidence_registry.json` |
| `doc/phases/optimization-matrix.zh.md` | phase optimization matrix | 记录 candidate family、row source、test、bench、board、asm 和 Evidence Doctor 状态。 | phase plan/result。 | Handoff Packet 和 roadmap。 | phase recovery（阶段恢复）和 decision audit（决策审计）。 | `test-rvv/registration/transformation_estimation_point_to_plane_lls/doc/phases/optimization-matrix.zh.md` |
| `doc/optimization-roadmap.zh.md` | optimization roadmap | 记录 helper shape review、registry adoption、test split、row-source follow-up 等恢复条件。 | phase loop 恢复。 | 下一 phase plan。 | unblocked next action index（未阻塞下一步索引）。 | `test-rvv/registration/transformation_estimation_point_to_plane_lls/doc/optimization-roadmap.zh.md` |
| topic-local doc suite | documentation index | README、测试体系、gtest 语义、bench/evidence、optimization evidence 和 code map。 | Phase 040 doc-suite parity。 | reviewer 和下一轮 worker。 | reviewer navigation（审查导航）和 evidence whitelist（证据白名单）。 | `test-rvv/registration/transformation_estimation_point_to_plane_lls/README.zh.md`、`doc/*.zh.md` |

## 验证约定

本轮审计使用以下本地验证命令：

```text
git diff --check
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_compare
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls dump_bench_rvv
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_bench_compare BENCH_ARGS="--size 65536,262144 --case-filter production-dispatch"
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_board_bench_compare BENCH_ARGS="--size 65536,262144 --case-filter production-dispatch"
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls fetch_board_logs
python3 test-rvv/script/analyze_bench_repeated.py <five fetched analyze_bench_compare.log files>
```

本轮 reference/production-detail 边界清理后已重跑：

```text
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_std
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_rvv
```

两侧 QEMU correctness 都是 40/40 通过。该改动未改变 RVV hot path 指令逻辑、bench case 或 production dispatch gate，因此不要求重跑板卡。

Phase 010 测试框架布局迁移后已重跑：

```text
git diff --check
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_std
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_rvv
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls USE_PCL_RVV10=0 TARGET_BENCH=bench_transformation_estimation_point_to_plane_lls_std build/riscv/bench_transformation_estimation_point_to_plane_lls_std
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls dump_bench_rvv
```

std/RVV QEMU correctness 仍为 40/40 通过；std bench compile smoke 和 `dump_bench_rvv`
只作为 bench compile smoke（编译冒烟）与路径形状检查，证明新 bench 聚合入口和薄
`src/bench_teptpl.cpp` 可以在 std/RVV 两侧编译，RVV 侧还能生成反汇编。它们没有运行 bench，也不刷新性能结论。

板卡 5-run 使用 summary-only 策略：每轮 fetch 后只把脱敏摘要数字写入文档和 summary artifact，不提交 raw run 目录。当前 production stable index 是 `test-rvv/registration/transformation_estimation_point_to_plane_lls/output/board/production_dispatch_generic_representative_5run_summary.md`，其中记录的 `<local-raw-archive>/production-dispatch-current` 和 `<local-raw-archive>/production-dispatch-fused` 是当轮本机临时归档，不是长期依赖；长期审计以 summary artifact 的 values、命令和 analyzer hash 为准。fused-formula direct diagnostic A/B 的 stable index 是 `test-rvv/registration/transformation_estimation_point_to_plane_lls/output/board/block_fused_formula_5run_summary.md`。文档、输出清理、注释整理、不改变 RVV hot path/bench 逻辑的入口 wrapper 重构，或本轮这种 reference/test-support 边界清理不要求重跑板卡。若后续修改 RVV hot path 指令逻辑、bench case 或 production dispatch gate，则需要补反汇编/QEMU bench，并按变更风险判断是否复跑板卡。

## Production 接入判断

结论升级为 `production-candidate/full-cloud-f32-aos-layout-gated-source-xyz-target-xyznormal-float-rvv-fused-formula-block-dispatch-representative-pointtypes`。理由：

1. 覆盖范围足够窄，fallback 矩阵清晰。
2. production direct tests 覆盖 public overload、fallback、invalid lane、`accepted_points`、`ATA/ATb` 和 matrix。
3. fused-formula 三类代表点型 production-dispatch 板卡 A/B 都稳定正向；没有 current block 在 256K generic rows 中出现的明显低谷。
4. 历史 negative evidence 支持不扩展到 indexed、correspondences、trusted-dense、grouped 或 weighted。

仍不覆盖范围必须保持显式：full-cloud f32 AoS layout-gated `Scalar=float` 之外的入口全部是标量或独立 topic；满足 layout gate 但未落在三类代表组合内的点型，不具备逐类型板卡结论。

## Remaining Risk 与后续扩展条件

当前剩余风险：

| 风险 | 当前为什么未闭合 | 后续闭合条件 |
| --- | --- | --- |
| debug loss 日志差异 | RVV fast path 成功后提前 `return`，绕过原 scalar helper 的 `PCL_DEBUG` loss 复算。默认矩阵输出不变，但 debug verbosity 下日志行为不同。 | 若要求日志等价，需要抽出 debug loss helper 或在 RVV fast path 后复刻调试计算，并重跑专项测试。 |
| production helper size / duplication | full-cloud scalar normal-equation reference 位于 `test-rvv/registration/transformation_estimation_point_to_plane_lls/include/impl/teptpl_common.hpp`，production fallback 仍走 `ConstCloudIterator`。但 block A/B/C/N 组仍重复 load/formula；helper 规模和重复度仍是 reviewer 审查项。 | 合入前若审查要求，可压缩 RVV block helper，并重跑 correctness。 |
| test reference / production detail 边界 | production-facing normal-equation tests 现在用 test-only scalar reference 作为 expected，仅在 RVV build 下把 production RVV helper 输出转换为 test-support equation 对拍。 | 若后续新增 production detail helper 给测试使用，应先确认它是否参与 runtime fallback；不参与时优先放在 test-support。 |
| generic board variability | fused-formula 三类 production-dispatch 板卡 5-run 都稳定正向；current block 的 256K 低谷已作为替换依据之一记录在 summary artifact。 | 若 reviewer 要求更多泛型目标证据，可追加 repeated runs 或拆分板卡负载/调度波动；当前不外推到未上板点型。 |
| gate-allowed point-type coverage | 当前 production gate 允许更多 source xyz / target xyz+normal f32 AoS 组合，但板卡只覆盖三类代表组合。 | 若新增点型实例进入 release 风险面，需要补该点型的 production direct、asm 和板卡抽样，或临时收窄 production gate。 |
| fused-formula 数值树 | 默认 hot path 改为 fused-formula 后，逐点 `a/b/c/d` 的舍入树和标量/current block 不同；当前 tests 和板卡 A/B 在预算内通过，但不承诺 bitwise 等价。 | 若扩大点型、规模、Scalar 或公式结构，需要重新做 near-cancellation、scale-stress、production-dispatch A/B 和 asm attribution。 |
| indexed/correspondences 扩展 | 历史负向证据分布敏感，不能单因归因为 gather。 | 需要独立 profile 或消融拆分 gather、query/match 展开、容器访问、baseline、分布局部性和后段成本。 |

可选 follow-up 应另开范围：

| 方向 | 需要新增证据 |
| --- | --- |
| 更多 f32 AoS 点型实例 | 基于现有 source xyz / target xyz+normal traits、offset、layout gate，逐类型确认 traits/POD/layout；若新增特殊布局或字段组合，补 production direct、asm 和板卡抽样。 |
| indexed/correspondences | profile 或消融拆分 gather、query/match 展开、baseline 和分布局部性。 |
| `Scalar=double` | 数值预算和目标硬件性能独立证明。 |
| trusted-dense | `is_dense` 合同审计和 invalid-lane 语义重评估。 |
