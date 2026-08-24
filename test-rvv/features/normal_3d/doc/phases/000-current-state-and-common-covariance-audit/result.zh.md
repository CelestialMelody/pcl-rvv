# Phase 000 Result: current-state and common covariance audit

## 结论

Phase 000 完成。`NormalEstimation::computeFeature` 通过
`computePointNormal` 复用了 common 层 dense indexed covariance RVV helper；反汇编可以归属到
`pcl::computeMeanAndCovarianceMatrixRVV<pcl::PointXYZ, float>`，板卡 component ablation（组件消融）
5-run 中位数为 `1.43x`。

公开入口 `public_normal_estimation_k` 的 5-run 中位数只有 `1.02x`，Evidence Doctor 标记为
`near_threshold_ba`。当前证据说明 covariance helper 本身值得保留，但 normal 公开路径主要被 neighbor
search（邻域搜索）和 Eigen 3x3 solve（小矩阵求解）稀释；本阶段不修改
`features/include/pcl/features/impl/normal_3d.hpp`，不创建 `doc-rvv/features/normal_3d-RVV.zh.md`。

## 计划执行状态

| action | 状态 | 命令 / 证据路径 | 结论 |
| --- | --- | --- | --- |
| A1 scaffold topic harness | done | `Makefile`, `board.mk`, `src/test_normal_3d.cpp`, `src/bench_normal_3d.cpp` | topic-local correctness、bench、board 和 Doctor wrapper 已建立。 |
| A2 QEMU correctness | done | `make -C test-rvv/features/normal_3d run_test_compare` | Std / RVV QEMU 均为 `4/4` 通过；QEMU 只作为 correctness 和日志形状证据。 |
| A3 asm attribution | done | `make -C test-rvv/features/normal_3d dump_bench_rvv` | full asm 中存在 `computeMeanAndCovarianceMatrixRVV<PointXYZ,float>` 调用和函数体；函数体内可见 `vluxseg3ei32.v`、`vfmacc.vv`、`vfredosum.vs`。 |
| A4 board smoke / compare | done | `make -C test-rvv/features/normal_3d board_smoke` | 板卡 correctness 通过；单次 compare 已升级为 5-run repeated。 |
| A5 Evidence Doctor readiness | done | `make -C test-rvv/features/normal_3d evidence_doctor_repeated` | Doctor: Errors=0, Warnings=0, Suggestions=5。 |

## 板卡 repeated summary

输入：`test-rvv/features/normal_3d/log/board/repeated/run_*/analyze_bench_compare.log`。

| case | Std ms values | RVV ms values | speedup values | bucket |
| --- | --- | --- | --- | --- |
| `component_compute_point_normal_indexed` | `0.5613, 0.5666, 0.5704, 0.5693, 0.5801` | `0.3960, 0.3988, 0.3972, 0.3985, 0.3988` | `1.42, 1.42, 1.44, 1.43, 1.45` | positive component signal |
| `public_normal_estimation_k` | `109.4570, 110.3480, 108.5220, 109.1410, 109.6610` | `105.6210, 108.0960, 106.6070, 107.7940, 105.5830` | `1.04, 1.02, 1.02, 1.01, 1.04` | weak-positive / near-threshold public signal |

5-run budget 已用完；两个 case 的 decision bucket 没有方向反转，不继续自动扩跑。component case 支持
common covariance RVV 对 indexed neighborhood 的局部收益；public case 只能说明当前 RVV build 相对 Std build
有很弱收益，不能支撑新的 `normal_3d.hpp` production patch。

## Evidence Doctor

命令：

```bash
make -C test-rvv/features/normal_3d evidence_doctor_repeated
```

输出：

- Manifest: `test-rvv/features/normal_3d/log/board/repeated/evidence_manifest.json`
- Report: `test-rvv/features/normal_3d/log/board/repeated/evidence_doctor.md`
- JSON: `test-rvv/features/normal_3d/log/board/repeated/evidence_doctor.json`

Doctor 结果为 `Errors=0, Warnings=0, Suggestions=5`。建议项为两组 case 的环境 metadata
和 binary identity 缺失，以及 `public_normal_estimation_k` 的 `near_threshold_ba`。处理动作：

- 环境字段和 binary hash 缺失不阻塞 Phase 000 的 diagnostic 结论；若后续要做 production direct 或严格
  RVV-vs-RVV A/B，必须补齐。
- public case 的 `median=1.02x` 只记录为 weak-positive；不写成稳定 production 加速。

## Diagnostic-to-production mismatch audit

| question | actual result |
| --- | --- |
| evidence role | `production_shaped_diagnostic`。bench 触发 public `NormalEstimation`，但没有新增 production dispatch。 |
| A/B boundary | Std build vs RVV build；RVV 差异来自 common covariance helper，而不是 `normal_3d.hpp` 新实现族。 |
| timer boundary | component case 计时 `computePointNormal` indexed neighborhood；public case 计时 `NormalEstimation::compute`。 |
| row source / point type / Scalar / layout | dense indexed neighborhood，`PointXYZ -> Normal`，`float` covariance，AoS xyz layout，synthetic plane grid。 |
| diagnostic 是否可外推到 production | 只能部分外推。公开入口真实命中 production header，但 synthetic dataset、点型、search policy 和 Eigen solve 边界未覆盖泛型生产场景。 |
| comparison-boundary / baseline mismatch 风险 | 中等。component case 和 public case 计时边界不同；component positive 被 public search / solve 稀释。 |
| weak public 结果是否允许 bounded production probe | 当前不建议。没有 normal_3d-local hotspot 证据；继续改 production 会把弱收益和维护成本倒挂。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 当前不适用，因为没有新增 `normal_3d.hpp` RVV family。若未来新增 local family，必须补同一 production boundary 内的 RVV-vs-RVV A/B。 |

## 文档套件和证据归属

| role | 状态 | 路径 / 说明 |
| --- | --- | --- |
| topic_navigation | `standalone` | `test-rvv/features/normal_3d/README.zh.md` |
| evaluation_diagnostic | `standalone` | `test-rvv/features/normal_3d/doc/normal_3d-evaluation.zh.md` |
| phase_index / matrix | `standalone` | `doc/phases/README.zh.md`, `doc/phases/optimization-matrix.zh.md` |
| phase_plan / phase_result | `standalone` | 本 phase 的 `plan.zh.md` 和 `result.zh.md` |
| optimization_roadmap | `standalone` | `doc/optimization-roadmap.zh.md` |
| testing_overview / correctness_tests / benchmark_and_evidence / optimization_evidence / test_support_code_map | `merged` | Phase 000 只有两个源码资产、两个 bench case 和一个 Doctor wrapper；职责合并在 evaluation、README 和本 result 中。若 topic 未来重开 production probe，再拆成独立 role 文档。 |
| production_topic_doc | `not_applicable with evidence` | 没有 adopted production behavior 或 PI5 production patch，不创建 `doc-rvv/features/normal_3d-RVV.zh.md`。 |

## Matrix 更新

- `reuse common dense indexed covariance RVV`: `attempted / reused-positive-component`。common helper 的局部收益成立，但本 topic 没有新增 production patch。
- `normal_3d local flip/output RVV`: `rejected with current evidence`。public 入口收益接近阈值，且没有 local flip/output 成为热点的证据。
- 点型扩展：`not_applicable for this topic closeout`。common helper 的点型扩展应在 common covariance 或新需求 topic 中闭合，不由 `normal_3d.hpp` 本地 patch 承担。

## Continue / Stop Decision

`continue_stop_decision`: `turn_stop_deferred with stop_condition_hit`

`stop_condition_hit`: 当前 Phase 000 的目标已闭合；进一步修改
`features/include/pcl/features/impl/normal_3d.hpp` 不建议继续，因为 public entry 只有 near-threshold 弱收益，
且可归属收益来自已存在的 common RVV helper。继续需要新的真实 PCD workload、profile 指向
`normal_3d.hpp` local flip/output loop，或用户明确授权 production probe。

`next_phase_default`: 无默认同轮下一 phase。恢复条件是：

- 有真实 workload / PCD board evidence 显示 public normal path 的 local normal output loop 成为热点；
- common covariance helper 改动后需要回归 normal public path；
- 用户明确要求做 bounded production probe 或 OMP / 点型扩展评估。
