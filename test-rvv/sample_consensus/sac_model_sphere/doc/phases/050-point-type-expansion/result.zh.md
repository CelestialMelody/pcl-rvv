# Phase 050: 点类型扩展结果

## 执行范围

本阶段把 `src/bench_sac_model_sphere.cpp` 从固定 `PointXYZ` bench 扩展为可通过第三个参数选择 `PointXYZ` 或 `PointXYZI`，并为 `PointXYZI` 生成接入后的 production direct（真实生产入口直连）board repeated evidence（重复板卡证据）。

本阶段不改 production 源码。`selectWithinDistance` 当前仍使用 Phase 045/046 已采纳的 `vcompress` RVV path；Phase 050 只证明该 production gate 在 `PointXYZI` 这个点型上有独立板卡收益。

## TDD 回填

| 步骤 | 命令 / 证据 | 结果 |
| --- | --- | --- |
| RED | `make -C test-rvv/sample_consensus/sac_model_sphere run_bench_rvv BENCH_ARGS='128 1 PointXYZI'` 后检查 `^Dataset: .*PointXYZI` | 修改前 bench 固定 `PointXYZ`，第三个参数不会进入 Dataset 行，预期 RED 成立。 |
| GREEN | 同一命令重跑 | 输出 `Dataset: synthetic sac_model_sphere PointXYZI direct indexed shell cloud ...`，日志形状通过。 |
| 回归 | `make -C test-rvv/sample_consensus/sac_model_sphere run_test_compare` | Std/RVV 两侧各 5 个 gtest 通过。 |
| asm | `make -C test-rvv/sample_consensus/sac_model_sphere clean_bench_rvv dump_bench_rvv` | 反汇编生成成功；`PointXYZI` 实例下 `selectWithinDistanceRVV` 为 `27` 条 RVV 指令，`countWithinDistanceRVV` 为 `19` 条。 |

## 实现变化

| 文件 | 改动 |
| --- | --- |
| `src/bench_sac_model_sphere.cpp` | 新增 `runBenchForPointType<PointT>` 和 `fillPoint<PointT>`；默认仍是 `PointXYZ`，第三个参数 `PointXYZI` 时构造带 intensity 的点云。 |
| `script/generate_sphere_board_evidence_manifest.py` | 从 `Dataset:` 解析 point type，并把 comparison 的 `point_type`、`name`、`gate` 和 asm attribution 绑定到当前点型实例。 |
| `Makefile` | 新增 Phase 050 collect / manifest / doctor / registry / status target。 |

## 板卡结果

板卡为 Milkv-Jupiter，case 为 `PointXYZI`、65536 点、direct indexed `indices_`、200 iterations、5 warmup。`B/A > 1` 表示 RVV 构建更快。

| case | B/A values | median | min / max | 结论 |
| --- | --- | ---: | ---: | --- |
| public `selectWithinDistance` | `1.6685, 1.6197, 1.5978, 1.6139, 1.6184` | `1.6184x` | `1.5978x / 1.6685x` | `PointXYZI` production public entry 全 run 正向，支持保持当前 `RVVXYZFloatLayout<PointT>` gate。 |
| public `countWithinDistance` | `2.1313, 2.0197, 2.0212, 2.0298, 2.0766` | `2.0298x` | `2.0197x / 2.1313x` | 既有 count RVV 在 `PointXYZI` 上也稳定正向。 |
| public `getDistancesToModel` | `0.9819, 1.0011, 1.0162, 1.0304, 0.9859` | `1.0011x` | `0.9819x / 1.0304x` | public entry 仍为标量；不作为 RVV 采纳证据。 |
| diagnostic select candidate | `1.3618, 1.3445, 1.3287, 1.3225, 1.3473` | `1.3445x` | `1.3225x / 1.3618x` | 历史测试专用候选仍正向，但不替代 production direct。 |
| diagnostic getDistances candidate | `0.7199, 0.7157, 0.7197, 0.7306, 0.7199` | `0.7199x` | `0.7157x / 0.7306x` | 继续支持当前 getDistances candidate family rejected。 |

板卡执行仍报告远端 clock skew warning。该 warning 没有造成 gtest、bench 或日志抓取失败；本阶段记录为环境 warning，不影响当前 decision bucket。

## Evidence Doctor 处理

`point-type-repeated-evidence-doctor.md` 输出 `Errors=2, Warnings=0, Suggestions=1`。

两个 Error 和一个 Suggestion 均属于 `getDistancesToModel`：

- public `getDistancesToModel PointXYZI` 有 2/5 run 低于 1，且 median 接近 1；该 public entry 没有 RVV dispatch，因此只能作为标量 public entry 的构建噪声 / 稳定性提示。
- diagnostic `getDistancesToModel PointXYZI` candidate 5/5 run 低于 1，继续支持当前 scratch + scalar sqrt/store 实现族 rejected。

public `selectWithinDistance PointXYZI` 行没有 Error / Warning，且全 run 正向。本阶段只用该行关闭点类型扩展的 `selectWithinDistance` 生产证据。

## Evidence Registry

本阶段新增 summary evidence：

| run label | path |
| --- | --- |
| `sphere-phase050-point-type-xyzi-repeated-board` | `test-rvv/sample_consensus/sac_model_sphere/doc/phases/050-point-type-expansion/point-type-repeated-evidence-manifest.json` |
| `sphere-phase050-point-type-xyzi-repeated-board` | `test-rvv/sample_consensus/sac_model_sphere/doc/phases/050-point-type-expansion/point-type-repeated-evidence-doctor.md` |
| `sphere-phase050-point-type-xyzi-repeated-board` | `test-rvv/sample_consensus/sac_model_sphere/doc/phases/050-point-type-expansion/point-type-repeated-evidence-doctor.json` |

Raw board logs 位于 `test-rvv/sample_consensus/sac_model_sphere/log/board/repeated-20260828-phase050-point-type-xyzi/`，默认 local-only。

## Diagnostic 到 Production 错配审计回填

| question | answer |
| --- | --- |
| evidence role | production direct point-type expansion |
| A/B boundary | Std build public `selectWithinDistance<PointXYZI>` vs RVV build public `selectWithinDistance<PointXYZI>` |
| 当前决策问题 | 当前泛型 layout gate 对 `PointXYZI` 是否值得保持生产分流 |
| diagnostic 是否可外推到 production | 不外推；本阶段直接跑接入后的 public entry。 |
| comparison-boundary / baseline mismatch 风险 | select 行低；两侧使用同一点型、同一输入、同一 public entry。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段结果 positive-stable；无需收窄 gate。 |
| clean adoption 是否需要同一 production boundary 内证据 | 已具备 `PointXYZI` 同边界 production direct board evidence。 |

## Continue / Stop Decision

`continue_stop_decision`：`continue_to_next_phase`。

`stop_condition_hit`：none。

Phase 050 关闭 `PointXYZI` 的 production point-type performance row，但不关闭 `PointXYZRGB`、`PointXYZRGBA` 或自定义 registered xyz 点型。当前仍有一个高优先级结构动作：用户指出本 topic 的 test support 尚未采用 `include/` / `include/impl` 布局；`.agents/config/defaults.yaml` 和 `rvv-test/references/optimization-phase-loop.zh.md` 都把该布局作为默认结构质量门槛。Phase 055 将审计并迁移当前 test/bench helper 结构，避免后续点型扩展继续堆在 `src/*.cpp` 大文件里。
