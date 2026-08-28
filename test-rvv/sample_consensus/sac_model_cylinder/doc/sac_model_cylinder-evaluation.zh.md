# sac_model_cylinder 函数级评估

## 当前 EvidenceDecision

`SampleConsensusModelCylinder<PointT, PointNT>` 的三个距离相关公开入口已经完成 production adoption（生产采纳）：

- `countWithinDistance`
- `selectWithinDistance`
- `getDistancesToModel`

最终 EvidenceDecision（证据决策）为
`production-adopted/count-select-getDistances-direct-indexed-representative-point-types`。该结论依据接入后的
production direct（真实生产路径）5-run board repeated（重复板卡测试），不使用 Phase 000 diagnostic
（诊断）性能外推。

当前采纳范围覆盖 direct indexed `indices_`、`PointXYZ + Normal`、`PointXYZI + Normal`、
`PointXYZRGB + Normal` 和 `PointXYZ + PointNormal` 四组代表点型、float xyz/normal AoS（结构数组）布局、
`Eigen::VectorXf` model coefficients 和 65536 点 shuffled adjacent pairs bench case。`optimizeModelCoefficients`、
`projectPoints`、`doSamplesVerifyModel`、`Scalar=double`、自定义点型全集和真实上游 RANSAC 分布不在本次采纳范围内。

## 函数语义和标量路径

三个入口都先调用 `isModelValid`，再按 `indices_` 扫描输入点。标量路径对每个 index 做同一圆柱距离核：

```text
point -> point-to-axis radial direction dir
weighted_euclid = (1 - normal_distance_weight) * abs(norm(dir) - radius)
normal_angle = acute angle(normal[index], dir)
distance = abs(normal_distance_weight * normal_angle + weighted_euclid)
```

`countWithinDistance` 和 `selectWithinDistance` 会先检查 `weighted_euclid > threshold`，若欧氏项已经失败则提前跳过
normal angle（法线夹角）。`getDistancesToModel` 没有阈值早停，必须为每个 index 生成一个 dense distance
（连续距离）输出。

## 生产实现审计

| 入口 | 当前 production helper | RVV 机制 | fallback |
| --- | --- | --- | --- |
| `countWithinDistance` | `countWithinDistanceStandardCylinder` / `countWithinDistanceRVVCylinder` | indexed xyz/normal gather、radial norm、normal angle、mask popcount。 | 非 RVV、layout 不满足、index type 不满足、offset 过大、normal 数不足。 |
| `selectWithinDistance` | `selectWithinDistanceStandardCylinder` / `selectWithinDistanceRVVCylinder` | 同一距离核，`vcompress.vm` 保序写 index，`vfwcvt + vse64` 写 double error。 | 同上；空命中会清空输出。 |
| `getDistancesToModel` | `getDistancesToModelStandardCylinder` / `getDistancesToModelRVVCylinder` | 同一距离核，所有 lane 都计算 normal angle，再 `vfwcvt + vse64` 写 dense double vector。 | 非 RVV、layout 不满足、index type 不满足、offset 过大、normal 数不足。 |

生产源码没有修改 public API（公开接口）。RVV helper 使用 traits-gated xyz + normal AoS layout；不满足 gate 时，
公开入口调用 Standard helper 保持原语义。

## 正确性与高效性证据链

| 证据层 | 当前证据 | 结论边界 |
| --- | --- | --- |
| correctness | `make -C test-rvv/sample_consensus/sac_model_cylinder run_test_compare`：Std/RVV 各 11 个 gtest 通过。 | 覆盖 public-vs-Standard、diagnostic cross-check、select stale state、getDistances dense output、typed representative correctness 和 count/select normal coverage fallback。 |
| QEMU / log shape | 窄参数 bench smoke 可输出 Dataset / Iterations / Checksum / timing 行。 | QEMU timing 不用于性能结论。 |
| asm attribution | `make clean_bench_rvv check_production_asm`：count 306 条、select 338 条、getDistances 298 条 RVV 指令。 | 三个 production helper 均有 RVV 指令归属；新增点型导致更多模板实例化。 |
| board performance | 5-run production repeated：基础 `PointXYZ + Normal` count/select/getDistances median 为 `5.3124x` / `4.6445x` / `6.9318x`；三组新增代表点型同样全部 positive。 | 只证明当前 public entry、四组代表点型、direct indexed shuffled case。 |
| Evidence Doctor | `doc/phases/020-cylinder-production-integration/production-repeated-evidence-doctor.md`：Errors=0、Warnings=0、Suggestions=0。 | Doctor 未发现脚本规则覆盖的异常；仍需保留范围边界。 |
| registry | `log/evidence_registry.json` | production manifest / doctor 已登记，`production_evidence_status` fresh。 |

## Traceability Map（可追踪性地图）

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `SampleConsensusModelCylinder::countWithinDistance` | production public entry | 真实公开计数入口，按 RVV gate 分流。 | SAC / RANSAC caller | RVV / Standard helper | production boundary | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_cylinder.hpp` |
| `SampleConsensusModelCylinder::selectWithinDistance` | production public entry | 真实公开选择入口，写 `inliers` 和 `error_sqr_dists_`。 | SAC / RANSAC caller | RVV / Standard helper | production boundary / output order | 同上 |
| `SampleConsensusModelCylinder::getDistancesToModel` | production public entry | 真实公开 dense distance 输出入口。 | MSAC / MLESAC 等距离消费者 | RVV / Standard helper | production boundary / dense output | 同上 |
| `computeCylinderDistanceTermsRVV` | production RVV math helper | 在 VL chunk 内计算 radial distance 和 acute normal angle。 | 三个 RVV helper | RVV sqrt / angle helper | shared RVV formula | 同上 |
| `countWithinDistanceRVVCylinder` / `selectWithinDistanceRVVCylinder` / `getDistancesToModelRVVCylinder` | production RVV helper | 承载 indexed gather、mask、compress、double store 或 dense store。 | public entries | RVV load wrappers | asm attribution | 同上 |
| `src/test_sac_model_cylinder.cpp` | correctness test | 对拍 public entry、Standard helper、diagnostic candidate 和代表点型组合。 | `run_test_compare` | gtest assertions | correctness gate | `test-rvv/sample_consensus/sac_model_cylinder/src/test_sac_model_cylinder.cpp` |
| `src/bench_sac_model_cylinder.cpp` | bench wrapper | 计时 public count/select/getDistances、三组 typed public entries 和历史 diagnostic candidate。 | board smoke / repeated target | manifest script | board performance input | `test-rvv/sample_consensus/sac_model_cylinder/src/bench_sac_model_cylinder.cpp` |
| `script/generate_cylinder_board_evidence_manifest.py` | analysis script | 从 repeated board logs 和 asm 生成 manifest。 | evidence Make targets | Evidence Doctor | summary artifact | `test-rvv/sample_consensus/sac_model_cylinder/script/generate_cylinder_board_evidence_manifest.py` |
| production manifest / doctor | evidence output summary | 保存 12 项 production public Std/RVV 对比和体检结果。 | manifest / doctor targets | evaluation、doc-rvv、registry | EvidenceDecision input | `test-rvv/sample_consensus/sac_model_cylinder/doc/phases/020-cylinder-production-integration/` |
| `doc-rvv/sample_consensus/sac_model_cylinder-RVV.zh.md` | long-term production doc | 保存当前生产行为、fallback、证据链和后续边界。 | worker / reviewer | topic-local docs | long-term maintenance | `doc-rvv/sample_consensus/sac_model_cylinder-RVV.zh.md` |

## Doc Suite Role Inventory

| role | 状态 | 证据 |
| --- | --- | --- |
| topic_navigation | `standalone:test-rvv/sample_consensus/sac_model_cylinder/README.zh.md` | 指向长期 doc、evaluation、testing、evidence、phase 和 matrix。 |
| testing_overview | `standalone:test-rvv/sample_consensus/sac_model_cylinder/doc/testing-overview.zh.md` | 覆盖 target 粒度和 QEMU / board 边界。 |
| correctness_tests | `standalone:test-rvv/sample_consensus/sac_model_cylinder/doc/correctness-tests.zh.md` | 覆盖 11 个 gtest 字典。 |
| benchmark_and_evidence | `standalone:test-rvv/sample_consensus/sac_model_cylinder/doc/benchmark-and-evidence.zh.md` | 覆盖 production board summary、Doctor、registry 和提交边界。 |
| optimization_evidence | `standalone:test-rvv/sample_consensus/sac_model_cylinder/doc/optimization-evidence.zh.md` | 覆盖 adopted / historical / deferred candidate。 |
| optimization_roadmap | `standalone:test-rvv/sample_consensus/sac_model_cylinder/doc/optimization-roadmap.zh.md` | 保留 point-type expansion 和 profile 触发路线。 |
| test_support_code_map | `standalone:test-rvv/sample_consensus/sac_model_cylinder/doc/test-support-code-map.zh.md` | 覆盖 production helper、test、bench、script 和 evidence output。 |
| phase_index | `standalone:test-rvv/sample_consensus/sac_model_cylinder/doc/phases/README.zh.md` | Phase 000-040 可恢复。 |
| evaluation_production | `standalone:test-rvv/sample_consensus/sac_model_cylinder/doc/sac_model_cylinder-evaluation.zh.md` | 本文承载 EvidenceDecision 和 Traceability Map。 |
| production_topic_doc | `standalone:doc-rvv/sample_consensus/sac_model_cylinder-RVV.zh.md` | 三入口 adopted production behavior 已创建。 |

## 生产接入判断

按用户偏好，接入后只要板卡 production direct 显示有收益即可采纳。当前四组代表点型的三入口 median speedup
均大于 `1.20x`，5/5 run 均大于 `1.0x`，Doctor 0/0/0，因此生产补丁按当前代表点型范围采纳。

当前 topic 内暂不建议继续盲跑 identity-index 专门路径或 `optimizeModelCoefficients` staging。前者需要真实 workload
或 profile 显示 identity indices 占主导；后者需要 profile 证明 Eigen array staging 接近主成本。
