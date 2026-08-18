# transformation_estimation_dual_quaternion 函数级评估

## 范围和目标源码

目标源码是 `registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp`。
公开入口包含四类 row source policy（行来源策略）：`ordered-cloud-pair`、
`source-indexed-cloud-pair`、`dual-indexed-cloud-pair` 和 `correspondence-pair`。

当前 production RVV patch 只覆盖前三类。`correspondence-pair` 保持原
`ConstCloudIterator` 标量路径。

## S0 偏好冻结

| 字段 | 当前值 |
| --- | --- |
| `preferences_loaded` | `.agents/config/defaults.yaml` 已读取；`.agents/local/user-preferences.yaml` 不存在；prompt override 授权推进到提交前。 |
| `comment_policy_frozen` | test-rvv / diagnostic / prototype 使用详细中文注释；production 注释只解释 fallback、dispatch、数值风险和数据布局边界。 |
| `documentation_policy_frozen` | closeout current-state-first；长期 `doc-rvv` 只写 adopted / retained production behavior（保留的生产行为）。 |
| `evidence_policy_frozen` | summary-only；raw logs 不默认提交；QEMU 不支撑性能结论。 |
| `commit_policy_frozen` | 本轮不创建 commit，停在用户判断提交或取消接入。 |

## 函数级结论

当前 EvidenceDecision（证据决策）是 `ready_for_user_submit_or_cancel`。
Phase 016 后，生产源码中的 retained RVV 范围是：

- `ordered-cloud-pair`：dense `PointXYZ`-like xyz AoS、`Scalar=float`、点数不少于 32。
- `source-indexed-cloud-pair`：dense xyz AoS、`Scalar=float`、合法 32-bit `pcl::index_t` source indices。
- `dual-indexed-cloud-pair`：dense xyz AoS、`Scalar=float`、合法 32-bit source / target indices。

所有不满足 gate（验收条件）的输入回退到原标量 iterator helper；`correspondence-pair`
不进入 production RVV。

## 标量路径重建

公开入口先检查输入数量，再构造 `ConstCloudIterator`。核心 helper
`estimateRigidTransformation(ConstCloudIterator&, ConstCloudIterator&, Matrix4&)` 做三段事：

1. 初始化 `C1`、`C2` 两个 4x4 double 矩阵。
2. 对每个点对读取 `a.x/y/z` 和 `b.x/y/z`，累加 dual quaternion 的 C1/C2 项。
3. 填充对称项、缩放 `C1/C2`，构造 4x4 `A`，用 Eigen 求最大特征向量，再构造 rotation / translation 输出矩阵。

逐点 C1/C2 累加是 RVV 接管的热点；Eigen 4x4 solve 每次 estimate 只执行一次，保持标量。

## RVV 实现边界

RVV 路径用 `RVVXYZAoSFloatLayout` gate 确认 `x/y/z` 为 float AoS（结构数组）字段。
ordered 路径使用 `vlse32` strided load（跨步加载），indexed 路径使用 `vle32` 读取索引并用
`vluxei32` gather（离散加载）读取 source / target 字段。乘法先在 float lane（向量通道）计算，
再 widen（拓宽）到 double 累加，最后用 `vfredosum` 做规约。规约树与标量顺序不同，因此正确性使用矩阵误差预算，不要求 bitwise exact（逐位相同）。

## 当前证据链

| 证据层 | 当前结果 | 证明范围 |
| --- | --- | --- |
| Correctness | `make run_test_compare`：Std `28/28`，RVV `32/32` | 诊断候选、retained production path-hit 和 fallback gate 均通过。 |
| QEMU smoke | `make run_qemu_smoke_evidence_doctor`：Errors=0，Warnings=9，Suggestions=0 | 只证明 retained production bench 的日志形状和 checksum；warnings 为 no-warmup smoke 边界。 |
| ASM smoke | `make dump_bench_rvv` 已生成 RVV asm dump | 证明 retained bench binary 有 RVV 指令；不单独作为性能结论。 |
| Board performance | `Milkv-Jupiter` retained-only 5-run repeated | 三类 retained public entry 均 positive，Evidence Doctor clean。 |

Retained board medians：

| row source policy | 4K | 64K | 256K | Evidence Doctor |
| --- | ---: | ---: | ---: | --- |
| `ordered-cloud-pair` | `3.232x` | `3.644x` | `3.656x` | `0/0/0` |
| `source-indexed-cloud-pair` | `2.540x` | `2.631x` | `2.586x` | `0/0/0` |
| `dual-indexed-cloud-pair` | `2.015x` | `1.808x` | `2.021x` | `0/0/0` |

## Correspondence 边界

Phase 015 中 `correspondence-pair` production RVV 曾在 4K positive，但 64K / 256K negative，
Evidence Doctor 为 `2/3/0`。Phase 016 已移除该 production dispatch。test-rvv 中
correspondence direct index stream、segment-load、index-locality 和 point-type layout 诊断仍保留，
但这些诊断证据不能替代 production direct evidence（真实生产路径证据）。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 证据角色 |
| --- | --- | --- | --- |
| `estimateRigidTransformation(cloud_src, cloud_tgt, matrix)` | production public entry | ordered-cloud-pair RVV dispatch，失败回退标量 | retained production direct |
| `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, matrix)` | production public entry | source-indexed RVV dispatch，失败回退标量 | retained production direct |
| `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, indices_tgt, matrix)` | production public entry | dual-indexed RVV dispatch，失败回退标量 | retained production direct |
| `estimateRigidTransformation(cloud_src, cloud_tgt, correspondences, matrix)` | production public entry | correspondence 标量 iterator path | production scalar boundary |
| `accumulateTransformationEstimationDualQuaternionRVV` | production detail helper | 共享 C1/C2 RVV 累加 | retained production implementation |
| `src/test_tedq.cpp` | test support | correctness、path-hit、fallback 和 diagnostic 对拍 | QEMU correctness |
| `src/bench_tedq.cpp` | bench support | retained production public case-filter 和历史诊断 filters | QEMU smoke / board performance |
| `script/generate_tedq_board_repeated_summary.py` | evidence script | board repeated summary / manifest 生成 | Evidence Doctor 输入 |
| `log/board/production_public_retained_row_sources_repeated/summary.md` | evidence summary | ordered retained board 结果 | production direct |
| `log/board/production_public_source_indexed_cloud_pair_repeated/summary.md` | evidence summary | source-indexed retained board 结果 | production direct |
| `log/board/production_public_dual_indexed_cloud_pair_repeated/summary.md` | evidence summary | dual-indexed retained board 结果 | production direct |

## 不覆盖范围和风险

- `Scalar=double` 明确回退标量。
- 非 dense cloud、小规模输入、unsupported xyz layout、越界或无法用 32-bit byte offset 表示的 indices 回退标量。
- `PointXYZI` / `PointXYZRGB` 有 test-rvv layout 诊断，但当前 production performance 只按 representative xyz AoS gate 解释，不外推为完整泛型性能结论。
- `correspondence-pair` production RVV 已取消；后续需要真实 workload index-distribution sampling（索引分布采样）或新的 bounded production probe（有界生产探针）才能重开。

## doc-rvv 适用性

`doc-rvv/registration/transformation_estimation_dual_quaternion-RVV.zh.md` 适用于当前 retained production patch。
若用户取消接入，长期文档需要同步删除或回滚。
