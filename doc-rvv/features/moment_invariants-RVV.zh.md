# Moment Invariants RVV

## 当前状态

`features/include/pcl/features/impl/moment_invariants.hpp` 已采用有界 RVV production path（生产路径）。真实公开入口是 `MomentInvariantsEstimation::computeFeature`：当 RVV 构建启用、输出类型为 `pcl::MomentInvariants`、输入点型满足 xyz 单 `float` AoS（结构数组）布局、点云为 dense、邻域 indices（索引）规模达到 16 且 32-bit byte offset 可表达时，indexed `computePointMomentInvariants(cloud, indices, ...)` 在 centroid（质心）之后用 RVV gather（离散加载）和 vector reduction（向量规约）累加六个中心矩。

当前已由 production-public（生产公开入口）板卡证据覆盖的输入点型为 `PointXYZ`、`PointXYZI`、`PointXYZRGB` 和 `PointXYZRGBA`。full-cloud overload、normal 复合点型、自定义点型、其它输出类型、`Scalar=double` 和 KdTree/search 优化不在本生产补丁范围内。

## 函数语义

`MomentInvariantsEstimation::computeFeature` 对每个查询点执行邻域搜索。搜索失败时输出 `j1/j2/j3 = NaN` 并把 `output.is_dense` 置为 false；搜索成功后调用 indexed `computePointMomentInvariants`。

indexed helper 先用 `compute3DCentroid(cloud, indices, xyz_centroid_)` 求邻域质心，再遍历邻域点。每个点减去质心后累加六个中心矩：

| 中心矩 | 标量贡献 |
| --- | --- |
| `mu200` | `dx * dx` |
| `mu020` | `dy * dy` |
| `mu002` | `dz * dz` |
| `mu110` | `dx * dy` |
| `mu101` | `dx * dz` |
| `mu011` | `dy * dz` |

最终三项输出为：

```text
j1 = mu200 + mu020 + mu002
j2 = mu200*mu020 + mu200*mu002 + mu020*mu002 - mu110^2 - mu101^2 - mu011^2
j3 = mu200*mu020*mu002 + 2*mu110*mu101*mu011 - mu002*mu110^2 - mu020*mu101^2 - mu200*mu011^2
```

## 当前采用的优化方式

RVV path 只接管 centroid 之后的 indexed moment accumulation。`computeFeature`、邻域搜索、输出写回和 full-cloud overload 保持原有流程。

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| indexed production RVV | adopted | helper 小，公开 API 不变，板卡 production-public 5-run 全部正向。 | Phase 030/040 summaries。 | 只覆盖 indexed neighbor path。 |
| traits-gated PointXYZ-like input | adopted | `RVVXYZAoSFloatLayout<PointT>` 证明 xyz 单 float、POD layout、stride 和 offset 前提。 | `PointXYZI/RGB/RGBA` correctness、asm、board。 | 不外推 normal 复合或自定义点型。 |
| vector reduction | adopted | 六个中心矩可在 VL chunk（可变向量长度分块）内独立规约。 | gtest 容差、production asm、board summary。 | 累加树不同，raw checksum 不要求严格相等。 |
| full-cloud overload | not_now | 当前 `computeFeature` 主路径使用 indexed 邻域；缺真实 caller/profile。 | helper-only correctness。 | 出现真实热点后另开 phase。 |
| search / centroid | scalar-only | 本 topic 只替换 moment accumulation；search 和 centroid 属于其它优化边界。 | 源码审计。 | profile 指向后另开 topic。 |

## VL Chunk 流程

每个 VL chunk 执行以下步骤：

1. 从 `indices` 读取邻域点下标，并乘以 `sizeof(PointT)` 得到 byte offset（字节偏移）。
2. 通过 `indexed_load3_f32m2<Pod, kX, kY, kZ>` 从当前点型布局中 gather `x/y/z`。
3. 用标量 centroid 广播成向量，得到 `dx/dy/dz`。
4. 生成六项贡献：`dx*dx`、`dy*dy`、`dz*dz`、`dx*dy`、`dx*dz`、`dy*dz`。
5. 用 `vfredusum` 把每项 chunk 内贡献规约到标量累加器。
6. 所有 chunk 完成后复用同一 `momentInvariantsFinalize` 公式计算 `j1/j2/j3`。

该实现没有 `vcompress` 或 scatter（离散写回），也没有改变输出容器布局。

## Fallback 矩阵

| 条件 | 行为 | 说明 |
| --- | --- | --- |
| 非 RVV 构建 | 只编译标量 helper。 | `__RVV10__` 外没有 RVV helper。 |
| `PointOutT != pcl::MomentInvariants` | 走标量 indexed helper。 | 当前只证明三值输出。 |
| 点型不满足 `RVVXYZAoSFloatLayout<PointT>` | RVV helper 返回 false。 | 避免字段 offset、POD 或 stride 不匹配。 |
| `cloud.is_dense == false` | RVV helper 返回 false。 | 当前 RVV path 不做 finite mask（有限值掩码）。 |
| 邻域规模小于 16 | RVV helper 返回 false。 | 小规模下 setup 成本不划算。 |
| 点云规模超过 u32 byte offset helper 范围 | RVV helper 返回 false。 | indexed load helper 使用 32-bit byte offset。 |
| full-cloud overload | 始终走 `momentInvariantsFullMomentsStd`。 | 当前没有 production direct 性能证据。 |

## 正确性与高效性证据链

| 证据 | 路径 / 命令 | 结果 | 边界 |
| --- | --- | --- | --- |
| QEMU correctness（QEMU 正确性验证） | `test-rvv/features/moment_invariants` 下 `make run_test_compare` | Std/RVV 各 8/8 pass。 | QEMU 不证明性能。 |
| board correctness（板卡正确性） | `make run_board_test` | RVV gtest 8/8 pass。 | 证明板卡可运行和容差。 |
| `PointXYZ` production board | `test-rvv/features/moment_invariants/log/board/repeated_phase030_production_compute_feature/summary.md` | median `1.067x`，min `1.023x`，max `1.085x`，0/5 below 1。 | production-public；只覆盖 `PointXYZ`。 |
| `PointXYZI` production board | `test-rvv/features/moment_invariants/log/board/repeated_phase040_production_pointxyzi_compute_feature/summary.md` | median `1.071x`，min `1.065x`，max `1.075x`，0/5 below 1。 | production-public；只覆盖 `PointXYZI`。 |
| `PointXYZRGB` production board | `test-rvv/features/moment_invariants/log/board/repeated_phase040_production_pointxyzrgb_compute_feature/summary.md` | median `1.078x`，min `1.066x`，max `1.091x`，0/5 below 1。 | production-public；只覆盖 `PointXYZRGB`。 |
| `PointXYZRGBA` production board | `test-rvv/features/moment_invariants/log/board/repeated_phase040_production_pointxyzrgba_compute_feature/summary.md` | median `1.078x`，min `1.059x`，max `1.087x`，0/5 below 1。 | production-public；只覆盖 `PointXYZRGBA`。 |
| Evidence Doctor | 对应 `evidence_doctor.md` | Phase 030/040 production summaries 均为 0 Error / 0 Warning / 2 Suggestion。 | metadata 和 binary identity 建议不阻塞当前采纳。 |
| production asm | `make check_production_rvv_asm` 和 typed asm targets | production symbols 中可归属 RVV indexed load 和 reduction。 | 反汇编归属不是性能结论。 |

Phase 000 helper-only 和 Phase 010 production-shaped diagnostic 只作为历史候选依据。最终性能结论使用接入后的 Phase 030/040 production-public 板卡数据。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `MomentInvariantsEstimation::computeFeature` | production public entry | 邻域搜索、fallback 和输出写回。 | PCL features estimator。 | indexed `computePointMomentInvariants`。 | production boundary。 | `features/include/pcl/features/impl/moment_invariants.hpp` |
| `momentInvariantsIndexedMomentsRVV` | production RVV helper | indexed gather 和中心矩 vector reduction。 | indexed public helper。 | `momentInvariantsFinalize`。 | production direct RVV path。 | `features/include/pcl/features/impl/moment_invariants.hpp` |
| `momentInvariantsIndexedMomentsStd` | production Std helper | 原 indexed 标量循环。 | fallback path。 | `momentInvariantsFinalize`。 | reference / fallback。 | `features/include/pcl/features/impl/moment_invariants.hpp` |
| `src/test_moment_invariants.cpp` | correctness test | production direct、typed 和 fallback tests。 | Makefile test targets。 | gtest output。 | correctness gate。 | `test-rvv/features/moment_invariants/src/test_moment_invariants.cpp` |
| `src/bench_moment_invariants.cpp` | bench wrapper | production-public case-filter。 | Makefile bench / board targets。 | summary generator。 | board performance input。 | `test-rvv/features/moment_invariants/src/bench_moment_invariants.cpp` |
| `script/check_mi_production_asm.py` | asm script | 符号级 RVV 指令归属检查。 | Makefile asm targets。 | reviewer。 | asm attribution。 | `test-rvv/features/moment_invariants/script/check_mi_production_asm.py` |
| `script/generate_mi_repeated_summary.py` | analysis script | 生成 summary 和 manifest。 | board repeated targets。 | Evidence Doctor。 | evidence manifest input。 | `test-rvv/features/moment_invariants/script/generate_mi_repeated_summary.py` |
| topic-local evaluation | documentation | 候选取舍、doc suite、fallback 和未覆盖范围主归属。 | reviewer / worker。 | 本文档引用。 | decision audit。 | `test-rvv/features/moment_invariants/doc/moment_invariants-evaluation.zh.md` |

## Production Closeout

| 项 | 最终状态 | 证据 |
| --- | --- | --- |
| 生产补丁范围 | 修改 `moment_invariants.hpp`，新增 Std/RVV helper 和 indexed public helper dispatch；public API 不变。 | 源码 diff。 |
| 覆盖范围 | `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`，`float`，dense AoS，indexed KdTree neighbor path。 | Phase 030/040 correctness、asm、board summaries。 |
| 不覆盖范围 | full-cloud overload、normal 复合点型、自定义点型、其它输出类型、`Scalar=double`、非 dense surface RVV、search/KdTree。 | fallback 矩阵和 roadmap。 |
| 证据策略 | summary-only；raw logs 不默认提交。 | `test-rvv/features/moment_invariants/log/evidence_registry.json`。 |
| 回退策略 | 任一 gate 不满足时走标量 helper；若后续证据反转，可移除 RVV dispatch 或收窄点型 gate。 | source fallback + gtest。 |

## 后续方向

当前没有需要同轮继续推进的高优先级未阻塞优化动作。后续可以做 evidence hardening（补环境和 binary hash）、normal 复合点型 / 自定义点型扩展，或在真实 profile 指向时另开 full-cloud/search topic；这些都需要新的 phase plan 和独立证据，不从当前四个点型的 weak-positive 结果外推。
