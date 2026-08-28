# SampleConsensusModelStick RVV

## 当前状态

`SampleConsensusModelStick<PointT>` 的三个距离相关公开入口已经接入 RVV（RISC-V Vector，可变长向量扩展）生产路径：

- `countWithinDistance`
- `selectWithinDistance`
- `getDistancesToModel`

当前采纳范围是 direct indexed `indices_`、traits-gated xyz AoS（结构数组）点类型、`pcl::index_t` 为 32-bit signed、模型系数为 `Eigen::VectorXf` 的路径。Phase 080 的 production direct（真实生产入口直连）板卡证据显示三条 public entry（公开入口）都有稳定收益；Phase 100 进一步把 `getDistancesToModelRVV` 的 staged scalar lane（暂存后逐向量通道标量写回）替换为 RVV mask / merge（掩码 / 合并）和 double vector store（双精度向量写回），并用接入后的板卡数据确认收益。因此本主题按用户采纳口径保留生产补丁。Phase 090 已补 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 和 `PointXYZRGBNormal` 的代表性 correctness（正确性）；这些点型的 dedicated board performance（专门板卡性能）仍不外推。

## 稳定证据索引

| 证据 | 路径 / 命令 | 作用 |
| --- | --- | --- |
| Phase 080 plan | `test-rvv/sample_consensus/sac_model_stick/doc/phases/080-stick-production-integration/plan.zh.md` | 冻结生产接入范围、fallback 和 board rerun budget（板卡复跑预算）。 |
| Phase 080 result | `test-rvv/sample_consensus/sac_model_stick/doc/phases/080-stick-production-integration/result.zh.md` | 记录 PI2-PI5 执行结果、EvidenceDecision（证据决策）和 doc-suite closeout。 |
| Phase 100 result | `test-rvv/sample_consensus/sac_model_stick/doc/phases/100-stick-getdistances-vector-writeback/result.zh.md` | 记录 getDistances vector writeback（向量写回）的源码、正确性、asm、板卡和 Evidence Doctor 结果。 |
| production manifest | `test-rvv/sample_consensus/sac_model_stick/doc/phases/080-stick-production-integration/production-repeated-evidence-manifest.json` | 五轮 board repeated（板卡重复采集）生产性能摘要。 |
| Phase 100 manifest | `test-rvv/sample_consensus/sac_model_stick/doc/phases/100-stick-getdistances-vector-writeback/vector-writeback-evidence-manifest.json` | 当前 getDistances 实现形态的五轮 board repeated 生产性能摘要。 |
| Evidence Doctor | `test-rvv/sample_consensus/sac_model_stick/doc/phases/080-stick-production-integration/production-repeated-evidence-doctor.md`、`test-rvv/sample_consensus/sac_model_stick/doc/phases/080-stick-production-integration/production-repeated-evidence-doctor.json` | 证据体检结果，当前为 Errors=0、Warnings=0、Suggestions=0。 |
| registry | `test-rvv/sample_consensus/sac_model_stick/log/evidence_registry.json` | 登记 Phase 080 和 Phase 100 summary evidence（摘要证据）。 |
| correctness | `make -C test-rvv/sample_consensus/sac_model_stick run_test_compare` | Std/RVV 两个构建各 11 个 gtest 通过。 |
| asm | `make -C test-rvv/sample_consensus/sac_model_stick check_production_asm` | 三个 production RVV helper 均命中预期 RVV 指令。 |
| point type correctness | `make -C test-rvv/sample_consensus/sac_model_stick run_stick_point_type_tests` | 代表性 xyz AoS 点型 public entry 与 Standard helper 输出一致。 |

## 函数语义

`SampleConsensusModelStick` 是 deprecated（已废弃）的 3D stick 模型。源码注释说明系数 0-2 是线上一点，系数 3-5 是方向，系数 6 是宽度；实际实现中三个距离入口的系数解释不完全一致，RVV 路径必须逐入口复刻现有行为。

`countWithinDistance` 把系数 0-2 当第一端点、3-5 当第二端点，归一化 `p1 - p0` 后扫描 `indices_`。平方距离小于 `threshold^2` 的点计入内圈 `nr_i`；平方距离位于 `[threshold^2, 4 * threshold^2)` 的点计入外圈 `nr_o`；返回值是 `nr_i <= nr_o ? 0 : nr_i - nr_o`。

`selectWithinDistance` 使用和 count 相同的端点语义，只保留平方距离小于 `threshold^2` 的点。它按 `indices_` 扫描顺序写出原始点云 index，并把平方距离以 `double` 存入 `error_sqr_dists_`。

`getDistancesToModel` 把系数 0-2 当线上一点，把系数 3-5 直接当方向向量并归一化。它按 `indices_` 顺序写 dense distance（稠密距离）输出；平方距离小于 `radius_max_^2` 时写 `sqrt(sqr_distance)`，否则写 `2 * sqrt(sqr_distance)`。

## 当前采用的优化方式

公开入口先执行原有 `isModelValid` 检查。RVV 构建中，如果 `PointT` 满足 `pcl::rvv::RVVXYZAoSFloatLayout<PointT>`、`pcl::index_t` 是 32-bit signed，且 `input_->points.size()` 不超过 `pcl::rvv::rvvMaxU32ByteOffsetElements<PointT>()`，入口短路进入对应 RVV helper。任一条件不满足时调用对应 Standard helper。

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| `countWithinDistanceRVV` | adopted | RVV indexed gather（索引离散加载）批量读取 xyz，批量计算 cross product（叉积）平方距离，用两个 mask 和 `vcpop.m` 统计内圈 / 外圈。 | Phase 080 median 4.1729x，min 4.1331x。 | 只证明当前 production public direct indexed `PointXYZ` bench。 |
| `selectWithinDistanceRVV` | adopted | RVV 批量计算平方距离，用 `vcompress.vm` 保序压缩命中 index，再把平方距离转换成 `double` 写入 `error_sqr_dists_`。 | Phase 080 median 3.3023x，min 3.2085x。 | 输出顺序由 gtest 和 checksum 保护。 |
| `getDistancesToModelRVV` | adopted | RVV 批量计算平方距离和 `vfsqrt.v`，用 mask / merge 应用 `radius_max_` penalty，再通过 `vfwcvt.f.f.v` 和 `vse64.v` 写 `std::vector<double>`。 | Phase 100 median 3.6879x，min 3.4122x。 | 系数方向语义与 count/select 不同，测试单独覆盖；Phase 080 旧写回形态只作 historical baseline（历史基线）。 |
| 代表点型 correctness | adopted for correctness | 生产 gate 使用 traits-gated xyz AoS，Phase 090 已补常见点型 public vs Standard 对拍。 | `AdditionalAoSPointTypesMatchStandardPath`。 | 不证明这些点型的 dedicated board performance。 |
| `Scalar=double` | not_applicable | 本类公开入口使用 `Eigen::VectorXf` 系数。 | 源码签名。 | 不作为当前 topic 扩展。 |
| 非 direct indexed row source | not_applicable | stick 模型公开距离入口只扫描 `indices_`。 | 源码和 bench 输入。 | 不创建 row-source 扩展。 |

## 范围决策表

| 范围 | 状态 | 证据 | 下一步 |
| --- | --- | --- | --- |
| `PointXYZ` direct indexed public count/select/getDistances | adopted | Phase 080 production direct board repeated、Phase 100 getDistances vector writeback board repeated、11/11 gtest、production asm gate。 | 当前 production 行为保留。 |
| `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`、`PointXYZRGBNormal` | adopted for representative correctness | Phase 090 public vs Standard helper 对拍。 | 若要把性能结论扩大到常见点型，另建 dedicated board performance phase。 |
| traits-gated xyz AoS 其它点类型 | retained with fallback risk | 代码 gate 可命中，但本轮 correctness 和 board 数据未覆盖全部自定义点型。 | 需要按具体点型补 correctness / fallback。 |
| 非 xyz AoS、字段类型非单 float、POD / stride / offset 不满足 | scalar fallback | `RVVXYZAoSFloatLayout<PointT>` 不通过时不会编译进入 RVV 主路径。 | 当前无需扩展。 |
| 输入点云超过 32-bit byte offset 表达范围 | scalar fallback | public entry 和 RVV helper 都检查 `rvvMaxU32ByteOffsetElements<PointT>()`。 | 当前无需扩展。 |
| 非 RVV 构建 | scalar fallback | `__RVV10__` 关闭时只声明 / 编译 Standard helper。 | `run_test_compare` 的 Std 构建覆盖。 |

## 标量路径与 RVV 路径差异

| 阶段 | Standard helper | RVV helper | 语义保持证据 |
| --- | --- | --- | --- |
| 模型有效性 | public entry 调用 `isModelValid`。 | public entry 先调用同一检查。 | 公开入口 tests 使用同一系数输入。 |
| 点读取 | `(*input_)[(*indices_)[i]].getVector4fMap()`。 | `indexed_load3_f32m2` 通过 byte offset 读取 xyz。 | shuffled indices case 保护输出顺序和 gather 语义。 |
| 距离公式 | Eigen `cross3().squaredNorm()`。 | 手写 cross product，并用 `vfmacc` 累加平方。 | candidate 回归和 public production tests 对齐。 |
| count 输出 | 标量累加 `nr_i` / `nr_o`。 | mask popcount 分别统计内圈 / 外圈。 | `CountPublicEntryPreservesInnerOuterPenalty`。 |
| select 输出 | 标量 push_back index 和平方距离。 | 预分配后 `vcompress` 保序写 index，短标量 tail 写 double 平方距离。 | `SelectPublicEntryPreservesOrderAndErrorDistances`、stale clear case。 |
| getDistances 输出 | 每点 `sqrt` 后按 `radius_max_` penalty 写 double。 | `vfsqrt.v` 后用 RVV mask / merge 选择 penalty，再扩宽成 double 向量写回。 | `GetDistancesPublicEntryPreservesDirectionPenaltyAndOrder`、Phase 100 asm gate。 |

## 数值算例

测试系数为 `(1.0, -2.0, 0.5, 3.0, -0.5, 1.5, 0.10)`。对 count/select，RVV 先计算第二端点减第一端点并归一化方向；对 getDistances，RVV 直接归一化后三个系数字段作为方向。以 `GetDistancesPublicEntryPreservesDirectionPenaltyAndOrder` 的第一个输出为例，`radius_max_ = 0.10`，该点平方距离超过阈值，因此输出为 `2 * sqrt(sqr_distance)`，测试期望约为 `2.2016206`。第二个输出约为 `1.4142270`，保护 dense 输出顺序和 penalty 分支。

VL chunk（可变向量长度分块）内流程是：

```text
indices[i..i+vl) -> 32-bit byte offsets
  -> gather x/y/z
  -> subtract line point
  -> cross with normalized direction
  -> squared distance
  -> mask / vcompress / vfsqrt / vector double store according to entry
  -> count, inlier output, or dense distance output
```

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `countWithinDistance` | production public entry | 模型评分 count 入口 | SAC 派生调用 | `countWithinDistanceRVV` / `countWithinDistanceStandard` | production boundary | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp` |
| `selectWithinDistance` | production public entry | 选择 inliers 并维护 `error_sqr_dists_` | SAC 派生调用 | `selectWithinDistanceRVV` / `selectWithinDistanceStandard` | production boundary | 同上 |
| `getDistancesToModel` | production public entry | 生成 dense distance 输出 | MSAC / MLESAC 等距离消费者 | `getDistancesToModelRVV` / `getDistancesToModelStandard` | production boundary | 同上 |
| `*Standard` helpers | production Std helper | 保存原标量语义和 fallback | public entries / RVV helper fallback | 原标量循环 | fallback coverage | `sac_model_stick.hpp` |
| `*RVV` helpers | production RVV helper | 承载 RVV gather、mask、compress、sqrt 路径 | public entries | RVV point load wrapper | asm attribution | `sac_model_stick.hpp` |
| `src/test_sac_model_stick.cpp` | correctness test | candidate 回归和 public production direct tests | `run_test_compare` | gtest assertions | correctness gate | `test-rvv/sample_consensus/sac_model_stick/src/test_sac_model_stick.cpp` |
| `src/bench_sac_model_stick.cpp` | bench wrapper | 计时三条 public entry | board smoke / repeated target | manifest script | board performance | `test-rvv/sample_consensus/sac_model_stick/src/bench_sac_model_stick.cpp` |
| `script/generate_stick_board_evidence_manifest.py` | analysis script | 从 repeated board logs 生成 manifest | production board target | Evidence Doctor | evidence summary | `test-rvv/sample_consensus/sac_model_stick/script/generate_stick_board_evidence_manifest.py` |
| Phase 080 manifest / doctor | evidence output summary | 保存 5-run production public Std/RVV 对比和体检结果 | manifest / doctor targets | evaluation、本文档、registry | EvidenceDecision input | `test-rvv/sample_consensus/sac_model_stick/doc/phases/080-stick-production-integration/` |
| Phase 100 manifest / doctor | evidence output summary | 保存 getDistances vector writeback 的 5-run production public Std/RVV 对比和体检结果 | manifest / doctor targets | evaluation、本文档、registry | EvidenceDecision input | `test-rvv/sample_consensus/sac_model_stick/doc/phases/100-stick-getdistances-vector-writeback/` |
| `sac_model_stick-evaluation.zh.md` | evaluation | 保存决策审计、文档归属和后续范围 | worker / reviewer | roadmap、matrix、本文档 | recovery pointer | `test-rvv/sample_consensus/sac_model_stick/doc/sac_model_stick-evaluation.zh.md` |

## Bench 与证据

Phase 080 和 Phase 100 bench 输入都是 65536 个 `PointXYZ`，`indices_` 以相邻交换方式打乱，计时迭代 200 次，warmup 5 次。计时边界只包含公开入口调用，不包含点云、indices 或系数构造。

| public entry | Std 平均 ms | RVV 平均 ms | speedup min / median / max | 证据角色 |
| --- | ---: | ---: | ---: | --- |
| `countWithinDistance` | 1.746817 | 0.419927 | 4.1331x / 4.1729x / 4.1756x | production direct |
| `selectWithinDistance` | 2.146644 | 0.650269 | 3.2085x / 3.3023x / 3.3860x | production direct |
| `getDistancesToModel` | 2.092935 | 0.582999 | 3.4122x / 3.6879x / 3.7150x | production direct, Phase 100 current implementation |

Phase 080 的旧 `getDistancesToModel` 数据为 Std 2.028692 ms、RVV 0.773412 ms、speedup 2.5722x / 2.5883x / 2.7872x。该数据现在只作为 staged scalar lane 写回形态的 historical baseline，不作为当前源码性能 truth。

QEMU（仿真器）只用于 correctness、构建和日志形状。性能结论只来自 board（板卡）或目标硬件。

## Fallback 矩阵

| 条件 | 行为 | 证据 |
| --- | --- | --- |
| `__RVV10__` 未定义 | 只编译并调用 Standard helper。 | Std 构建的 `run_test_compare`。 |
| 点类型不满足 `RVVXYZAoSFloatLayout<PointT>` | public entry 不进入 RVV branch。 | compile-time gate（编译期门控）。 |
| `pcl::index_t` 不是 32-bit signed | public entry 不进入 RVV branch。 | compile-time gate。 |
| 输入点云规模超过 u32 byte offset 上限 | public entry / RVV helper 均调用 Standard helper。 | 源码 gate。 |
| 模型系数无效 | public entry 保持原 PCL error path。 | public entry 在 dispatch 前调用 `isModelValid`。 |
| `indices_` 为空 | RVV helper 循环自然不执行，输出为空或 count 为 0。 | gtest aggregate 覆盖公开入口常规语义；后续可补显式空输入 case。 |

## 正确性与高效性证据链

correctness（正确性）：`run_test_compare` 在 Std/RVV 两个构建中各运行 11 个 gtest。测试覆盖 stick count 的内外圈扣减、select 的保序输出和旧 `error_sqr_dists_` 清理、getDistances 的方向系数语义、dense 输出顺序、`radius_max_` penalty，以及 `PointXYZI` / RGB / RGBA / RGBNormal 代表点型的 public entry vs Standard helper 对拍。公开入口 tests 不调用 test-only candidate helper，因此能覆盖 production direct 语义。

path / asm（路径 / 反汇编）：`check_production_asm` 检查 RVV bench 反汇编，要求 `countWithinDistanceRVV`、`selectWithinDistanceRVV` 和 `getDistancesToModelRVV` 三个生产 helper 中出现 RVV 指令。Phase 100 后，该 gate 还检查 `getDistancesToModelRVV` 源码不再包含 staged scalar lane 写回，并要求 asm 中出现 `vfwcvt.f.f.v` 和 `vse64.v`。该检查避免把仅有 RVV 构建但未命中生产 helper 的二进制误写成 production direct，也避免 getDistances 写回形态退回标量 lane loop。

performance（性能）：Phase 080 的 5-run board repeated summary 是 count/select 当前性能结论来源；Phase 100 的 5-run board repeated summary 是 getDistances 当前实现形态的性能结论来源。三条 public entry 的 speedup min 和 median 都大于 1.0，Evidence Doctor 为 0/0/0。

boundary（边界）：本结论覆盖 `PointXYZ` 代表性板卡性能、traits-gated xyz AoS 生产分流，以及 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`、`PointXYZRGBNormal` 的代表性 correctness。它不证明这些点型的独立性能，也不证明 deprecated stick 模型之外的 sample_consensus 入口。

risk（风险）：生产 gate 已经允许其它满足 traits 的 xyz AoS 点类型命中 RVV。Phase 090 已降低代表点型 correctness 风险；若维护者需要把性能结论写成常见点型集合，应另建 dedicated board performance phase，补对应 bench label、board repeated 和 Evidence Doctor。

## Production closeout

| 项 | 状态 |
| --- | --- |
| production patch | adopted；修改 `sac_model_stick.h` 和 `impl/sac_model_stick.hpp`，新增 Standard / RVV helper 并接入三条 public entry。 |
| public API | unchanged。 |
| test assets | retained；`test-rvv/sample_consensus/sac_model_stick/` 保存 correctness、bench、manifest、doctor 和 registry。 |
| evidence policy | summary-only；raw board logs、QEMU logs 和 build output 默认不提交。 |
| rollback boundary | 若后续 reviewer 要求撤回，应同时撤回生产 helper / dispatch，并把本文档改回 not_applicable 或历史归档。 |

## 后续方向

当前 topic 内剩余方向是 dedicated point-type board performance。Phase 090 已证明常见 xyz AoS 代表点型能保持 public correctness；如果需要把性能结论从 `PointXYZ` 扩大到这些点型，需要另建板卡性能阶段。它属于扩展证据范围，不影响 Phase 080 / Phase 100 对 `PointXYZ` production direct 的采纳结论。

当前不建议继续做 identity-index strided load（恒等索引跨步加载）或上游 RANSAC 全流程 bench。stick 当前生产入口只有 direct indexed `indices_` 形态；全流程 RANSAC 会把采样、模型验证和上游控制流混入计时边界，不能直接解释当前三条 public entry helper 的收益。
