# 010 production integration: PointNormal 窄范围接入计划

## 阶段意图和边界

本阶段把上一阶段的 `partial-production-candidate`（局部生产候选）推进为有界生产探针：只在 `surface/include/pcl/surface/impl/marching_cubes_rbf.hpp` 的 `MarchingCubesRBF<PointNT>::voxelizeData()` 中接入 `pcl::PointNormal` exact-type gate（具体点型门控）下的 RVV（RISC-V Vector，可变向量扩展）路径。其它 `PointNT`、非 RVV 构建、小规模输入和未覆盖布局必须自然回退到原标量语义。

本阶段证明：

- `pcl::PointNormal` 公开生产入口在 RVV 构建中能命中 RVV matrix fill（矩阵填充）和 voxel grid evaluation（体素网格求值）。
- 同一公开入口的 Std/RVV（标量 / RVV）构建在板卡上是否有足够收益，是否值得建议保留 production patch（生产补丁）。
- 生产接入后的测试、反汇编和 Evidence Doctor（证据体检）是否支撑 PI5 用户确认检查点。

本阶段不证明：

- `PointXYZRGBNormal`、`PointXYZINormal` 或 PointNormal-like 泛型 traits（字段特征）已经覆盖。
- `Scalar=double`、其它输入规模、其它 MarchingCubes surface extraction 阶段或 `createSurface()` 输出构造已被本次 RVV 证明。
- 新 RVV family（实现族）已 clean-adopt（干净采纳）。PI5 后仍需要用户明确确认保留 / 采纳。

## 当前状态清单

| 项 | 状态 | 路径 / 证据 |
| --- | --- | --- |
| 生产源码 | 尚未接入 RBF RVV；`voxelizeData()` 是单个大标量主体 | `surface/include/pcl/surface/impl/marching_cubes_rbf.hpp` |
| 诊断 helper | 已复刻 centers、RBF matrix、Eigen solve、grid evaluation | `test-rvv/surface/marching_cubes_rbf/include/impl/marching_cubes_rbf_core.hpp` |
| QEMU correctness | Std/RVV gtest 已通过，证明诊断 helper 数值一致 | `test-rvv/surface/marching_cubes_rbf/log/qemu/run_test_*.log` |
| 板卡诊断收益 | full pipeline 约 `1.21x-1.28x`，matrix fill 约 `2.28x-2.33x` | `test-rvv/surface/marching_cubes_rbf/log/board/analyze_bench_compare.log` |
| Evidence Doctor | `Errors=0`，低 run count warnings | `test-rvv/surface/marching_cubes_rbf/log/board/evidence_doctor.md` |

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | 上一阶段是 `production-shaped diagnostic`；本阶段新增 `production-public`。 |
| A/B boundary | 上一阶段是 test helper；本阶段要求 public overload / production dispatch。 |
| 当前决策问题 | `RVV-vs-scalar`，判断真实 `voxelizeData()` RVV 是否快于当前标量实现。 |
| diagnostic 是否可外推到 production | 只能作为升级理由，不能替代 production evidence。production 中对象状态、模板实例化和 fallback gate 必须重测。 |
| comparison-boundary / baseline mismatch 风险 | 有。诊断 helper 使用 SoA 输入和测试 wrapper；production 从 `PointNormal` cloud 进入，并由真实 `grid_`、`lower_boundary_`、`size_voxel_` 写回。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 允许，条件是只接 exact `PointNormal`、不改 public API、生产 patch 可独立审查，PI5 后停等用户确认。当前诊断为正向。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 本阶段没有已有 adopted RBF family；public Std/RVV positive 可支持 bounded production candidate，但 PI5 后仍需用户确认。 |

## 优化矩阵

| candidate family | row source policy | point type / layout | scope and entry | correctness / fallback target | bench / board target | asm boundary | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| centers SoA + RVV matrix fill + RVV grid reduction | single input cloud, off-surface pair expansion | exact `pcl::PointNormal`, `N >= 16`, RVV build | `MarchingCubesRBF<PointNormal>::voxelizeData()` | production direct gtest: grid value tolerance、active sign checksum；fallback gtest: 非 PointNormal 或小规模不崩溃并走标量语义 | `mcrbf_prod_pointnormal_*` Std/RVV board compare | bench RVV binary 中 `vle64/vse64/vfmacc/vfsqrt/vfredusum`，尽量归属到 production helper | JSON manifest + Evidence Doctor | planned |
| PointNormal-like generic expansion | normal traits + AoS layout | `PointXYZRGBNormal`、`PointXYZINormal` 等 | 后续 phase | 独立 correctness、fallback、bench、asm、board | 后续新增 | 后续新增 | 后续新增 | deferred |

## 实现和测试动作

1. 在 production 头中抽出内部标量 helper，让 `voxelizeData()` 的 fallback 边界清晰。
2. 在 `__RVV10__` 且 `PointNT == pcl::PointNormal` 且 `N >= 16` 时尝试 RVV helper；不满足时走标量 helper。
3. RVV helper 保留 Eigen `fullPivLu()` 为标量 solver，只接管 RBF matrix fill 与 grid evaluation；使用 `double` RVV lane 和向量 reduction，结果用测试容差和 sign/topology checksum 验证。
4. 在 `test-rvv` 新增 production direct gtest 和 bench case，case label 使用 `mcrbf_prod_pointnormal_*`，与诊断 case 分层。
5. 运行 QEMU correctness（正确性）、窄 QEMU bench smoke（只验证日志形状）、反汇编、板卡 Std/RVV compare 和 Evidence Doctor。

## fallback 矩阵

| 条件 | 行为 |
| --- | --- |
| 非 RVV 构建 | 只编译并执行标量 helper。 |
| `PointNT` 不是 exact `pcl::PointNormal` | RVV helper 编译期返回 false，公开入口走标量 helper。 |
| `input_->size() < 16` | 走标量 helper，避免小规模 RVV overhead。 |
| Eigen solver、`createSurface()`、mesh 输出 | 保持原标量 / 既有路径。 |
| 数值容差或 topology checksum 失败 | 阻塞 PI5，不建议采纳，等待用户确认回滚或修正。 |

## 板卡预算和决策桶

- 初始 budget：production bench 每个 case `5` 次 iteration、`2` 次 warm-up，执行一次全 case smoke。
- 若 Evidence Doctor 出现 Error 或 speedup 接近 `1.0x` 且方向摇摆，最多追加一次同边界复跑。
- `positive`：production public full cases 稳定大于 `1.05x`，无 Evidence Doctor Error，correctness 与 asm 通过。
- `weak_positive`：`1.00x-1.05x` 或个别 case 弱正向，需要用户判断。
- `neutral/negative`：均值不优于标量或收益无法覆盖维护成本，PI5 建议不保留。
- `unstable`：预算耗尽后方向跨桶摇摆，降级为需要用户 / reviewer 判断。

## 文档更新清单

- 本 phase 完成后写 `result.zh.md`，回填 PI1-PI5、命令、证据和建议。
- 更新 `doc/phases/README.zh.md`、`optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md` 和 `doc/marching_cubes_rbf-evaluation.zh.md`。
- 若 production evidence 支持保留，本轮只可创建 / 更新 `doc-rvv/surface/marching_cubes_rbf-RVV.zh.md` 的 production-candidate 草稿状态；最终 adopted production behavior 仍等待用户确认。

## 继续 / 停止条件

继续到 PI2-PI5 的条件：

- production gate 可保持在 exact `PointNormal`，不扩大 public API。
- 测试资产能构造真实 production direct evidence。
- 板卡可用。

停止条件：

- 需要扩大到泛型 normal traits、其它生产文件或 public API。
- production direct correctness、asm 或板卡证据无法闭合。
- PI5 完成后，无论正负，都停在用户确认检查点。

下一阶段默认入口：若 PI5 positive 且用户确认保留，进入 adoption closeout 和泛型点型扩展计划；若用户未确认，保持 `pending_user_confirmation_adopt_production`。
