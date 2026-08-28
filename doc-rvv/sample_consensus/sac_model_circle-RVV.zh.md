# SampleConsensusModelCircle2D RVV 生产实现

## 当前生产状态

`SampleConsensusModelCircle2D<PointT>` 当前已采纳三条 production RVV（生产 RVV）路径：

- `selectWithinDistance` 在 RVV 构建下通过 x/y gather（离散加载）、shell mask（圆壳掩码）和 `vcompress` 保序写回 inlier index，并在 compressed lanes（压缩后的有效向量通道）上用 `vfsqrt + vfwcvt + vse64` 写回误差距离。
- `countWithinDistance` 保留既有 RVV 计数路径，并补齐 signed 32-bit `pcl::index_t` 和 u32 byte offset（32 位字节偏移）gate。
- `getDistancesToModel` 已采纳 Phase 060 接入后的 full-RVV（完整 RVV）`vfsqrt + vfwcvt + vse64` 路径：RVV 批量计算平方距离和平方根，转换为 double 后直接写 `distances`。

Phase 020 的测试专用 `RVV sqr distance + scalar sqrt + dense double store` 旧候选仍为已拒绝实现族。Phase 040 尝试的 identity-index strided load（恒等索引跨步加载）也已被 RVV-vs-RVV（两个 RVV 实现族直接比较）strict A/B 证据拒绝，当前 select/count production 继续使用 direct indexed gather RVV。

## 函数语义和标量路径

2D 圆模型使用三个 `Eigen::VectorXf` 系数：圆心 `(a, b)` 和半径 `r`。三条距离入口都遍历 `indices_` 指向的输入点，只读取点的 `x/y` 字段：

| public entry | 输出语义 | 当前 production 状态 |
| --- | --- | --- |
| `selectWithinDistance` | 判断点到圆心的平方距离是否落在 `[max(0, r-threshold)^2, (r+threshold)^2]`，命中时按 `indices_` 顺序写 `inliers`，并写 `abs(sqrt(sqr_dist)-r)` 到 `error_sqr_dists_`。 | RVV adopted |
| `countWithinDistance` | 使用同一圆壳判断，只累计内点数量。 | RVV adopted |
| `getDistancesToModel` | 为每个 index 输出 `abs(sqrt((x-a)^2+(y-b)^2)-r)`。 | RVV adopted |

`selectWithinDistanceStandard` 和 `getDistancesToModelStandard` 保存原标量主体，用于 fallback（回退路径）和测试对拍。`selectWithinDistanceRVV` 和 `getDistancesToModelRVV` 是当前已采纳的 production RVV helper。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| `selectWithinDistance` gather + `vcompress` + full-RVV error tail | adopted | 公开入口的真实 row source（行来源）是 `indices_`；RVV 批量完成 x/y gather、平方距离、圆壳判断、命中 index 压缩写回和误差距离写回。 | Phase 090 接入后 public median 2.5700x，Evidence Doctor 0/0/0。 | 板卡性能只覆盖 `PointXYZ` / registered single-float x-y AoS。Phase 000 的 scalar error tail 旧实现只作为 historical baseline。 |
| `countWithinDistance` gather + mask popcount | adopted | 计数入口只需要 mask 真值数量，`vcpop` 可替代标量逐点累加。 | Phase 000 board median 1.4012x，Evidence Doctor 0/0/0。 | 同 select 的 layout / index / offset gate。 |
| `getDistancesToModel` RVV sqr + scalar sqrt/store | rejected | 每个点都必须写 `double` 距离，当前候选仍有临时 buffer、逐 lane 标量 `sqrt` 和 dense store 成本。 | Phase 020 candidate B/A median 0.6590x，Evidence Doctor Errors=1 / Warnings=1。 | 只有新的 RVV sqrt 或 store 组织能解释并消除该成本时恢复。 |
| `getDistancesToModel` full-RVV production | adopted | Phase 050/060 改为在 RVV chunk 中执行 `vfsqrt`、abs、`vfwcvt` 和 `vse64`，消除旧候选的标量 sqrt/store 后处理。 | Phase 060 接入后 public median 1.4737x，Evidence Doctor 0/0/0。 | 当前采纳范围只覆盖 `PointXYZ` board、direct indexed `indices_` 和 float x/y AoS。 |
| identity-index strided load | rejected | 恒等索引 case 仍慢于 gather-only RVV，说明 chunk 级检测和额外实现形态没有换来收益。 | Phase 040 select median 0.9914x，count median 0.9698x，Evidence Doctor Errors=2。 | 当前不保留 identity fast path。 |

## VL Chunk 流程

`selectWithinDistanceRVV` 每次用 `vsetvl` 选择当前 VL chunk（可变向量长度分块）：

1. 从 `indices_` 加载一段 index，并转换成 `index * sizeof(PointT)` 的 32-bit byte offset。
2. 使用 `pcl::rvv_load::indexed_load_field_f32m2` 按当前 `PointT` 的 traits offset 读取 `x/y`。
3. 广播圆心，调用 `sqr_distRVV_f32m2` 计算 `(x-a)^2 + (y-b)^2`。
4. 用 `sqr_dist >= inner` 和 `sqr_dist <= outer` 生成圆壳 mask。
5. 使用 `vcompress` 保序压缩命中的 index 和平方距离，把 index 写入预分配 `inliers`。
6. 对压缩后的平方距离继续执行 `vfsqrt.v`、abs、`vfwcvt.f.f.v`，再用 `vse64.v` 写 `error_sqr_dists_`。

`countWithinDistanceRVV` 复用同一 gather、平方距离和 mask 逻辑，最后用 `vcpop` 统计有效 lane（向量通道）数量。当前实现没有 identity strided-load 分支；identity 输入也走 direct indexed gather RVV。

`getDistancesToModelRVV` 同样按 `indices_` 做 x/y gather，RVV 计算 `(x-a)^2 + (y-b)^2`，再用 `vfsqrt.v` 得到点到圆心距离。随后用 `vfsgnjx` 取 `abs(distance-r)`，通过 `vfwcvt.f.f.v` 扩成 double，并用 `vse64.v` 直接写入 `std::vector<double>`。这条路径没有 Phase 020 旧候选中的逐 lane 标量 `sqrt` 和临时 float buffer 后处理。

## 覆盖范围与 Fallback

| 条件 | 当前行为 |
| --- | --- |
| 非 `__RVV10__` 构建 | 使用 Standard / SSE / AVX 既有路径。 |
| `pcl::rvv::RVVFloatFieldLayout<PointT, pcl::fields::x/y>::value == false` | public entry 回退 `selectWithinDistanceStandard` / `countWithinDistanceStandard` 或既有 SIMD。 |
| `sizeof(pcl::index_t) != sizeof(std::int32_t)` 或非 signed index | 不进入 RVV helper，回退标量。 |
| 点云规模超过 `rvvMaxU32ByteOffsetElements<PointT>()` | 避免 32-bit byte offset 溢出，回退标量。 |
| shuffled、identity 或其它 direct indexed `indices_` | select/count 均使用 direct indexed gather RVV；不区分 identity fast path。 |
| `getDistancesToModel` | 命中 gate 时使用 `getDistancesToModelRVV`；非 RVV 构建、字段 / index / offset gate 不满足时使用 `getDistancesToModelStandard`。 |
| 非 RVV 可证 layout、自定义非 float x/y 点型、`Scalar=double` | 不在当前采纳范围内。 |

当前 correctness（正确性）覆盖 `PointXYZ` 和 `PointXYZI`；板卡性能只覆盖 `PointXYZ`。不能把这些结果外推到所有 PointXYZ-like 点型、自定义点型或其它硬件。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- |
| `selectWithinDistance` | production public entry | 做模型有效性检查并按 RVV gate 分流。 | production boundary | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` |
| `selectWithinDistanceStandard` | production Std helper | 保存原标量 fallback 语义。 | fallback coverage | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` |
| `selectWithinDistanceRVV` | production RVV helper | 执行 indexed x/y gather、圆壳判断、`vcompress` 写回 inlier 和 full-RVV error tail。 | Phase 090 adopted production direct / asm attribution | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` |
| `countWithinDistanceRVV` | production RVV helper | 执行 indexed x/y gather、圆壳判断和 `vcpop` 计数。 | production direct / regression | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` |
| `getDistancesToModelCandidateRVV` | test-only diagnostic | 测试当前 getDistances 候选形态，不进入 production。 | diagnostic rejection | `test-rvv/sample_consensus/sac_model_circle/src/test_sac_model_circle.cpp`、`src/bench_sac_model_circle.cpp` |
| `getDistancesToModelRVV` | production RVV helper | 执行 full-RVV getDistances dense double 输出。 | adopted production direct / asm attribution | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` |
| `bench_sac_model_circle` | bench wrapper | 计时 public select/count/getDistances 和 test-only candidate rows。 | board performance input | `test-rvv/sample_consensus/sac_model_circle/src/bench_sac_model_circle.cpp` |
| `generate_circle_board_evidence_manifest.py` | analysis script | 生成 Phase 000 / 020 / 040 / 050 / 060 / 080 / 090 manifest。 | evidence summary | `test-rvv/sample_consensus/sac_model_circle/script/generate_circle_board_evidence_manifest.py` |
| Phase 000 manifest / doctor | evidence output summary | 保存第一版 select scalar error tail 和 count production board 数据。 | historical baseline / count production evidence | `test-rvv/sample_consensus/sac_model_circle/doc/phases/000-circle-select-distance-production/` |
| Phase 050 manifest / doctor | evidence output summary | 保存 full-RVV getDistances 测试专用候选数据。 | diagnostic evidence | `test-rvv/sample_consensus/sac_model_circle/doc/phases/050-getdistances-vfsqrt-full-rvv/` |
| Phase 060 manifest / doctor | evidence output summary | 保存接入后 public getDistances production board 数据。 | adopted production evidence | `test-rvv/sample_consensus/sac_model_circle/doc/phases/060-getdistances-production-probe/` |
| Phase 090 manifest / doctor | evidence output summary | 保存接入后 public select full-RVV error tail production board 数据。 | adopted production evidence | `test-rvv/sample_consensus/sac_model_circle/doc/phases/090-select-error-tail-production-probe/` |
| Phase 040 manifest / doctor | evidence output summary | 保存 identity 实现族负向 RVV-vs-RVV A/B 数据。 | rejected family evidence | `test-rvv/sample_consensus/sac_model_circle/doc/phases/040-production-closeout-and-identity-frontier/` |

## 数值算例

设圆心 `(a,b)=(0.25,-0.20)`、半径 `r=1.00`、阈值 `0.08`。点 `(1.25,-0.20)` 的平方距离为：

```text
sqr_dist = (1.25 - 0.25)^2 + (-0.20 + 0.20)^2 = 1.0
inner = (1.00 - 0.08)^2 = 0.8464
outer = (1.00 + 0.08)^2 = 1.1664
```

因为 `0.8464 <= 1.0 <= 1.1664`，该点进入 `inliers`。误差距离为 `abs(sqrt(1.0)-1.0)=0.0`。RVV helper 批量计算 `sqr_dist` 和 mask；Phase 090 后，命中点的误差也在 RVV 中由 `vfsqrt`、abs、`vfwcvt` 和 `vse64` 写入。

## Bench 与证据

当前 production 数据来自接入后的板卡 repeated 结果：

```bash
SSH_AUTH_SOCK=/run/user/1001/gcr/.ssh \
  make -C test-rvv/sample_consensus/sac_model_circle collect_select_error_tail_production_repeated_board_evidence
make -C test-rvv/sample_consensus/sac_model_circle record_select_error_tail_production_board_evidence_state
```

| 证据 | 结果 | 说明 |
| --- | --- | --- |
| QEMU correctness | Std/RVV gtest 通过。 | QEMU 只证明 correctness 和路径，不证明性能。 |
| production `selectWithinDistance` board | Phase 090 接入后 median 2.5700x，min/max 2.5357x / 2.6770x。 | 支撑当前 adopted select full-RVV error tail。 |
| production `countWithinDistance` board | median 1.4012x，min/max 1.3898x / 1.4216x。 | 支撑当前 adopted count RVV。 |
| Phase 060 production `getDistancesToModel` board | median 1.4737x，min/max 1.4687x / 1.4962x。 | 支撑当前 adopted getDistances full-RVV。 |
| `selectWithinDistanceRVV` asm | Phase 090 asm gate 确认 `vcompress.vm`、`vfsqrt.v`、`vfwcvt.f.f.v` 和 `vse64.v`，manifest 统计 24 条 RVV 指令。 | helper 符号级归属闭合。 |
| `countWithinDistanceRVV` asm | Phase 000 manifest 统计 15 条 RVV 指令。 | helper 符号级归属闭合。 |
| `getDistancesToModelRVV` asm | Phase 060 asm gate 确认 `vfsqrt.v`、`vfwcvt.f.f.v` 和 `vse64.v`。 | helper 符号级归属闭合。 |
| Evidence Doctor | Phase 000、Phase 060 与 Phase 090 均为 Errors=0，Warnings=0，Suggestions=0。 | 无阻塞异常。 |

Phase 040 identity A/B 不是 production adoption 证据，而是实现族拒绝证据：identity candidate 的 select median 0.9914x、count median 0.9698x，两个 row 都 5/5 低于 1。

## 正确性与高效性证据链

- correctness：`run_test_compare` 覆盖 public select/count/getDistances、Standard helper、RVV helper、`PointXYZI` correctness、identity indices 和显式空 indices。
- path / asm：Phase 090 确认 `selectWithinDistanceRVV` 命中 `vcompress.vm`、`vfsqrt.v`、`vfwcvt.f.f.v` 和 `vse64.v`；Phase 000 仍证明 `countWithinDistanceRVV` 的符号级 RVV 指令归属；Phase 060 确认 `getDistancesToModelRVV` 命中 `vfsqrt.v`、`vfwcvt.f.f.v` 和 `vse64.v`。
- performance：性能结论只来自 `Milkv-Jupiter` 板卡 5-run repeated board，QEMU timing 不作为性能证据。
- boundary：当前 EvidenceDecision 只覆盖 `PointXYZ` board case、direct indexed `indices_`、registered single-float x/y AoS、signed 32-bit index 和 u32 byte offset 范围。
- risk：更多 PointXYZ-like 点型 dedicated board、`Scalar=double`、自定义 layout 和其它模型入口没有被本 topic closeout 采纳。`selectWithinDistance` 的标量 error tail 已由 Phase 090 关闭；新的优化方向需要新的 load 组织、点型扩展或其它硬件证据。

## Production Closeout

| 文件 | 改动 | 回滚边界 |
| --- | --- | --- |
| `sample_consensus/include/pcl/sample_consensus/sac_model_circle.h` | 新增 `selectWithinDistanceStandard` / `selectWithinDistanceRVV`、`getDistancesToModelStandard` / `getDistancesToModelRVV` 声明；保留 `countWithinDistanceRVV`。 | 若以后撤销 getDistances，只回滚 getDistances 两个 helper 声明和 public dispatch；select/count 仍保留。 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` | 抽出 select/getDistances Standard helper，新增 select RVV helper 和 getDistances RVV helper；public select/count/getDistances 增加 RVV gate 和 u32 byte offset 检查；Phase 090 把 select 命中点误差写回改成 RVV `vfsqrt + vfwcvt + vse64`。 | 若以后撤销 Phase 090，只把 select 误差写回恢复为 Phase 000 scalar tail；getDistances 和 count 可独立保留。 |
| `test-rvv/sample_consensus/sac_model_circle/**` | 新增 correctness、bench、board、manifest、Evidence Doctor、registry 和 topic-local 文档。 | 测试资产可保留为回归和审查证据。 |

## 后续方向

当前 topic 已在 Phase 090 完成 `selectWithinDistance` full-RVV error tail production closeout，并在 Phase 070 完成 `getDistancesToModel` full-RVV production closeout。当前 scope 内没有新的同边界高优先级性能候选；更多点型 dedicated board、`Scalar=double`、新的 identity/load 组织或 `circle3d` 投影核属于新 scope，不由本文的 adopted 结论外推。
