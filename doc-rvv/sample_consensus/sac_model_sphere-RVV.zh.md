# SampleConsensusModelSphere RVV 生产实现

## 当前状态

`SampleConsensusModelSphere<PointT>` 当前有两个 production 行为需要区分：

- `countWithinDistance` 在 RVV 构建下使用已有 `countWithinDistanceRVV`。
- `selectWithinDistance` 的 Phase 045/046 `vcompress` RVV path 已是 adopted production behavior，
  接管 indexed xyz gather（按索引离散加载坐标）、平方距离、球壳双边界判断和命中 lane 压缩写回。

`getDistancesToModel` 没有接入 production RVV。当前测试专用 `RVV squared-distance + scalar sqrt/store` candidate 在板卡上退化，保持标量入口。

## 函数语义和标量路径

基础球模型使用四个 `Eigen::VectorXf` 系数：球心 `x/y/z` 和半径 `r`。三条距离入口都遍历 `indices_` 指向的输入点：

| public entry | 输出语义 | 当前 production 状态 |
| --- | --- | --- |
| `selectWithinDistance` | 判断点到球心的平方距离是否落在 `[max(0, r-threshold)^2, (r+threshold)^2]`，命中时按 `indices_` 顺序写 `inliers`，并写 `abs(sqrt(sqr_dist)-r)` 到 `error_sqr_dists_`。 | RVV adopted for current scope |
| `countWithinDistance` | 使用同一球壳判断，只累计内点数量。 | RVV adopted |
| `getDistancesToModel` | 为每个 index 输出 `abs(norm(point-center)-r)`。 | scalar-only |

`selectWithinDistanceStandard` 保存原标量主体，用于 fallback（回退路径）和测试对拍。它仍使用 `getVector3fMap().squaredNorm()`，命中后执行有序 `push_back` 和精确 `sqrt`。

## 当前生产实现与点型边界

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| `selectWithinDistance` Phase 045/046 `vcompress` 分流 | adopted/current production behavior | `PointXYZ` 生产 public entry 重跑后 5-run median `2.0989x`；用户确认收益即可采纳后保留 patch。 | `production-vcompress-repeated-evidence-manifest.json`；`selectWithinDistanceRVV` asm count `27`，可见 `vcompress.vm`。 | 证明当前 direct indexed `indices_`、registered single-float xyz layout 和 `PointXYZ` board case。 |
| `selectWithinDistance` Phase 050 `PointXYZI` 点型扩展 | adopted for `PointXYZI` | `PointXYZI` 生产 public entry 5-run median `1.5901x`，全 run 正向。 | `point-type-repeated-evidence-manifest.json`；`PointXYZI` `selectWithinDistanceRVV` asm count `27`。 | 只关闭 `PointXYZI`，不外推到自定义点型。 |
| `selectWithinDistance` Phase 060 RGB/RGBA 点型扩展 | adopted for tested RGB/RGBA point types | `PointXYZRGB` public select median `1.6225x` 且 5/5 run 正向；`PointXYZRGBA` median `1.5528x`，但 1/5 run 为 `0.9184x`。 | Phase 060 RGB/RGBA manifests / Doctors；两点型 `selectWithinDistanceRVV` asm count `27`。 | RGBA 带稳定性和长尾 warning；不外推到自定义 registered xyz 点型或其它规模。 |
| `selectWithinDistance` Phase 020 RVV 分流 | adopted historical baseline | 生产 public entry 重跑后 5-run median `1.5020x`。 | `production-repeated-evidence-manifest.json`；historical asm count `17`。 | 若用户要求回滚 Phase 045，可回到该实现族。 |
| `countWithinDistance` RVV | adopted | 既有 production RVV 仍稳定正向。 | Phase 045 5-run median `3.5154x`，asm count `19`。 | 本文只记录回归状态，不改变已有实现。 |
| `getDistancesToModel` RVV | rejected for current family | 当前 candidate 额外 scratch store 后仍要逐点标量 `sqrt`，`PointXYZ` 5-run median `0.7723x`。 | Evidence Doctor 对 candidate 报 5/5 退化。 | 只有出现 RVV sqrt/helper 审计或新的 dense-store 消融时恢复。 |

`selectWithinDistanceRVV` 的每个 VL chunk（可变向量长度分块）流程如下：

1. 从 `indices_` 加载一段 index，并转成 `index * sizeof(PointT)` 的 32-bit byte offset（字节偏移）。
2. 使用 `pcl::rvv_load::indexed_load3_fields_f32m2` 按当前 `PointT` 的 traits offset 读取 `x/y/z`。
3. 广播球心并调用 `sqr_distRVV_f32m2` 计算平方距离。
4. 用 shell mask（球壳掩码）筛出命中 lane，并通过 `vcompress` 压缩 index 和平方距离。
5. 直接把压缩后的 index 写入预分配 `inliers`，再用标量 lane loop 只对命中点执行精确 `sqrt` error distance 写回。

`vcompress` 保留 chunk 内 lane 顺序，因此输出顺序仍与标量 `indices_` 遍历一致。Phase 046 已把该 patch
标为当前采用的 production 行为。

## 覆盖范围与 Fallback

| 条件 | 行为 |
| --- | --- |
| 非 `__RVV10__` 构建 | 只编译和调用 `selectWithinDistanceStandard`。 |
| `pcl::rvv::RVVXYZFloatLayout<PointT>::value == false` | public entry 回退 `selectWithinDistanceStandard`。 |
| `input_->points.size() > pcl::rvv::rvvMaxU32ByteOffsetElements<PointT>()` | 避免 32-bit byte offset 溢出，回退标量。 |
| `PointT` 满足 registered single-float xyz layout | 可以进入 RVV helper；offset 来自当前 `PointT` traits，不硬编码 `PointXYZ`。 |
| `getDistancesToModel` | 始终使用当前标量路径。 |
| `countWithinDistance` | 保持既有 RVV / SSE / AVX / Standard 分流。 |

当前性能证据覆盖 `PointXYZ`、`PointXYZI`、`PointXYZRGB` 和 `PointXYZRGBA` 板卡 case。不能把这些性能结论自动外推到自定义点型或其它规模。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- |
| `selectWithinDistance` | production public entry | 做模型有效性检查并按 RVV gate 分流。 | SAC model callers | production boundary | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_sphere.hpp` |
| `selectWithinDistanceStandard` | production Std helper | 保存原标量 fallback 语义。 | `selectWithinDistance`、测试 direct helper 对拍 | fallback coverage | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_sphere.hpp` |
| `selectWithinDistanceRVV` | production RVV helper | 执行 indexed xyz gather、平方距离和球壳判断。 | `selectWithinDistance` | production direct / asm attribution | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_sphere.hpp` |
| `src/test_sac_model_sphere.cpp` | correctness tests | 保留 gtest case；共享 helper 经 `include/test_sac_model_sphere.h` 引入。 | `run_test_compare` | correctness gate | `test-rvv/sample_consensus/sac_model_sphere/src/test_sac_model_sphere.cpp` |
| `include/impl/sac_model_sphere_access.hpp` | internal test support | 对拍 public entry、Standard helper、RVV helper 和测试专用候选。 | test / bench 聚合头 | correctness / diagnostic support | `test-rvv/sample_consensus/sac_model_sphere/include/impl/sac_model_sphere_access.hpp` |
| `include/bench_sac_model_sphere.h` | bench wrapper | 计时 public select/count/getDistances 和保留的 test-only candidate。 | board repeated targets | board performance input | `test-rvv/sample_consensus/sac_model_sphere/include/bench_sac_model_sphere.h` |
| `src/bench_sac_model_sphere.cpp` | bench entry | 保留 CLI 点型参数解析。 | board repeated targets | bench executable entry | `test-rvv/sample_consensus/sac_model_sphere/src/bench_sac_model_sphere.cpp` |
| `generate_sphere_board_evidence_manifest.py` | analysis script | 把 5-run board logs 转成 Evidence Doctor manifest。 | `record_production_board_evidence_state` | manifest generation | `test-rvv/sample_consensus/sac_model_sphere/script/generate_sphere_board_evidence_manifest.py` |
| Phase 020 production manifest / doctor | evidence output summary | 保存 production direct repeated board 和异常检查。 | evaluation / 本文 | production evidence | `test-rvv/sample_consensus/sac_model_sphere/doc/phases/020-select-production-integration-plan/` |

## 数值算例

设模型为球心 `(1.0, -2.0, 0.5)`、半径 `2.0`、阈值 `0.25`。一个点 `(3.0, -2.0, 0.5)` 的平方距离为：

```text
sqr_dist = (3.0 - 1.0)^2 + (-2.0 + 2.0)^2 + (0.5 - 0.5)^2 = 4.0
inner = (2.0 - 0.25)^2 = 3.0625
outer = (2.0 + 0.25)^2 = 5.0625
```

因为 `3.0625 <= 4.0 <= 5.0625`，该点进入 `inliers`。误差距离为 `abs(sqrt(4.0)-2.0)=0.0`。RVV helper 只批量计算 `sqr_dist`；最终 `sqrt` 和 push order（写入顺序）仍按标量 lane 顺序执行。

## Bench 与证据

当前 Phase 045 production `vcompress` 数据来自板卡 repeated 结果：

```text
SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_sphere collect_production_vcompress_repeated_board_evidence
make -C test-rvv/sample_consensus/sac_model_sphere record_production_vcompress_board_evidence_state
```

| 证据 | 结果 | 说明 |
| --- | --- | --- |
| QEMU correctness | Std/RVV 各 6 个测试通过。 | QEMU 只证明 correctness 和路径，不证明性能。 |
| production `selectWithinDistance` `PointXYZ` board | median `2.0989x`，min/max `2.0867x / 2.1404x`。 | 支撑 Phase 045/046 adopted production behavior。 |
| production `selectWithinDistance` `PointXYZI` board | median `1.5901x`，min/max `1.4672x / 1.6317x`。 | 支撑 Phase 050 `PointXYZI` 点型扩展。 |
| production `selectWithinDistance` `PointXYZRGB` board | median `1.6225x`，min/max `1.6181x / 1.6331x`。 | 支撑 Phase 060 RGB 点型扩展；5/5 run 正向。 |
| production `selectWithinDistance` `PointXYZRGBA` board | median `1.5528x`，min/max `0.9184x / 1.6354x`。 | 支撑 Phase 060 RGBA 点型扩展；median 正向但 1/5 run 退化且有长尾 warning。 |
| production `countWithinDistance` board | median `3.5154x`，min/max `3.5121x / 3.5201x`。 | 既有 RVV 回归仍正向。 |
| `selectWithinDistanceRVV` asm | RVV instruction count `27`，可见 `vcompress.vm`。 | 符号级归属闭合。 |
| Evidence Doctor | Phase 045 `Errors=2, Warnings=0, Suggestions=1`；Phase 050 `Errors=1, Warnings=1, Suggestions=1`；Phase 060 RGB `Errors=1, Warnings=1, Suggestions=1`；Phase 060 RGBA `Errors=1, Warnings=3, Suggestions=1`。 | Errors 属于未接入的 `getDistancesToModel` 行；RGBA select warning 已保留为稳定性风险。 |

## 正确性与高效性证据链

- correctness：`run_test_compare` 在 Std/RVV 两侧均通过 6 个 gtest，其中 `ProductionSelectWithinDistanceMatchesStandardHelper` 直接对拍 public entry、Standard helper 和 RVV helper，`PointXYZRGBAndRGBALayoutsMatchReference` 覆盖 RGB/RGBA layout correctness。
- path / asm：强制重建 RVV bench 后，`selectWithinDistanceRVV` 符号下可归属 27 条 RVV 指令，并可见 `vcompress.vm`。
- performance：板卡 5-run repeated 中 `PointXYZ` public `selectWithinDistance` 全部正向，median `2.0989x`；`PointXYZI` 全部正向，median `1.5901x`；`PointXYZRGB` 全部正向，median `1.6225x`；`PointXYZRGBA` median `1.5528x` 但 1/5 run 退化。
- boundary：Phase 045/050/060 结论只覆盖当前 production scope、列出的内建代表点型和 65536 点 board case；`getDistancesToModel`、自定义点型和其它规模仍未采纳。
- risk：`PointXYZRGBA` 的 public select 有 1/5 run 退化和长尾 warning；自定义 registered xyz 点型和其它规模仍未覆盖。

## Production Closeout

| 文件 | 改动 | 回滚边界 |
| --- | --- | --- |
| `sample_consensus/include/pcl/sample_consensus/sac_model_sphere.h` | 新增 protected `selectWithinDistanceStandard` 和 `selectWithinDistanceRVV` 声明。 | 删除两个 helper 声明，并把 public entry 恢复为原标量主体。 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_sphere.hpp` | 抽出 Standard helper，新增 RVV helper 和 public dispatch。 | 移除 RVV helper 和 dispatch，保留或内联 Standard helper 均可恢复原行为。 |
| `test-rvv/sample_consensus/sac_model_sphere/**` | 新增 production direct 测试、bench evidence target、manifest / doctor。 | 测试资产可保留为诊断和回归证据。 |

Phase 045/046 已完成 S11 closeout，Phase 050 已补 `PointXYZI` 点型扩展证据，Phase 055 已完成 topic-local
test support 结构迁移，Phase 060 已补 RGB/RGBA dedicated board evidence。当前没有可直接继续的内建代表点型扩展；自定义 registered xyz 点型需要先定义代表类型和测试边界。
