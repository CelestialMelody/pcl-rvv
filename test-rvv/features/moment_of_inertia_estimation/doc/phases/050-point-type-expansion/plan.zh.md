# Phase 050 plan: PointXYZ-like production evidence expansion

## 阶段意图和边界

本阶段不修改 production（生产源码）算法。目标是在 phase040 已采纳的 projected covariance fusion（投影协方差融合）production path 上，为常见 `PointXYZ`-like 点型补一组独立的 production-public（真实公开入口）证据，验证 `RVVXYZAoSFloatLayout<PointT>` 没有把 `PointXYZ` 的字段 offset、stride 或 POD（底层标准布局表示）外推到其它点型。

范围冻结：

- production entry：`MomentOfInertiaEstimation<PointT>::compute()`。
- adopted RVV helper：`computeProjectedCovarianceRVV()`。
- 点型：本阶段验证 `pcl::PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`、`PointXYZRGBNormal`；自定义点型保留到后续 phase。
- row source（行来源）：`indices_` indexed cloud（索引点云）。
- `Scalar`：生产实现中的 `float`。
- layout：`RVVXYZAoSFloatLayout<PointT>`，要求 `x/y/z` 都是单个 `float` 字段，且 AoS stride / offset 满足 RVV load 前提。
- 不证明：所有 PointXYZ-like 点型、真实数据集 workload、`Scalar=double`、完整 `compute()` 全量 RVV 替换。

## 当前状态清单

| item | status | path |
| --- | --- | --- |
| phase040 production evidence | `PointXYZ` public compute 5-run median `1.984x`，bucket `positive`，Evidence Doctor 0 Error / 0 Warning | `log/board/repeated_phase040_projected_covariance_production/summary.md` |
| production gate | helper 已使用 `RVVXYZAoSFloatLayout<PointT>` 和 `rvvMaxU32ByteOffsetElements<PointT>()` | `features/include/pcl/features/impl/moment_of_inertia_estimation.hpp` |
| uncovered point type scope | phase040 不能外推到 `PointXYZI` 或其它 PointXYZ-like 点型 | `doc/phases/040-projected-covariance-production-probe/result.zh.md` |
| current bench | `moi_public_compute` 仅构造 `PointXYZ` | `src/bench_moi.cpp` |

## 假设与候选族

候选族是 `PointXYZ-like production evidence expansion`。`MomentOfInertiaEstimation<PointT>` 标量路径只读取和写出 `x/y/z`，`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`、`PointXYZRGBNormal` 的额外字段不参与 moment、eccentricity、AABB 或 OBB 公式。若 production RVV gate 正确使用当前 `PointT` 的 traits offset 和 `sizeof(PointT)`，这些 typed cases 应与同坐标 `PointXYZ` 形成相同几何输出，并在板卡上保持正向收益。

主要风险：

- typed case 的 stride 大于或不同于 `PointXYZ`，indexed gather（索引离散加载）可能降低收益。
- output `PointT` 中非 xyz 字段不是本阶段语义目标；本阶段只比较几何输出。
- 如果 typed public bench 出现 neutral / negative，不自动回滚 phase040 `PointXYZ` 采纳范围，只限制对应 typed case 的 adopted scope。

## 优化矩阵

| candidate family | row source | point type / Scalar / layout | test | bench / board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| PointXYZ-like production evidence expansion | `indices_` indexed cloud | `PointXYZI`, `PointXYZRGB`, `PointXYZRGBA`, `PointXYZRGBNormal`, `float`, AoS, 32-bit byte offset gate | add typed public compute correctness and private helper hit test | `moi_public_compute_pointxyzi`, `moi_public_compute_pointxyzrgb`, `moi_public_compute_pointxyzrgba`, `moi_public_compute_pointxyzrgbnormal`, 5-run repeated board | existing production helper asm plus typed bench binary | phase050 manifest + Evidence Doctor | adopt each typed case if public board bucket positive and Doctor has no blocking Error |

## 实现和测试动作

1. 扩展 `src/test_moi.cpp`：新增 `PointXYZI` / `PointXYZRGB` / `PointXYZRGBA` / `PointXYZRGBNormal` public compute correctness（正确性）测试，对比同坐标 `PointXYZ` 输出；在 RVV 构建下新增 typed helper 命中测试。
2. 扩展 `src/bench_moi.cpp`：新增 typed `moi_public_compute_pointxyzi`、`moi_public_compute_pointxyzrgb`、`moi_public_compute_pointxyzrgba`、`moi_public_compute_pointxyzrgbnormal` case，计时边界仍是真实 public `compute()`。
3. 扩展 Makefile：新增 phase050 repeated board target、summary / manifest / doctor / registry freshness target。
4. 执行 QEMU correctness：`make run_test_compare`。
5. 执行 QEMU bench smoke：分别跑 typed case filter，至少覆盖 `moi_public_compute_pointxyzi` 与 `moi_public_compute_pointxyzrgb`；只验证 label 和日志形状，不使用 QEMU timing 做性能结论。
6. 执行 asm：`make dump_bench_rvv`，确认 typed bench binary 仍包含 projected covariance RVV 指令族。
7. 执行 board：按 typed case 分别跑 repeated board，默认每个 case 5 runs。
8. 执行 evidence freshness：`make evidence_status_phase050`。
9. 同步 result、matrix、roadmap、evaluation、README、长期 `doc-rvv` 和 Handoff。

## Evidence Doctor 和 registry 规则

phase050 使用：

- summary：`log/board/repeated_phase050_pointxyzi_production/summary.md`
- manifest：`log/board/repeated_phase050_pointxyzi_production/evidence_manifest.json`
- doctor：`log/board/repeated_phase050_pointxyzi_production/evidence_doctor.md`
- registry：`log/evidence_registry.json`

若 Evidence Doctor 有 Error，先判断是否是 typed case 的 checksum / A-B 边界 / 退化频率问题；不能用 phase040 `PointXYZ` 证据关闭其它 typed cases。若只有 metadata suggestion，则记录为后续补强，不阻塞正向 bucket。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production-public`，不使用 diagnostic 外推。 |
| A/B boundary | public typed `MomentOfInertiaEstimation<PointT>::compute()`。 |
| 当前决策问题 | 同一 adopted RVV helper 是否可扩展到常见 PointXYZ-like typed 范围。 |
| diagnostic 是否可外推到 production | 不外推；本阶段直接测真实公开入口。 |
| comparison-boundary / baseline mismatch 风险 | Std/RVV 使用同一 bench wrapper、同一点型、同一 synthetic indexed cloud、同一 timer boundary。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前已是 production probe；若 bucket 不正向，只把对应 typed case 标为 not adopted / deferred，不回滚 phase040 `PointXYZ`。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 不需要；没有新 RVV family，只扩展同一 helper 的点型证据。 |

## 板卡复跑预算和决策桶

默认 5 runs，`points=65536`、`iterations=5`、`warmup=1`，每个 typed case 单独跑一次。decision bucket（决策桶）沿用 summary 脚本：median >= 1.10 且 0/5 低于 1 为 `positive`；median >= 1.03 且退化不超过 1/5 为 `weak_positive`；median 在 0.97-1.03 为 `neutral`；median < 0.97 为 `negative`；其它为 `unstable`。

## 继续 / 停止条件

若 typed public board 为 positive 且 Evidence Doctor 无阻塞 Error，则把对应 typed case 加入已测 production evidence scope，并继续判断自定义点型或更宽 layout 是否作为下一 phase。若某个 typed case 为 weak / neutral / negative / unstable，则暂停该 typed case 的 adopted scope，把 phase040 的 `PointXYZ` 采纳范围保持不变，并说明 stride / layout 扩展当前不建议继续。

本阶段不命中停止条件：板卡可用，扩证据在当前 topic 授权范围内，生产算法无需扩大。

## 文档更新清单

- `doc/phases/050-point-type-expansion/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/moment_of_inertia_estimation-evaluation.zh.md`
- `README.zh.md`
- `doc-rvv/features/moment_of_inertia_estimation-RVV.zh.md`
- `tmp/rvv-work-logs/features/moment_of_inertia_estimation/current-handoff/current-handoff.zh.md`
