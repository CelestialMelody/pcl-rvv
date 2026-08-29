# implicit_shape_model RVV 生产文档

## 当前状态

`recognition/include/pcl/recognition/impl/implicit_shape_model.hpp` 已有一个窄范围 adopted production
behavior（已采纳生产行为）：`ImplicitShapeModelEstimation::findObjects()` 中 descriptor-to-cluster
nearest assignment（描述子到聚类中心最近邻分配）在 `__RVV10__` 构建下走 RVV（RISC-V Vector，
可变长度向量扩展）helper，非 RVV 构建走 Std helper。当前 `153` 只是本轮证据里使用的代表性
`FeatureSize`，不是生产分流门禁。

当前采纳范围只覆盖 `findObjects()` 公开入口的 nearest cluster assignment。`trainISM()`、
`calculateSigmas()`、`calculateWeights()`、vote density（投票密度）、其它 `FeatureSize`、其它点型
或其它目标硬件仍未被本生产文档声明为已优化。

## 稳定证据索引

| 证据 | 路径 / 命令 | 作用 |
| --- | --- | --- |
| production source | `recognition/include/pcl/recognition/impl/implicit_shape_model.hpp` | 当前真实生产行为 |
| evaluation | `test-rvv/recognition/implicit_shape_model/doc/implicit_shape_model-evaluation.zh.md` | 决策审计和 fallback matrix |
| Phase 020 result | `test-rvv/recognition/implicit_shape_model/doc/phases/020-production-integration-findobjects-public-entry/result.zh.md` | PI2-PI5 执行事实和 EvidenceDecision |
| production direct summary | `test-rvv/recognition/implicit_shape_model/log/board/repeated_phase020_public_entry_findobjects_production_direct/summary.md` | 板卡 repeated 性能摘要 |
| Evidence Doctor | `test-rvv/recognition/implicit_shape_model/log/board/repeated_phase020_public_entry_findobjects_production_direct/evidence_doctor.md` | 异常信号和降级判断 |
| manifest | `test-rvv/recognition/implicit_shape_model/log/board/repeated_phase020_public_entry_findobjects_production_direct/evidence_manifest.json` | case metadata、checksum、计时边界 |
| registry | `test-rvv/recognition/implicit_shape_model/log/evidence_registry.json` | 摘要证据登记 |
| correctness | `make -C test-rvv/recognition/implicit_shape_model run_test_compare` | topic helper 对拍 |
| upstream correctness | `make -C test-rvv/recognition/implicit_shape_model run_upstream_test_compare` | 真实公开入口回归 |
| asm | `make -C test-rvv/recognition/implicit_shape_model check_ism_rvv_asm` | RVV 指令归属 |

## 函数语义

`findObjects()` 用训练好的 `ISMModel` 和测试点云生成 vote list。入口先通过 feature estimator
计算每个 sampled point 的 descriptor，然后为每个 descriptor 找最近 cluster center，得到
`min_dist_inds`。后续代码再根据 class、方向、权重和 sigma 生成 vote。

本次 RVV patch 只替换 `min_dist_inds` 的 nearest cluster 计算。原标量路径会为每个 cluster center
复制一个 `Eigen::VectorXf`，再用 `computeDistance()` 计算平方差距离。新 helper 直接在 descriptor
和 `model->clusters_centers_` 上计算，不改变后续 vote 生成。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| dispatch（分流逻辑） | adopted | public entry 只调用 `findNearestClusterIndex()`；编译宏决定 RVV / Std | production diff | 非 RVV 构建自然 fallback |
| RVV helper | adopted | descriptor 连续加载，center row 用 `outerStride()` 跨步加载，按 VL chunk 做平方差规约 | asm gate + board summary | cluster 间 min 选择仍为标量 |
| Std helper | adopted | 保存原标量语义，避免非 RVV build 行为漂移 | `run_test_compare` | helper 位置为 internal `detail` free helper |
| `trainISM()` | not_now | 训练路径有 KMeans、feature estimator 和对象状态，当前无 production direct 证据 | roadmap | 需另建 phase |
| sigma / density | deferred | 只有局部 diagnostic positive，未闭合入口和数学语义 | Phase 000 result | 需 profile / math audit |
| 泛型扩展 | deferred | 当前板卡证据覆盖 `FeatureSize=153`、`PointXYZ` / `Normal` fixture | Phase 020 result | 其它组合需重跑 production direct |

RVV helper 的 chunk 内部流程是：

1. 用 `vsetvl` 按剩余 descriptor 维度确定 VL。
2. 连续加载 `curr_descriptor.data() + i_dim`。
3. 按 `clusters_centers_.outerStride()` 跨步加载当前 center row。
4. 计算差值和平方。
5. 用 RVV reduction（向量规约）得到当前 chunk 的平方差和。
6. 在 cluster 级外层循环中继续标量比较 `curr_dist < best_dist`，保持原 tie 选择语义。

## 范围决策表

| 范围 | 状态 | 证据 | 下一步 |
| --- | --- | --- | --- |
| `findObjects()` descriptor nearest cluster assignment | adopted | Phase 020 production direct summary | 当前关闭 |
| `FeatureSize=153` + `PointXYZ` / `Normal` public-entry fixture | validated | upstream test + board repeated | 当前关闭 |
| 非 RVV 构建 | scalar fallback | Std build correctness | 当前关闭 |
| `trainISM()` | scalar / not optimized | production source 未修改 | 另建 profile phase |
| `calculateSigmas()` | deferred | Phase 000 diagnostic positive only | trainISM-shaped profile 后再评估 |
| vote density / `std::exp` | deferred | Phase 000 diagnostic positive only | math semantic audit 后再评估 |
| 其它 `FeatureSize` / 点型 / layout | deferred | 当前无 production direct board | 新建 scope expansion phase |

## 标量路径与 RVV 路径差异

| 阶段 | 原标量路径 | RVV 路径 | 保留标量原因 |
| --- | --- | --- | --- |
| descriptor sum gate | `findObjects()` 原有循环求和，接近零则跳过 | 不变 | gate 本身简单，且要保持原跳过语义 |
| cluster center distance | 复制 center vector 后 `computeDistance()` | `findNearestClusterIndexRVV()` 直接按 descriptor / matrix 访存规约 | 已接 RVV |
| cluster min 选择 | 标量比较距离 | 标量比较每个 cluster 的规约结果 | 保持 tie 语义，避免复杂横向状态 |
| vote 生成 | class/weight/sigma/direction 状态更新 | 不变 | 输出顺序和对象状态不属于本阶段 |

## 数值算例

若 descriptor 前四维为 `[1.0, 2.0, 3.0, 4.0]`，某个 center row 前四维为
`[1.5, 1.0, 2.0, 6.0]`，一个 VL chunk 的贡献是：

```text
(1.0 - 1.5)^2 + (2.0 - 1.0)^2 + (3.0 - 2.0)^2 + (4.0 - 6.0)^2
= 0.25 + 1.00 + 1.00 + 4.00
= 6.25
```

RVV helper 对每个 chunk 得到这样的局部平方差和，再累加成当前 cluster 距离；外层仍按标量方式选择
最小距离的 cluster index。

## Bench 与证据

Phase 020 的 production direct bench case 是 `public_find_objects_descriptor_assignment`。它使用
deterministic model、测试专用 `SyntheticIsmFeature`、`pcl::PointXYZ` cloud 和 `pcl::Normal` normals，
调用真实 `findObjects()` 公开入口。计时边界包含 `findObjects()` 入口和 production helper，不包含
`trainISM()`、PCD 文件 I/O 或完整真实 feature estimator 成本。

板卡 repeated 参数：

```bash
--case-filter public_find_objects_descriptor_assignment --clusters 184 --descriptors 512 --iterations 80 --warmup-iterations 5
```

结果：5-run median `1.060x`，min `1.040x`，max `1.070x`，p10 `1.044x`，p90 `1.066x`，
`B/A < 1` 为 `0/5`。decision bucket 为 `weak_positive`。QEMU timing（仿真计时）不作为性能结论。

## 正确性与高效性证据链

| 层级 | 当前证据 | 结论 |
| --- | --- | --- |
| correctness（正确性） | `run_test_compare` 和 `run_upstream_test_compare` 通过 | helper 和公开入口语义未漂移 |
| checksum（校验） | `semantic:public_votes=494:votes_match=True:peak_density_match=True:peak_fingerprint_match=True`，raw bit checksum `7645179244906730525` | Std/RVV public-entry vote 数和峰值指纹一致 |
| asm（反汇编） | `check_ism_rvv_asm` 命中 `vle32` / `vlse32` / `vfmul` / `vfred` 等指令 | RVV helper 路径可见 |
| performance（性能） | Phase 020 board repeated median `1.060x`，0/5 退化 | 目标硬件弱正向 |
| Evidence Doctor | `Errors=0 / Warnings=0 / Suggestions=2` | 无阻塞异常；metadata 建议留作后续 |
| boundary（边界） | evidence role=`production_direct`，A/B boundary=`public_overload` | 只证明当前公开入口 RVV 比当前标量快 |

## Fallback 矩阵

| 条件 | 行为 | 语义边界 |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | 调用 `findNearestClusterIndexStd()` | 保留原标量语义 |
| `__RVV10__` 构建 | 调用 `findNearestClusterIndexRVV()` | 当前生产优化路径 |
| 非 `__RVV10__` 构建 | 调用 `findNearestClusterIndexStd()` | 保留原标量语义 |
| `number_of_clusters == 0` | 返回 `0` | 与原 `min_dist_idx` 默认值一致 |
| descriptor sum 接近 0 | `findObjects()` 原有 `continue` | RVV helper 不会被调用 |
| `trainISM()` / sigma / density | 未修改 | 保持原生产路径 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `findObjects()` | production public entry | 识别入口并生成 vote | 用户代码 | nearest-cluster helper、vote list | production boundary | `recognition/include/pcl/recognition/impl/implicit_shape_model.hpp` |
| `findNearestClusterIndexStd` | production Std helper | 标量 fallback | `findNearestClusterIndex` | `findObjects()` | fallback coverage | 同上 |
| `findNearestClusterIndexRVV` | production RVV helper | RVV 平方差规约 | `findNearestClusterIndex` | `findObjects()` | production RVV path | 同上 |
| `src/test_ism.cpp` | correctness source | helper 对拍 | `run_test_compare` | gtest | correctness gate | `test-rvv/recognition/implicit_shape_model/src/test_ism.cpp` |
| `src/bench_ism.cpp` | bench wrapper | public-entry bench | board target | analyzer script | performance input | `test-rvv/recognition/implicit_shape_model/src/bench_ism.cpp` |
| `generate_ism_evidence_manifest.py` | analysis script | summary / manifest / Doctor 输入 | Makefile evidence targets | registry | evidence manifest | `test-rvv/recognition/implicit_shape_model/script/generate_ism_evidence_manifest.py` |
| Phase 020 summary | evidence output summary | 5-run production direct 统计 | `public_entry_board_repeated` | evaluation / 本文档 | board performance | `test-rvv/recognition/implicit_shape_model/log/board/repeated_phase020_public_entry_findobjects_production_direct/summary.md` |
| Phase 020 result | phase result | 接入闭环事实 | phase loop | evaluation / Handoff | recovery pointer | `test-rvv/recognition/implicit_shape_model/doc/phases/020-production-integration-findobjects-public-entry/result.zh.md` |

## Production closeout

| 项目 | 状态 |
| --- | --- |
| production files | `recognition/include/pcl/recognition/impl/implicit_shape_model.hpp` |
| public API | unchanged |
| adopted helper | `pcl::ism::detail::findNearestClusterIndexStd/RVV/findNearestClusterIndex` |
| evidence decision | adopted production behavior / narrow |
| rollback boundary | 删除 helper 并恢复 `findObjects()` 中原 cluster loop 即可回滚该窄范围 |
| raw logs | 不默认提交；summary / manifest / doctor / registry 是摘要证据候选 |

## 后续方向

当前同 scope 没有继续自动推进的高优先级 RVV 方向。后续如果继续当前 topic，建议单独选择一个新的 phase：

- `030-findobjects-scope-expansion`：扩大到其它 `FeatureSize`、点型或 fixture。
- `030-sigma-production-shaped-profile`：检查 `calculateSigmas()` 是否在训练入口里值得接 production。
- `030-density-math-boundary-audit`：先审计 double `std::exp` 语义和 radiusSearch/tree 边界，再决定是否生产接入。
