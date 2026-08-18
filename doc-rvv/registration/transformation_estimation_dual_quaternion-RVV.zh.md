# registration/transformation_estimation_dual_quaternion RVV production 说明

## 当前状态

`TransformationEstimationDualQuaternion::estimateRigidTransformation` 当前结论是：

```text
production-adopted/ordered-source-indexed-dual-indexed-c1-c2-f32-xyz-aos-rvv-correspondence-scalar
```

已接入 production（生产源码）的范围有三条 public overload（公开入口）：

- `ordered-cloud-pair`：source 和 target 按相同下标一一对应。`Scalar=float`、两侧 dense、两侧点型满足 `RVVXYZAoSFloatLayout`、点数不少于 32 时，进入 C1/C2 accumulation（累加）RVV 分流；其它情况回到原 `ConstCloudIterator` 标量路径。
- `source-indexed-cloud-pair`：source 由 `indices_src` 指定，target 顺序扫描。`Scalar=float`、两侧 dense、两侧点型满足 xyz f32 AoS layout、`pcl::index_t` 为 32-bit、source indices 全部合法并且 byte offset 可由当前 RVV gather 表示时，进入 direct gather（直接离散加载）RVV 分流；失败时回到原标量路径。
- `dual-indexed-cloud-pair`：source 和 target 都由 indices 指定。它使用两路 direct gather RVV 分流，gate 与 source-indexed 相同，但 source / target 两侧 indices 都必须合法。

当前不覆盖：

- `correspondence-pair` production RVV。该公开入口继续使用 `ConstCloudIterator` 标量路径。
- `Scalar=double`。
- 非 dense cloud、小规模输入和不满足 xyz f32 AoS layout 的点型。
- 无法用当前 32-bit byte offset gather 表示的超大 cloud 或非法 indices。
- 真实 workload 的 correspondence index-distribution（对应关系索引分布）外推。

稳定证据索引：

| 证据 | 路径 | 用途 |
| --- | --- | --- |
| production retained board summary | `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/production_public_retained_row_sources_repeated/summary.md` | ordered-cloud-pair 真实 public Std/RVV repeated 性能主证据。 |
| source-indexed board summary | `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/production_public_source_indexed_cloud_pair_repeated/summary.md` | source-indexed 真实 public Std/RVV repeated 性能证据。 |
| dual-indexed board summary | `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/production_public_dual_indexed_cloud_pair_repeated/summary.md` | dual-indexed 真实 public Std/RVV repeated 性能证据。 |
| QEMU smoke manifest / doctor | `test-rvv/registration/transformation_estimation_dual_quaternion/log/qemu/evidence_manifest.json`；`test-rvv/registration/transformation_estimation_dual_quaternion/log/qemu/evidence_doctor.md` | retained production bench 的日志形状和 checksum smoke；不作为性能结论。 |
| correctness logs | `test-rvv/registration/transformation_estimation_dual_quaternion/log/qemu/run_test_std.log`；`test-rvv/registration/transformation_estimation_dual_quaternion/log/qemu/run_test_rvv.log` | `make run_test_compare` 生成；Std `28/28`，RVV `32/32`。 |
| asm dump | `test-rvv/registration/transformation_estimation_dual_quaternion/build/asm/riscv/bench_transformation_estimation_dual_quaternion_rvv.asm` | 本地反汇编输入；确认 retained bench binary 中出现 RVV load/gather/widen/reduction 指令。 |
| evaluation 文档 | `test-rvv/registration/transformation_estimation_dual_quaternion/doc/transformation_estimation_dual_quaternion-evaluation.zh.md` | EvidenceDecision、Traceability Map 和 correspondence 撤下理由主归属。 |
| optimization evidence 文档 | `test-rvv/registration/transformation_estimation_dual_quaternion/doc/optimization-evidence.zh.md` | adopted / removed / deferred candidate 到代码、target、board 和证据边界的索引。 |
| topic README | `test-rvv/registration/transformation_estimation_dual_quaternion/README.zh.md` | 文档导航、常用命令和提交证据边界入口。 |
| Phase 016 result | `test-rvv/registration/transformation_estimation_dual_quaternion/doc/phases/016-production-adoption-scope-finalization/result.zh.md` | 三类 retained、correspondence 回退标量的 production closeout 记录。 |

QEMU correctness（QEMU 正确性验证）用于构建、路径和日志形状，不作为性能结论。性能结论只引用 repeated board（重复板卡）或目标硬件结果。

## 函数语义

该类实现 dual quaternion（双四元数）刚体变换估计。公开入口把 source / target 点对交给内部 helper，helper 对每个点对累加两个 4x4 double 矩阵 `C1` 和 `C2`。点对全部处理后，代码填充矩阵对称项，缩放 `C1/C2`，构造 4x4 矩阵 `A`，调用 Eigen 求最大特征向量，再由 quaternion（四元数）生成 rotation 和 translation，最后写入 4x4 transformation matrix。

公开入口的 row source policy（行来源策略）是第一层语义轴：

| 公开入口 | 点对来源 | 当前 production RVV |
| --- | --- | --- |
| `estimateRigidTransformation(cloud_src, cloud_tgt, matrix)` | `source[k] + target[k]` | adopted：ordered-cloud-pair C1/C2 RVV。 |
| `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, matrix)` | `source[indices_src[k]] + target[k]` | adopted：source-indexed direct gather RVV。 |
| `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, indices_tgt, matrix)` | `source[indices_src[k]] + target[indices_tgt[k]]` | adopted：dual-indexed direct gather RVV。 |
| `estimateRigidTransformation(cloud_src, cloud_tgt, correspondences, matrix)` | `source[correspondence.index_query] + target[correspondence.index_match]` | scalar-only：保持原 iterator 标量路径。 |

标量 C1/C2 逐点公式只使用 source / target 的 `x/y/z`。对一个点对 `a=(ax, ay, az)`、`b=(bx, by, bz)`，标量路径先形成乘积：

```text
axbx = ax * bx
ayby = ay * by
azbz = az * bz
axby = ax * by
aybx = ay * bx
axbz = ax * bz
azbx = az * bx
aybz = ay * bz
azby = az * by
```

然后累加 C1 上三角和 C2 的非零项：

```text
C1[0]  += axbx - azbz - ayby
C1[5]  += ayby - azbz - axbx
C1[10] += azbz - axbx - ayby
C1[15] += axbx + ayby + azbz
C1[1]  += axby + aybx
C1[2]  += axbz + azbx
C1[3]  += aybz - azby
C1[6]  += azby + aybz
C1[7]  += azbx - axbz
C1[11] += axby - aybx

C2[1]  += az + bz
C2[2]  -= ay + by
C2[3]  += ax - bx
C2[6]  += ax + bx
C2[7]  += ay - by
C2[11] += az - bz
```

RVV 只接管这一段 C1/C2 前置累加。矩阵对称补齐、Eigen 4x4 solve、quaternion 到 transformation matrix 的后段仍复用原标量逻辑。

## 当前采用的优化方式

### Dispatch 与 fallback

三条 retained public overload 都在原有数量检查之后、构造 `ConstCloudIterator` 之前尝试 RVV helper。helper 返回 `true` 表示已经写出 `transformation_matrix`，公开入口直接返回；helper 返回 `false` 时，公开入口继续走原来的 iterator 标量路径。

生产代码路径是：

```text
ordered public overload
  -> detail::estimateRigidTransformationDualQuaternionOrderedCloudPairRVV(...)
  -> detail::accumulateTransformationEstimationDualQuaternionRVV(...)
  -> detail::finishTransformationEstimationDualQuaternion(...)

source-indexed public overload
  -> detail::estimateRigidTransformationDualQuaternionSourceIndexedCloudPairRVV(...)
  -> detail::accumulateTransformationEstimationDualQuaternionRVV(...)
  -> detail::finishTransformationEstimationDualQuaternion(...)

dual-indexed public overload
  -> detail::estimateRigidTransformationDualQuaternionDualIndexedCloudPairRVV(...)
  -> detail::accumulateTransformationEstimationDualQuaternionRVV(...)
  -> detail::finishTransformationEstimationDualQuaternion(...)
```

`correspondence-pair` public overload 没有 RVV dispatch。它仍构造 source / target `ConstCloudIterator`，再调用同一个标量 helper。

### Layout gate

当前 RVV helper 使用 `RVVXYZAoSFloatLayout<PointT>` 验收 source 和 target。这个 gate 证明：

- `x/y/z` 是 PCL traits 可定位的单个 `float` 字段。
- 点型是当前 RVV stride/gather load 可以按 AoS（结构数组）读取的布局。
- source 和 target 可以分别使用自己的 `sizeof(PointT)`、字段 offset 和 base pointer。

这个 gate 不证明所有额外字段参与语义。`PointXYZI` 的 intensity、`PointXYZRGB` 的颜色字段都不是 dual quaternion 数学输入；它们只证明“带额外字段但 xyz offset/stride 合法”的布局能被正确跳过。

### Ordered stride load

ordered-cloud-pair 的 loader 从 `cloud.points.data()` 取 base pointer。每个 VL chunk（可变向量长度分块）从第 `i` 行开始，用 `vlse32.v` 按 `sizeof(PointT)` 跨步读取 `x/y/z`：

```text
vsetvl_e32mf2(n - i)
  -> source x/y/z: vlse32.v(base_src + xyz_offset, sizeof(PointSource))
  -> target x/y/z: vlse32.v(base_tgt + xyz_offset, sizeof(PointTarget))
  -> C1/C2 per-lane formula
  -> widen f32 result to f64
  -> vector partial sums
```

ordered 路径没有 staging（暂存）和 scatter（离散写回）；它只改变 C1/C2 累加的执行方式。

### Indexed direct gather

source-indexed 和 dual-indexed 使用 direct gather。helper 先在标量 gate 中确认 indices 非负、在 cloud 范围内，并且 `cloud.size()` 不超过 `rvvMaxU32ByteOffsetElements<PointT>()`。通过后，RVV chunk 直接读取 32-bit `pcl::index_t`，把 index 乘以 `sizeof(PointT)` 得到 byte offsets，再用 `vluxei32.v` 从 xyz 字段离散加载。

source-indexed 的单个 chunk：

```text
vsetvl_e32mf2(n - i)
  -> source index: vle32.v indices_src[i..i+vl)
  -> source byte offsets: index * sizeof(PointSource)
  -> source x/y/z: vluxei32.v(base_src + xyz_offset, byte_offsets)
  -> target x/y/z: vlse32.v(base_tgt + xyz_offset, sizeof(PointTarget))
  -> C1/C2 per-lane formula
  -> widen to f64 and accumulate
```

dual-indexed 的单个 chunk 在 source 和 target 两侧都执行 `vle32.v -> byte offset -> vluxei32.v`。它没有把 indexed rows 先 materialize 成临时 ordered cloud，因此当前文档称为 direct gather，而不是 staged gather。

### Reduction 与后段

`accumulateTransformationEstimationDualQuaternionRVV` 在 vector partial sums 中维护 C1/C2 需要的 16 个非零累加项。每个 chunk 先在 f32 lane 中计算乘法、加法或减法，再 widen（拓宽）到 f64 partial sum。循环结束后，helper 用 `vfredosum.vs` 横向规约成 double 标量，填入 `TransformationEstimationDualQuaternionAccumulation`。

`finishTransformationEstimationDualQuaternion` 接收 RVV 累加结果后继续执行原标量后段：

```text
fill symmetric C1/C2 entries
  -> C1 *= -2
  -> C2 *= 2
  -> A = (0.25 / count) * C2^T * C2 - C1
  -> Eigen self-adjoint eigen solve
  -> q / s quaternion
  -> rotation and translation
```

规约树和标量 row-order double 累加不同，因此测试使用 matrix 误差预算，不要求 bitwise exact（逐位相同）。

## 范围决策表

| 方向 | 状态 | 证据 / 理由 | 边界 / 下一步 |
| --- | --- | --- | --- |
| ordered-cloud-pair production C1/C2 RVV | adopted | `make run_test_compare` 通过；retained board median `3.232x / 3.644x / 3.656x`；doctor `0/0/0`。 | 只覆盖 `Scalar=float`、dense、xyz f32 AoS、点数不少于 32。 |
| source-indexed-cloud-pair direct gather RVV | adopted | public path-hit 和 fallback tests 通过；board median `2.540x / 2.631x / 2.586x`；doctor `0/0/0`。 | 只覆盖合法 32-bit source indices；其它情况 fallback。 |
| dual-indexed-cloud-pair direct gather RVV | adopted | public path-hit 和 fallback tests 通过；board median `2.015x / 1.808x / 2.021x`；doctor `0/0/0`。 | source / target indices 都必须合法并适合 32-bit byte offset gather。 |
| correspondence-pair production RVV | rejected / scalar-only | Phase 015 public correspondence probe 在 64K / 256K negative，doctor `2/3/0`；Phase 016 已移除 production dispatch。 | 后续若重开，需要真实 workload distribution 或新的 bounded production probe。 |
| C1/C2 accumulation-only component | diagnostic positive | test support component ablation 说明前端累加有收益。 | 只解释热点，不替代 production direct。 |
| staged source / dual / correspondence diagnostics | historical diagnostic | 用于比较 row-source policy 和 staging 成本。 | 不作为当前 production 结论。 |
| correspondence segment-load / locality candidates | rejected with evidence | segment-load negative / unstable，local-window negative。 | 保留为 correspondence 专项的历史输入。 |
| point type layout diagnostics | diagnostic support | `PointXYZI` / `PointXYZRGB` correctness 和部分 board 诊断。 | 不外推为所有点型逐类型性能结论。 |
| `Scalar=double` | deferred / fallback | RVV helper 只在 `Scalar=float` 编译分支继续。 | 若要支持 double，需另开数值和性能证据。 |

## 标量路径与 RVV 路径差异

| 阶段 | 标量实现 | 当前 RVV 实现 | 对应测试 / 证据 |
| --- | --- | --- | --- |
| public 输入数量检查 | ordered 检查 source/target size；source-indexed 检查 `indices_src.size()==cloud_tgt.size()`；dual-indexed 检查两侧 indices size。 | 保留原检查，检查失败时不尝试 RVV。 | public entry correctness tests。 |
| row source 展开 | 所有入口最终交给 `ConstCloudIterator` 同步前进。 | 三类 retained 入口在 iterator 前尝试显式 RVV loader；correspondence 仍只走 iterator。 | production path-hit tests；Traceability Map。 |
| load | 每行通过 iterator 读取 `a.x/y/z` 和 `b.x/y/z`。 | ordered 用 `vlse32.v`；source-indexed 用 source `vluxei32.v` + target `vlse32.v`；dual-indexed 两侧 `vluxei32.v`。 | asm dump；board production summaries。 |
| C1/C2 公式 | 每点 double 乘法和累加，按 row order 更新 `C1/C2`。 | lane 内先 f32 运算，再 widen 到 f64 partial sum，最后 `vfredosum.vs` 到 double。 | `run_test_compare`；matrix tolerance。 |
| solve / output | Eigen 4x4 solve，构造 quaternion、rotation、translation。 | 完全复用同一后段。 | public matrix equality tests。 |
| fallback | 非覆盖输入自然使用原标量 helper。 | helper 返回 `false` 后继续原标量 helper；非 RVV 构建没有 RVV helper。 | `ProductionRVVFallbackGatesRejectOutOfScope`。 |

## 数值算例

单个点对：

```text
source a = (1, 2, 3)
target b = (4, 6, 8)

axbx = 4
ayby = 12
azbz = 24
axby = 6
aybx = 8
axbz = 8
azbx = 12
aybz = 16
azby = 18
```

该点对贡献：

```text
C1[0]  += 4 - 24 - 12 = -32
C1[5]  += 12 - 24 - 4 = -16
C1[10] += 24 - 4 - 12 = 8
C1[15] += 4 + 12 + 24 = 40
C1[1]  += 6 + 8 = 14
C1[2]  += 8 + 12 = 20
C1[3]  += 16 - 18 = -2
C1[6]  += 18 + 16 = 34
C1[7]  += 12 - 8 = 4
C1[11] += 6 - 8 = -2

C2[1]  += 3 + 8 = 11
C2[2]  -= 2 + 6 = -8
C2[3]  += 1 - 4 = -3
C2[6]  += 1 + 4 = 5
C2[7]  += 2 - 6 = -4
C2[11] += 3 - 8 = -5
```

如果一个 VL chunk 有 4 个 lane，ordered path 会同时读取 4 个 source 点和 4 个 target 点，分别计算上述同类贡献。`C1[0]` 的 4 个 lane 先留在一个 f64 vector partial sum 中；所有 chunk 扫完后，`vfredosum.vs` 把这些 lane 合成一个 double，写回 `acc.c1[0]`。这解释了为什么 RVV 与标量允许规约顺序差异，但矩阵输出仍必须在误差预算内。

## Bench 与性能证据

当前生产性能结论来自真实 public overload 的 repeated board summary。采集命令：

```bash
make run_board_bench_production_public_repeated
```

采集合同：

- device：`Milkv-Jupiter`
- case-filter：`production-public-retained-row-sources`
- repeated runs：`5`
- iterations：`20`
- warm-up iterations：`5`
- B/A：`Std public ms / RVV public ms`

| row source policy | 4K median | 64K median | 256K median | Evidence Doctor | 证据边界 |
| --- | ---: | ---: | ---: | --- | --- |
| `ordered-cloud-pair` | `3.232x` | `3.644x` | `3.656x` | `0/0/0` | 真实 ordered public overload，含 dispatch、C1/C2 和 Eigen solve。 |
| `source-indexed-cloud-pair` | `2.540x` | `2.631x` | `2.586x` | `0/0/0` | 真实 source-indexed public overload，含 index gate、gather、C1/C2 和 Eigen solve。 |
| `dual-indexed-cloud-pair` | `2.015x` | `1.808x` | `2.021x` | `0/0/0` | 真实 dual-indexed public overload，含两路 index gate、gather、C1/C2 和 Eigen solve。 |

QEMU smoke 使用：

```bash
make run_qemu_smoke_evidence_doctor
```

该 smoke 的 `--warmup-iterations 0` 会触发 9 个 `zero_warmup_iterations` warnings；Evidence Doctor 为 Errors=0，Warnings=9，Suggestions=0。它只证明 retained case-filter 可运行、日志可解析和 checksum 形状正常，不写入 speedup 结论。

`make dump_bench_rvv` 生成本地 asm dump。反汇编用于确认 retained bench binary 中有 `vlse32`、`vluxei32`、`vfwcvt`、`vfredosum` 等 RVV 指令；最终性能判断仍以 board repeated 为准。

## Fallback 矩阵

| 条件 | RVV 行为 | 语义保持方式 |
| --- | --- | --- |
| 非 RVV 构建 | 不编译 RVV helper。 | 公开入口保持原标量实现。 |
| `Scalar != float` | helper 在 `if constexpr` 中返回 `false`。 | 继续走 `ConstCloudIterator` 标量路径。 |
| source 或 target 不满足 `RVVXYZAoSFloatLayout` | helper 返回 `false`。 | 原 iterator 按点型 public 语义读取字段。 |
| `cloud.is_dense == false` | helper 返回 `false`。 | 原标量路径保持既有 dense / 非 dense 行为。 |
| 点数小于 32 | helper 返回 `false`。 | 小规模输入避免 RVV 启动成本，结果由标量生成。 |
| ordered source/target size 不一致 | 原 public 检查报错并返回。 | 不进入 RVV 或标量 helper。 |
| source-indexed `indices_src.size()!=cloud_tgt.size()` | 原 public 检查报错并返回。 | 不进入 RVV 或标量 helper。 |
| dual-indexed 两侧 indices size 不一致 | 原 public 检查报错并返回。 | 不进入 RVV 或标量 helper。 |
| indexed path 中存在负 index 或越界 index | helper 返回 `false`。 | 回到原 public iterator 边界。 |
| cloud 太大，index byte offset 超出当前 32-bit gather 表示范围 | helper 返回 `false`。 | 回到标量，避免 gather offset 截断风险。 |
| `correspondence-pair` public overload | 没有 RVV dispatch。 | 始终构造 correspondence iterator 并走标量 helper。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `estimateRigidTransformation(cloud_src, cloud_tgt, matrix)` | production public entry | ordered-cloud-pair dispatch。 | PCL public API。 | `estimateRigidTransformationDualQuaternionOrderedCloudPairRVV` 或标量 iterator helper。 | retained production direct。 | `registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp` |
| `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, matrix)` | production public entry | source-indexed dispatch。 | PCL public API。 | `estimateRigidTransformationDualQuaternionSourceIndexedCloudPairRVV` 或标量 iterator helper。 | retained production direct。 | 同上 |
| `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, indices_tgt, matrix)` | production public entry | dual-indexed dispatch。 | PCL public API。 | `estimateRigidTransformationDualQuaternionDualIndexedCloudPairRVV` 或标量 iterator helper。 | retained production direct。 | 同上 |
| `estimateRigidTransformation(cloud_src, cloud_tgt, correspondences, matrix)` | production public entry | correspondence 标量入口。 | PCL public API。 | `ConstCloudIterator` 标量 helper。 | production scalar boundary。 | 同上 |
| `accumulateTransformationEstimationDualQuaternionRVV` | production detail helper | 共享 C1/C2 RVV 累加。 | 三类 retained RVV helper。 | `finishTransformationEstimationDualQuaternion`。 | retained production implementation。 | 同上 |
| `TransformationEstimationDualQuaternionOrderedXYZLoader` | production detail helper | ordered stride load source / target xyz。 | ordered RVV helper，也可作为 target loader。 | C1/C2 accumulation。 | load path evidence。 | 同上 |
| `TransformationEstimationDualQuaternionIndexedXYZLoader` | production detail helper | indexed direct gather source / target xyz。 | source-indexed / dual-indexed RVV helper。 | C1/C2 accumulation。 | gather path evidence。 | 同上 |
| `transformationEstimationDualQuaternionIndicesFitRVVGather` | production gate | index range 和 32-bit byte offset gate。 | indexed RVV helpers。 | fallback decision。 | fallback coverage。 | 同上 |
| `src/test_tedq.cpp` | test support | correctness、path-hit、fallback、row-source 语义。 | `make run_test_compare`。 | QEMU logs。 | correctness gate。 | `test-rvv/registration/transformation_estimation_dual_quaternion/src/test_tedq.cpp` |
| `src/bench_tedq.cpp` | bench support | retained production public case-filter 和 historical probes。 | QEMU / board bench targets。 | summary scripts。 | performance evidence input。 | `test-rvv/registration/transformation_estimation_dual_quaternion/src/bench_tedq.cpp` |
| `script/generate_tedq_board_repeated_summary.py` | evidence script | repeated board summary / manifest 生成。 | board collect targets。 | Evidence Doctor。 | board summary input。 | `test-rvv/registration/transformation_estimation_dual_quaternion/script/generate_tedq_board_repeated_summary.py` |
| `doc/transformation_estimation_dual_quaternion-evaluation.zh.md` | topic-local doc | EvidenceDecision 和 closeout 主归属。 | reviewer / worker。 | 本文和 phase docs。 | decision audit。 | `test-rvv/registration/transformation_estimation_dual_quaternion/doc/transformation_estimation_dual_quaternion-evaluation.zh.md` |
| `doc/phases/016-production-adoption-scope-finalization/result.zh.md` | phase doc | retained scope finalization。 | phase loop recovery。 | README / evaluation / doc-rvv。 | PI5 closeout evidence。 | `test-rvv/registration/transformation_estimation_dual_quaternion/doc/phases/016-production-adoption-scope-finalization/result.zh.md` |

## 正确性与高效性证据链

| 证据层 | 当前结果 | 证明范围 | 不能证明 |
| --- | --- | --- | --- |
| correctness | `make run_test_compare`：Std `28/28`，RVV `32/32` | 三类 retained path-hit、fallback gate、row-source 语义和诊断候选对拍。 | 板卡性能、所有点型逐类型性能。 |
| QEMU smoke | `make run_qemu_smoke_evidence_doctor`：Errors=0，Warnings=9，Suggestions=0 | retained production bench 可运行、checksum / log shape 可解析。 | 任何性能结论；warnings 是 no-warmup smoke 边界。 |
| asm smoke | `make dump_bench_rvv` 已生成 RVV asm dump | retained bench binary 中存在 RVV stride load、gather、widen、reduction 指令。 | 单独证明不了 speedup，也不能替代符号级 profile。 |
| board performance | `make run_board_bench_production_public_repeated` | 三类 retained public entry 在 `Milkv-Jupiter` repeated board 上均 positive，doctor clean。 | correspondence、`Scalar=double`、非 dense、小规模、unsupported layout 或其它硬件。 |
| boundary | Phase 016 result 和 evaluation 已同步 | correspondence production RVV 被撤下，当前长期文档只覆盖三类 retained。 | 不能把历史 correspondence 诊断写成当前 adopted 行为。 |
| risk | fallback matrix 和范围决策表 | 未覆盖范围均保留标量或另开专项。 | 不证明后续扩展无需重新测试。 |

## 遗留风险与后续条件

- `correspondence-pair` 当前保持标量。若后续重开，需要先把 `local-window`、`contiguous`、`strided` 等 index pattern（索引分布模式）作为 correspondence 内部维度处理，而不是当成第五类 row source policy；再补 production direct path-hit、fallback、QEMU、asm、board repeated 和 Evidence Doctor。
- `Scalar=double` 未进入 RVV。double 路径需要重新设计 load、accumulation、误差预算和性能证据，不能继承 float 结论。
- 当前 production gate 是 xyz f32 AoS layout gate。`PointXYZI` / `PointXYZRGB` 等额外字段点型有 correctness / diagnostic 支撑，但长期性能结论仍按“layout gate + 当前 board case”解释，不能写成所有 gate-allowed 点型逐类型上板。
- 当前实现没有优化 Eigen 4x4 solve。若未来 C1/C2 前端不再是主成本，应通过 component ablation 或 profile 重新判断，不应继续在前端微调里消耗板卡预算。
- 若后续更改 `pcl::index_t` 宽度或 gather offset 表示方式，indexed helper 的 `static_assert`、range gate 和 fallback 测试必须重新审查。

## Production Closeout 表

| area | 当前状态 | 回滚 / 复核边界 |
| --- | --- | --- |
| production file | `registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp` 新增三类 retained RVV helper 和 dispatch；correspondence 无 RVV dispatch。 | 若取消接入，回滚该 header 的 RVV helper / dispatch，并删除或回滚本文。 |
| public API | 不新增 API，不改变参数或返回类型。 | public 数量检查和标量 fallback 必须保持。 |
| helper ownership | RVV helper 位于 header `detail` namespace，受 `__RVV10__` 保护。 | 非 RVV 构建不应看到 RVV intrinsic。 |
| tests | `src/test_tedq.cpp` 覆盖 correctness、path-hit、fallback 和 historical diagnostics。 | 提交前保留测试资产；raw logs 默认不提交。 |
| bench | `src/bench_tedq.cpp` 的 `production-public-retained-row-sources` 是当前 production 性能入口。 | historical correspondence filter 只作为恢复输入。 |
| evidence | 三类 retained board summary doctor clean；QEMU smoke doctor 无 Errors。 | 若重新跑改变 decision bucket，必须刷新 evaluation、phase result 和本文。 |
| doc suite | topic-local README、evaluation、optimization evidence、benchmark/evidence、phase 016 和本文已同步 retained-three 状态。 | 若用户取消接入，topic-local 文档改为 rollback/no-production closeout，`doc-rvv` 删除或标为 not_applicable。 |
