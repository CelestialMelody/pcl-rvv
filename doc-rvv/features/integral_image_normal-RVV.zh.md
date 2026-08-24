# Integral Image Normal RVV

## 当前状态

`features/include/pcl/features/impl/integral_image_normal.hpp` 当前采用一条有界 RVV production path
（生产路径）：`IntegralImageNormalEstimation<PointInT, PointOutT>::computeFeature(PointCloudOut&)`
在 `__RVV10__` 构建下，若 `PointInT` 满足 `pcl::rvv::RVVXYZAoSFloatLayout<PointInT>`，会先用 RVV
初始化 depth-change map（深度突变图）和 distance map（距离图）；不满足 gate（准入条件）时执行同语义
Std helper。

这份文档只描述已采纳的 map-prep production 行为。average 3D gradient diff-buffer、distance transform、
normal solver、indices 输出和其它 normal method 的诊断过程主归属在 `test-rvv/features/integral_image_normal/doc/**`。

## 函数语义和标量路径

公开入口通过 `Feature::compute()` 进入 `computeFeature(PointCloudOut&)`。该函数的主流程是：

1. 为 organized input cloud 构造 depth-change map。每个内部像素读取当前点 `z`、右邻 `z` 和下邻 `z`；
   若深度差超过 `max_depth_change_factor_ * (abs(depth) + 1) * 2`，或任一参与深度不是 finite
   （有限值），则把当前点和对应邻点标记为 0。
2. 把 depth-change map 转成 distance map 初值。标记为 0 的位置写 `0.0f`，其它位置写
   `width + height`。
3. 执行两遍 distance transform（距离传播），把距离从深度突变边界向外传播。
4. 根据是否使用完整 organized cloud，进入 `computeFeatureFull()` 或 `computeFeaturePart()`。
5. 每个输出点调用 `computePointNormal()` 或 mirror variant，查询积分图、计算 normal / curvature，并按
   border policy、mirror policy 和 viewpoint flip（视点翻转）写出结果。

RVV 当前只接管第 1-2 步。distance transform、normal query、Eigen solver、border/mirror policy、
indices 子集输出和最终 normal 写回仍保持既有标量实现。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| dispatch / fallback（分流 / 回退） | `__RVV10__` 下调用 `tryInitializeMapPrepRVV`；返回 false 时调用 `initializeMapPrepStd`。 | 生产 diff 小，公开 API 不变，fallback 清楚。 | `run_test_compare` Std/RVV 各 5/5 pass。 | 非 RVV 构建自然只有 Std helper。 |
| 输入布局 | `PointInT` 必须满足 `RVVXYZAoSFloatLayout<PointInT>`，当前 RVV stage 实际只读取 z 字段。 | 通过 PCL traits 和 AoS layout gate 避免硬编码 `PointXYZ` offset。 | public compute correctness 和 board production direct case。 | 未证明所有 xyz-like 点型都有相同性能收益。 |
| depth-change map | VL chunk 中跨步读取当前、右邻、下邻 depth，构造右向 / 下向 edge mask，并用 masked byte store 写 0。 | 0 写入是 idempotent（幂等），当前 / 右邻 / 下邻多次写 0 与标量语义一致。 | `map_prep_*` diagnostic 5-run 约 5.1x-5.3x。 | 非 finite、tail width 和边界由 correctness test 覆盖；其它 normal method 不外推。 |
| distance map initialization | 连续读取 byte map，用 `vmseq` 生成 zero mask，在 `0.0f` 与 `width + height` 之间选择并写 float map。 | 原标量是逐元素 select，适合连续 RVV store。 | public compute distance-map test 与 checksum 一致。 | 后续 distance transform 仍标量。 |
| AVERAGE_3D_GRADIENT diff-buffer | 不接 production。 | diff-only 大图有收益，但 production-shaped / exact PCL profile 弱或不稳定。 | Phase 020/030/040/050 Evidence Doctor findings。 | 只有新的同边界 production direct 证据反转时再恢复。 |
| distance transform / normal solver | 暂缓。 | distance transform 有行内传播依赖；normal solver 混合积分图查询、Eigen 解算和输出策略。 | 当前收益已经来自 map-prep；没有证据支持扩大 patch。 | 需要另开 phase，先做 profile 和算法证明。 |

## VL Chunk 流程

RVV map-prep 对每一行的内部像素按 VL chunk（可变向量长度分块）处理：

```text
for each row except the last:
  for each chunk over columns [0, width - 2]:
    depth  = strided_load z(row, col)
    depthR = strided_load z(row, col + 1)
    depthD = strided_load z(row + 1, col)

    threshold = max_depth_change_factor * (abs(depth) + 1) * 2
    edge_r = abs(depth - depthR) > threshold or !finite(depth) or !finite(depthR)
    edge_d = abs(depth - depthD) > threshold or !finite(depth) or !finite(depthD)

    depth_change_map[current] = 0 where edge_r or edge_d
    depth_change_map[right]   = 0 where edge_r
    depth_change_map[down]    = 0 where edge_d

for each chunk over all pixels:
  distance_map = depth_change_map == 0 ? 0.0f : float(width + height)
```

最后一行和最后一列不作为“当前像素”参与右 / 下邻检查，但仍可能被相邻内部像素写成 0；这与原标量循环一致。

## 数值算例

假设一行中有三个相邻 depth：

```text
current = [1.0,  2.0, NaN]
right   = [1.1, 10.0, 3.0]
down    = [1.0,  2.1, 4.0]
max_depth_change_factor = 0.02
```

第一个 lane 的 threshold 是 `0.02 * (abs(1.0) + 1) * 2 = 0.08`，右向差 `0.1` 超阈值，
因此当前点和右邻写 0。第二个 lane 的右向差 `8.0` 超阈值，当前点和右邻写 0。第三个 lane 的
current 是 NaN，因此当前点、右邻和下邻都按对应 edge mask 写 0。

distance map 初始化只看最终 depth-change map：map 为 0 的位置写 `0.0f`，其余位置写
`width + height`。后续距离传播仍由原标量代码处理。

## Fallback 矩阵

| 条件 | 行为 | 维护边界 |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | RVV helper 不编译，直接使用 Std helper。 | 非 RVV 行为与原语义一致。 |
| `PointInT` 不满足 `RVVXYZAoSFloatLayout` | `tryInitializeMapPrepRVV` 返回 false，调用 Std helper。 | 其它模板实例保持标量语义，不声明 RVV 性能收益。 |
| NaN / Inf depth | RVV finite mask 与标量 `std::isfinite` 语义对齐。 | correctness 覆盖非有限 depth。 |
| 非整齐宽高 / tail | 使用 `vsetvl` 处理 tail。 | board case 覆盖 `641x481_tail`。 |
| `indices_` 非空 | map-prep 仍对完整 organized input 构建 distance map，后续 `computeFeaturePart()` 保持标量。 | 当前 RVV stage 不改变 indices 输出顺序。 |
| 其它 normal method | map-prep 前缀仍可执行；method-specific normal 初始化和查询保持标量。 | 当前 production bench 只覆盖 `AVERAGE_DEPTH_CHANGE`。 |

## Bench 与证据

证据主归属：

- Phase 050 result：`test-rvv/features/integral_image_normal/doc/phases/050-map-prep-production-probe/result.zh.md`
- evaluation：`test-rvv/features/integral_image_normal/doc/integral_image_normal-evaluation.zh.md`
- benchmark/evidence：`test-rvv/features/integral_image_normal/doc/benchmark-and-evidence.zh.md`
- repeated summary：`test-rvv/features/integral_image_normal/log/board/repeated-summary.md`
- Evidence Doctor：`test-rvv/features/integral_image_normal/log/board/evidence_doctor.md`

板卡 repeated 数据集是 `integral_image_normal_synthetic_diagnostics`，每 run 200 iterations、1 warmup，共 5 runs。

| case | 入口 / 计时边界 | evidence role（证据角色） | median speedup | mean speedup | 证明点 | 不能证明 |
| --- | --- | --- | ---: | ---: | --- | --- |
| `map_prep_320x240` | map-prep helper | diagnostic | 5.27x | 5.25x | RVV 前缀本身在常规 organized image 下强正向。 | 完整 public `compute()` 收益。 |
| `map_prep_641x481_tail` | map-prep helper | diagnostic | 5.13x | 5.11x | 非整齐宽高和 tail 下前缀仍强正向。 | 泛型点类型完整外推。 |
| `prod_compute_avg_depth_320x240` | public `compute()`，`AVERAGE_DEPTH_CHANGE` | production public | 1.06x | 1.06x | 真实公开入口在当前规模下有小幅收益。 | 其它 normal method、indices 路径、真实 workload 全集。 |
| `prod_compute_avg_depth_641x481_tail` | public `compute()`，`AVERAGE_DEPTH_CHANGE` | production public | 1.06x | 1.06x | tail 规模下真实公开入口仍有小幅收益。 | 所有 point type / layout 的性能。 |

Evidence Doctor（证据体检）结果为 `Errors=3, Warnings=21, Suggestions=8`。Errors 来自非 production-direct
profile component 的退化频率，不指向 map-prep production patch 的 correctness 或 checksum。`prod_compute_*`
两项各有 1/5 run 低于 1，因此生产结论写成 weak-positive（弱正向），但用户已确认采纳并保留当前 patch。

## 正确性与高效性证据链

| 层级 | 证据 | 结论 |
| --- | --- | --- |
| correctness（正确性） | `make -C test-rvv/features/integral_image_normal run_test_compare`：Std/RVV 各 5/5 pass。 | test helper 和 public `compute()` distance map 与 reference 一致。 |
| production direct（真实生产入口） | `IntegralImageNormalProductionRVV.PublicComputeDistanceMapMatchesReference` 调用真实 `compute()`。 | production patch 没有破坏最终 distance map 语义。 |
| asm attribution（反汇编归属） | `dump_test_rvv` / `dump_bench_rvv` 中存在 `vlse32`、masked `vse8`、`vse32`、`vmseq`、`vfsub`、`vsetvli`。 | RVV build 中目标路径存在；helper 是 header inline template。 |
| performance（性能） | Milkv-Jupiter repeated board：production public median / mean 1.06x。 | 当前有界生产补丁值得保留。 |
| boundary（边界） | gate、fallback、row source 和 method 边界已写入 phase result、evaluation 和本文件。 | 结论只覆盖 map-prep 前缀和当前 production public bench 范围。 |

QEMU timing（仿真器计时）没有性能意义，本 topic 的性能结论只使用板卡 repeated 数据。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `computeFeature()` | production public-derived entry | 构建 depth-change map / distance map，并分派 full / part 输出。 | `Feature::compute()` | `computeFeatureFull()` / `computeFeaturePart()` | adopted production dispatch boundary | `features/include/pcl/features/impl/integral_image_normal.hpp` |
| `initializeMapPrepStd` | production Std helper | 保留原 map-prep 标量语义。 | `computeFeature()` | distance transform | fallback semantic baseline | `features/include/pcl/features/impl/integral_image_normal.hpp` |
| `tryInitializeMapPrepRVV` | production RVV helper | RVV 初始化 depth-change map 和 distance map。 | `computeFeature()` | distance transform | adopted production implementation | `features/include/pcl/features/impl/integral_image_normal.hpp` |
| `src/test_integral_image_normal.cpp` | correctness tests | 对拍 helper 和 public `compute()` distance map。 | `run_test_compare` | gtest | correctness gate | `test-rvv/features/integral_image_normal/src/test_integral_image_normal.cpp` |
| `src/bench_integral_image_normal.cpp` | bench wrapper | 生产 direct、diagnostic 和 production-shaped profile case。 | board targets | analyze logs | board performance | `test-rvv/features/integral_image_normal/src/bench_integral_image_normal.cpp` |
| `generate_integral_image_normal_evidence_manifest.py` | analysis script | 生成 Evidence Doctor manifest。 | board repeated logs | `evidence_doctor.py` | evidence validation | `test-rvv/features/integral_image_normal/script/generate_integral_image_normal_evidence_manifest.py` |
| Phase 050 result | phase closeout | PI5 事实、命令和 EvidenceDecision 主归属。 | reviewer / worker | evaluation / this doc | recovery pointer | `test-rvv/features/integral_image_normal/doc/phases/050-map-prep-production-probe/result.zh.md` |
| evaluation | decision audit | 候选取舍、fallback、doc ownership 和 Traceability Map 主归属。 | reviewer / worker | this doc | production decision audit | `test-rvv/features/integral_image_normal/doc/integral_image_normal-evaluation.zh.md` |

## 生产 closeout

| area | 当前状态 |
| --- | --- |
| production file | `features/include/pcl/features/impl/integral_image_normal.hpp` |
| public API | 不变。 |
| RVV compile gate | `__RVV10__`。 |
| layout gate | `pcl::rvv::RVVXYZAoSFloatLayout<PointInT>`。 |
| adopted helper | `tryInitializeMapPrepRVV` for map-prep prefix。 |
| fallback | RVV helper 返回 false 或非 RVV 构建时执行 `initializeMapPrepStd`。 |
| test scope | `PointXYZ -> Normal`, organized full image, `AVERAGE_DEPTH_CHANGE`, synthetic depth scenes。 |
| evidence | QEMU 5/5 each side, asm attribution, board repeated, Evidence Doctor 3/21/8 with production direct warnings only。 |
| rollback boundary | 删除 RVV include、detail helper 和 `computeFeature()` 中的 RVV short-circuit 即可回到原标量行为；不涉及公开 API。 |

## 后续方向

当前 topic 内没有建议立即继续扩大 production patch 的方向：

- average 3D gradient diff-buffer 已有多阶段证据，完整链路弱 / 不稳定，不建议接 production。
- distance transform 有方向传播依赖，直接 RVV 化需要算法证明、correctness corpus 和新的 board ablation。
- normal solver / output path 混合积分图查询、Eigen 解算、border/mirror policy 和 indices 语义，应另开专项 profile phase。
- point type / method 扩展可作为后续验证方向，但需要独立 correctness、bench、asm、board 和 Evidence Doctor。
