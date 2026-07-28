# registration/transformation_estimation_point_to_plane_lls 函数级 RVV 评估

## 范围

目标源码：`registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls.hpp`。

专项目录：`test-rvv/registration/transformation_estimation_point_to_plane_lls/`。

当前 EvidenceDecision：

```text
production-candidate/full-cloud-f32-aos-layout-gated-source-xyz-target-xyznormal-float-rvv-block-dispatch-representative-pointtypes
```

本评估文档只负责决策审计。长期实现说明、算法解释和证据边界见 `doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md`。

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
| production direct | std/RVV 两侧都调用真实 public full-cloud overload，三类 production-dispatch 板卡 5-run median：`pointnormal` 64K/256K `2.73x/2.71x`，`pointxyz-to-pointnormal` `2.93x/2.77x`，`pointxyz-to-pointxyzinormal` `2.92x/2.81x`。 | 支撑当前 generic production-candidate。 |

QEMU timing 不作为性能结论。QEMU 只用于 correctness、checksum、case filter 和日志格式。

## 标量流程与 RVV 流程对照

| 流程段 | 标量 production | 当前 RVV production |
| --- | --- | --- |
| 入口 | full-cloud overload 检查 source/target size。 | 同一入口先做窄 gate。 |
| row 读取 | `ConstCloudIterator` 顺序读取 `source[k]`、`target[k]`。 | `vlse32` strided load 分别按 `PointSource` / `PointTarget` 的 layout offset 和 stride 读取字段。 |
| finite 检查 | 逐 row 检查 9 个字段。 | 每个 VL chunk 生成 finite mask，invalid lane 置零。 |
| normal-equation | 按 row 顺序 double 累加上三角 `ATA` 和 `ATb`。 | 按 a/b/c/normal 四组做 vector partial sums 后横向规约，改变 reduction tree。 |
| accepted_points | 每个有效 row 加一。 | block A 组用 `vcpop` 统计有效 lane。 |
| 求解和矩阵 | Eigen inverse + matrix construct。 | 复用同一求解和矩阵构造。 |
| fallback | 不适用。 | gate 失败回标量 helper。 |
| debug loss | 标量 helper 在 debug verbosity 下会复算并打印 loss。 | RVV fast path 成功后提前返回，当前不打印该 debug loss。 |

`a/b/c/d` 逐点公式当前显式使用 `vfmul`、`vfadd` 和 `vfsub` intrinsic（内建函数）组织，没有改成 fused-formula（融合公式）形态，用于让语义审计变量边界收敛；这不是禁止 FMA。normal-equation 的 `ATA/ATb` product accumulation（乘积累加）阶段已经使用 `vfmacc`。若未来把逐点公式也改为 fused intrinsic（融合乘加内建函数），需要单独做反汇编归属、误差预算和板卡 A/B。

## 测试 Inventory

| 测试组 | 测试 | 保留理由 |
| --- | --- | --- |
| production direct | `ProductionFullCloudPublicOverloadMatrixMatchesStdWithinBudget`、`ProductionFullCloudNormalEquationMatchesStdWithinBudget`、`ProductionFullCloudInvalidLanesMatchStdWithinBudget`、`ProductionFullCloudScaleStressMatchesStdWithinBudget`、generic `PointXYZ -> PointNormal` 和 `PointXYZ -> PointXYZINormal` case | 当前 production correctness 证据，覆盖 public overload、`accepted_points`、`ATA/ATb`、matrix、invalid lane、数值压力和 generic source/target layout gate。 |
| fallback | `ProductionFullCloudSmallInputFallsBackToScalar`、`ProductionFullCloudScalarDoubleFallbackSmoke`、`SmallInputFallsBackForIsolatedSizeGate` | 保护小规模和 `Scalar=double` 不误命中 RVV。 |
| RVV-only diagnostic | `FullCloudBlockReduction*`、`InvalidLaneMaskMatchesStd` | 保护当前 block-reduction math、invalid lane 和 reduction-tree 预算。 |
| historical diagnostic | std/public 对拍、source-indexed、dual-indices、correspondences、trusted-dense、fused、grouped tests | 解释历史方案和未扩展范围，防止未来误把旧诊断收益当 production 证据。 |
| bench-only | `bench_transformation_estimation_point_to_plane_lls.cpp` 中 component-only、public-entry-shaped 和 historical rows | 用于归因或形态复核，不单独决定 production。 |

本轮没有删除、合并或重命名测试。原因是现有测试分别保护 reduction-tree、invalid lane、`accepted_points`、`ATA/ATb`、matrix、fallback 和 public overload；没有发现入口形态、输入构造、断言和 adversarial 条件完全重复的 case。

## Bench 审计

| case 类型 | 代表 case | 审计结论 |
| --- | --- | --- |
| diagnostic direct | `lls normal-equation full-cloud block-reduction pointnormal` | 只证明 test-rvv helper 的目标硬件表现；不能替代 production-dispatch。 |
| public-entry-shaped | `lls public-entry-shaped full-cloud block-reduction pointnormal` | 证明公开入口输入/输出形态下 block helper 有稳定正向；它不是真实 dispatch。 |
| production-dispatch | `lls production-dispatch full-cloud pointnormal`、`pointxyz-to-pointnormal`、`pointxyz-to-pointxyzinormal` | 三类 case 均为真实 public full-cloud overload 的 std/RVV A/B；支撑当前 generic EvidenceDecision。 |
| indexed/correspondences historical | source-indexed、dual-indices、correspondences rows | 用于解释不扩展。退化不能单因归因为 gather，还可能来自 query/match 展开、容器访问、baseline、分布局部性和后段成本。 |
| component-only | load/gather、formula、mask/compress、tail、no-solve rows | 只提供局部线索，不等价于端到端 profile。 |

板卡 production-dispatch 5-run speedup summary：

| Benchmark Item | runs | 64K speedup median/min/p10 | 256K speedup median/min/p10 | 备注 |
| --- | ---: | --- | --- | --- |
| `pointnormal` | 5 | `2.73x/2.69x/2.70x` | `2.71x/2.49x/2.53x` | exact PointNormal 子集。 |
| `pointxyz-to-pointnormal` | 5 | `2.93x/2.90x/2.91x` | `2.77x/2.24x/2.39x` | generic source xyz gate。 |
| `pointxyz-to-pointxyzinormal` | 5 | `2.92x/2.89x/2.90x` | `2.81x/1.52x/2.04x` | generic target xyz+normal gate；256K 有一次低谷。 |

稳定 summary-only 证据索引：

```text
test-rvv/registration/transformation_estimation_point_to_plane_lls/output/board/production_dispatch_generic_representative_5run_summary.md
```

该 summary artifact（只提交摘要的证据文件）记录了 run1..run5 的命令、case filter、analyzer SHA256、完整 values 和当轮本机临时 raw archive 路径。长期审计以 summary artifact 内的 values、命令和 analyzer hash 为准，不依赖临时 raw archive 永久存在；顶层 board 日志仍可能被 runner 覆盖，不作为当前 generic 5-run 的稳定来源。用户后续单轮 production-dispatch smoke 与 5-run 结论方向一致，但不作为替代证据。

## Evidence / Output 审计

当前 evidence policy 是 `summary-only`：

- 不提交大量 raw run 目录。
- 不保留临时 QEMU 子目录作为长期证据。
- 已跟踪顶层 output 日志如果只是被最近 run 覆盖，应恢复为干净状态。
- 文档只记录摘要数字、命令、证据边界和不能证明的范围。

若未来选择提交日志，必须先使用 topic Makefile 的 `sanitize_output_logs` / `check_output_logs_sanitized`，或 `test-rvv/script/sanitize_evidence_logs.py --check <logs>`。

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

板卡 5-run 使用 summary-only 策略：每轮 fetch 后只把脱敏摘要数字写入文档和 summary artifact，不提交 raw run 目录。当前 stable index 是 `test-rvv/registration/transformation_estimation_point_to_plane_lls/output/board/production_dispatch_generic_representative_5run_summary.md`，其中记录的 `/tmp/teptpl_generic_board_5run_fetch_20260728_173246` 是当轮本机临时归档，不是长期依赖；长期审计以 summary artifact 的 values、命令和 analyzer hash 为准。文档、输出清理、注释整理或不改变 RVV hot path/bench 逻辑的入口 wrapper 重构不要求重跑板卡。若后续修改 RVV hot path 指令逻辑、bench case 或 production dispatch gate，则需要补反汇编/QEMU bench，并按变更风险判断是否复跑板卡。

## Production 接入判断

结论升级为 `production-candidate/full-cloud-f32-aos-layout-gated-source-xyz-target-xyznormal-float-rvv-block-dispatch-representative-pointtypes`。理由：

1. 覆盖范围足够窄，fallback 矩阵清晰。
2. production direct tests 覆盖 public overload、fallback、invalid lane、`accepted_points`、`ATA/ATb` 和 matrix。
3. 三类代表点型 production-dispatch 板卡 A/B 都是正向；64K 稳定，256K 有个别低谷但 median/p10 仍正向。
4. 历史 negative evidence 支持不扩展到 indexed、correspondences、trusted-dense、fused、grouped 或 weighted。

仍不覆盖范围必须保持显式：full-cloud f32 AoS layout-gated `Scalar=float` 之外的入口全部是标量或独立 topic；满足 layout gate 但未落在三类代表组合内的点型，不具备逐类型板卡结论。

## Remaining Risk 与后续扩展条件

当前剩余风险：

| 风险 | 当前为什么未闭合 | 后续闭合条件 |
| --- | --- | --- |
| debug loss 日志差异 | RVV fast path 成功后提前 `return`，绕过原 scalar helper 的 `PCL_DEBUG` loss 复算。默认矩阵输出不变，但 debug verbosity 下日志行为不同。 | 若要求日志等价，需要抽出 debug loss helper 或在 RVV fast path 后复刻调试计算，并重跑专项测试。 |
| production helper size / duplication | 公开入口已收束为 `*_RVV` 尝试和 `*_Std` fallback，但 block A/B/C/N 组仍重复 load/formula；helper 规模和重复度仍是 reviewer 审查项。 | 合入前若审查要求，可压缩 helper 或拆清 Std/RVV/reference 边界，并重跑 correctness。 |
| `buildPointToPlaneLLSFullCloudStd` / solve / matrix helper 职责 | 这些 helper 当前服务 production-facing tests 和 RVV 对拍，也与 production 源码同文件共存。 | 审查时需要确认它们作为 reference/test 支撑是否可接受；若不接受，应移动或收窄职责后重跑测试。 |
| generic board variability | 新增三类 production-dispatch 板卡 5-run 都正向，但 `PointXYZ -> PointXYZINormal` 256K 有一次 1.52x 低谷。 | 若 reviewer 要求更稳定的泛型目标证据，可追加 repeated runs 或拆分板卡负载/调度波动；当前作为性能波动风险记录。 |
| gate-allowed point-type coverage | 当前 production gate 允许更多 source xyz / target xyz+normal f32 AoS 组合，但板卡只覆盖三类代表组合。 | 若新增点型实例进入 release 风险面，需要补该点型的 production direct、asm 和板卡抽样，或临时收窄 production gate。 |
| FMA formula variant | 标量源码不是 fused 写法，但编译器可能做 FMA contraction。当前逐点公式显式使用 `vfmul`、`vfadd` 和 `vfsub`，没有做 fused-formula；`ATA/ATb` 乘积累加已使用 `vfmacc`。 | 若未来推进 fused formula，需要反汇编归属、`accepted_points`、`ATA/ATb`、matrix、invalid/scale-stress 误差预算和板卡 A/B。 |
| indexed/correspondences 扩展 | 历史负向证据分布敏感，不能单因归因为 gather。 | 需要独立 profile 或消融拆分 gather、query/match 展开、容器访问、baseline、分布局部性和后段成本。 |

可选 follow-up 应另开范围：

| 方向 | 需要新增证据 |
| --- | --- |
| 更多 f32 AoS 点型实例 | 基于现有 source xyz / target xyz+normal traits、offset、layout gate，逐类型确认 traits/POD/layout；若新增特殊布局或字段组合，补 production direct、asm 和板卡抽样。 |
| indexed/correspondences | profile 或消融拆分 gather、query/match 展开、baseline 和分布局部性。 |
| `Scalar=double` | 数值预算和目标硬件性能独立证明。 |
| trusted-dense | `is_dense` 合同审计和 invalid-lane 语义重评估。 |
