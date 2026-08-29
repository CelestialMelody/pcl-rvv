# Phase 000: current state and ISM local formula diagnostic result

## 阶段结论

Phase 000 已完成局部公式 diagnostic（诊断）闭环。当前证据支持继续进入
`010-production-shaped-ism-subkernel-diagnostic`，但不支持直接接入 production（生产源码）。

本阶段实际执行顺序有一次 workflow deviation（流程偏离）：topic scaffold 和部分文档先于
`plan.zh.md` 写入；随后已补齐计划并按计划完成 correctness（正确性）、asm（反汇编）、board
repeated（板卡重复采集）、Evidence Doctor（证据体检）和 registry（证据登记）回填。该偏离不改变
证据边界，但保留给 reviewer 审查。

## 执行事实

| action | result | evidence |
| --- | --- | --- |
| topic scaffold | adopted | `test-rvv/recognition/implicit_shape_model/` |
| correctness compare | passed | `make -C test-rvv/recognition/implicit_shape_model run_test_compare` |
| asm gate | passed | `make -C test-rvv/recognition/implicit_shape_model check_ism_rvv_asm` |
| board repeated | collected 5/5 runs | `test-rvv/recognition/implicit_shape_model/log/board/repeated_phase000_ism_local_formula_diagnostic/summary.md` |
| manifest | generated | `test-rvv/recognition/implicit_shape_model/log/board/repeated_phase000_ism_local_formula_diagnostic/evidence_manifest.json` |
| Evidence Doctor | `Errors=0，Warnings=2，Suggestions=6` | `test-rvv/recognition/implicit_shape_model/log/board/repeated_phase000_ism_local_formula_diagnostic/evidence_doctor.md`、`test-rvv/recognition/implicit_shape_model/log/board/repeated_phase000_ism_local_formula_diagnostic/evidence_doctor.json` |
| registry | fresh after doc refresh | `test-rvv/recognition/implicit_shape_model/log/evidence_registry.json` |

## 板卡结果

run label 为 `ism_phase000_local_formula_diagnostic_repeated`，配置为 5-run、`iterations=100`、
`warmup_iterations=5`。结果如下：

| case | evidence role | median speedup | min | max | decision |
| --- | --- | ---: | ---: | ---: | --- |
| `descriptor_cluster_distance` | diagnostic | 1.980x | 1.910x | 1.980x | positive |
| `sigma_pairwise_max_dot` | diagnostic | 3.880x | 3.870x | 3.900x | positive |
| `vote_density_gaussian_sum` | diagnostic | 10.010x | 9.980x | 10.060x | positive |

QEMU 测试使用容差正确性：descriptor case 要求最近 cluster index 相同且 distance 误差
`<=1e-4`；sigma case 要求 `<=1e-4`；density case 要求 `max(1e-3, abs(expected) * 3e-5)`。
manifest 中保留 raw bit checksum（原始位级校验和），但 Evidence Doctor 的主 checksum 使用与
correctness test 对齐的 semantic fingerprint（语义指纹），避免把浮点规约顺序或 `expf`
近似差异误写成正确性失败。

## Evidence Doctor 解释

当前 Doctor 无 Error。两个 Warning 都是 `group_outlier`：

- `descriptor_cluster_distance` 的 median 为 1.98x，低于三条 case 的组内 median 3.88x；这说明不能把
  sigma 或 density 的更高收益外推到 descriptor path。
- `vote_density_gaussian_sum` 的 median 为 10.01x，高于组内 median；这是 `expf_RVV_f32m2`
  和向量规约在纯数组上的局部收益，不代表 radiusSearch、vote tree 或 double `std::exp`
  production 语义已经成立。

Suggestions 只提示缺少 device/taskset/governor/freq/temperature 和 binary hash（环境与二进制身份）。
当前三条 case 方向稳定且没有退化 run，因此作为 diagnostic 可以继续；下一阶段如果进入
production-shaped 或 production direct，应补更完整环境 metadata。

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | `test_helper`；Std/RVV 两侧共享 `src/bench_ism.cpp` wrapper |
| 当前决策问题 | 局部公式是否值得继续映射到 production-shaped diagnostic |
| diagnostic 是否可外推到 production | no；完整 `trainISM()` / `findObjects()` 还有 FPFH、VoxelGrid、KMeans、radiusSearch、`nth_element` 和对象状态 |
| comparison-boundary / baseline mismatch 风险 | yes；当前输入是 synthetic finite float，不是真实 descriptor、training object 或 vote tree state |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段未触发；若后续 production-shaped diagnostic 转弱或负向，不能直接拒绝 production probe，必须重新审计 |
| clean adoption 是否需要同一 production boundary 内的证据 | yes；需要 PI1-PI5、production direct correctness / asm / board / Doctor |

## Phase scope 与扩展队列

- validated_scope：测试专用 synthetic float、`FeatureSize=153`、contiguous descriptor/centers、
  PointXYZ-like AoS training points、radiusSearch 后 squared distance + strength 数组。
- unvalidated_scope：真实 FPFH descriptor 分布、真实 training clouds、KMeans label 分布、`PointT` /
  `NormalT` 泛型、完整 `trainISM()`、完整 `findObjects()`、vote list tree state、double
  `std::exp` 生产语义。
- point_type_expansion_queue：当前 not_applicable；尚无 production patch。
- phase_closeout_boundary：关闭 Phase 000 局部公式 diagnostic；不关闭 topic-level production 结论。

## 下一阶段

默认继续 `010-production-shaped-ism-subkernel-diagnostic`。优先把
`descriptor_cluster_distance` 映射回 `findObjects()` 中“每个 keypoint 对 cluster center 找最近
visual word”的形态，因为它处于识别入口、无需改变 double `std::exp` 语义，也比训练期 sigma
更接近用户可感知路径。`sigma_pairwise_max_dot` 可作为训练期第二候选保留；`vote_density_gaussian_sum`
收益最高，但因 math 语义和 tree boundary 风险更高，暂缓到后续 phase。
