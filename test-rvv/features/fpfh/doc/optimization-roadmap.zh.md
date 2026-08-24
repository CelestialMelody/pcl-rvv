# FPFH Optimization Roadmap

## 当前边界

当前 topic 已完成 Phase 000、Phase 001 和 Phase 002。Phase 002 将
`features/include/pcl/features/impl/fpfh.hpp::weightPointSPFHSignature` 的 33-bin 加权合成接入
`__RVV10__` production path（生产路径），并用 post-integration repeated board data（接入后重复板卡数据）
证明有界保留成立。

当前 adopted production behavior（已采用生产行为）的范围是：

- production helper：`pcl::detail::weightFPFHSignature33RVV`，由 `weightPointSPFHSignature` 短路尝试。
- layout：`hist_f1/hist_f2/hist_f3` 各 11 bins，Eigen column-major matrix，输出为 contiguous `FPFHSignature33`。
- row source：production lookup 后任意有效 SPFH row indices，包括非连续重映射行。
- evidence：QEMU Std/RVV 6/6 pass，asm 归到 `fpfh.hpp`，Milkv-Jupiter repeated board 中 detail 4.804x、public 1.262x，Evidence Doctor 0 Error / 0 Warning。

当前不把该结论外推到 `computePointSPFHSignature`、OMP FPFH、generic point type（泛型点类型）、
`Scalar=double`、custom bin count 或真实工作负载全集。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `weighted-spfh-33-production-rvv` | Phase 001 dense-row diagnostic + Phase 002 production probe | `weightPointSPFHSignature`, valid SPFH row indices, `PointNormal -> FPFHSignature33`, float, 33 bins | repeated board detail avg 4.804x；public KSearch case avg 1.262x | 只覆盖 weighted FPFH 合成；public 收益依赖数据集和 search 占比 | QEMU correctness、remapped-row regression、asm attribution、board repeated、Evidence Doctor | `adopted_with_bounded_scope` | no next phase inside this family |
| `spfh-pair-feature-batch` | `computePointSPFHSignature` 中每邻居 pair feature | 固定 neighborhood component，`PointNormal` xyz/normal AoS | 如果 pair math 是主成本，可减少 SPFH 构建时间 | `atan2`、`acos` helper 已存在但为近似；SPFH 11-bin 输出对 bin boundary（分箱边界）敏感；histogram scatter 冲突语义未闭合 | caller-specific bin-stability test、boundary-margin fallback、scatter staging design、component candidate board repeated | `deferred_with_stop_condition_hit` | recover only after bin-stability/scatter prework |
| `public-fpfh-k` | 队列建议和公开主路径 | `FPFHEstimation::compute` KSearch synthetic cloud | 继续作为 dilution check（稀释检查）评估局部收益是否转化为公开入口收益 | search dominates；synthetic 数据不能代表全部 workload | public correctness、board repeated、dataset variation | `adopted_as_check_case` | reuse in future phases |
| `pair-feature-shared-helper` | `features/src/pfh.cpp` 由 FPFH/PFH/VFH 共享 | caller-shaped batch helper | 若 pair math 可批处理，可能跨 PFH family 复用 | 本文件无 batch API；生产 API 变更风险高 | FPFH / PFH sibling evidence、caller-shaped tests | `deferred` | after FPFH pair-feature audit |
| `generic-point-type-expansion` | production template scope remains wider than evidence | PointNormal-like traits, custom point types, `Scalar=double` | 扩大 production coverage | traits、offset、POD / standard-layout、custom output layout 和 fallback 矩阵未闭合 | traits/layout audit、fallback tests、production direct bench、asm、board | `deferred` | separate point-type expansion phase |
| `omp-carryover` | `fpfh_omp.hpp` may call weighted helper or need separate threading evidence | OMP FPFH entry | 可能间接受益或需要专门分流 | OpenMP scheduling、RVV contention、bench attribution 复杂 | OMP correctness、threaded board bench、Doctor | `deferred` | after scalar FPFH closeout |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| Phase 000 | `weighted-spfh-33` test-only RVV helper | baseline Std/RVV build 没有稳定收益，但 weighted 33-bin 是最窄的可控组件。 | same-chain correctness、QEMU、asm、board repeated、Doctor | done |
| Phase 001 | PI1 row-source gate audit | dense-row candidate 不能证明 production remap；需要 arbitrary valid row indices。 | remapped-row test、production helper bounds gate、asm 和 board direct | done |
| Phase 002 | include-order freshness check | 初始 asm 没有命中 production helper，原因是 test-rvv include path 使用 installed PCL header。 | Makefile include order 修正、`make -B dump_bench_rvv` 和 `addr2line` | done |
| Phase 002 | evidence metadata enrichment | Doctor 仍提示 environment metadata 和 binary identity 缺失；当前正向足够，但遇到反转时解释力不足。 | board summary 增加 taskset/governor/freq/temperature/binary hash | medium |
| Phase 002 | `spfh-pair-feature-batch` math audit | weighted helper 已闭合，剩余主成本在 SPFH pair feature 和 histogram scatter。 | RVV math helper availability、finite-domain approximation policy、same-chain tests | high |
| Phase 003 | bin-stability prework before pair-feature candidate | `acos_RVV_f32m2` 和 `atan2_RVV_f32m2` 已存在，但它们是近似 helper；SPFH bin index 比纯角度误差更敏感。 | pair-order decision、f1/f2/f3、bin index same-chain tests；靠近 bin boundary 的 fallback policy | high if reopened |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| `weighted-spfh-33-test-only-candidate` | Phase 001 只是 diagnostic evidence（诊断证据），Phase 002 已用 production boundary 证据替代采纳依据。 | 仅当 production helper 出现回归时作为诊断对照重跑。 |
| generic production gate | 当前 production patch 没有 traits/layout 泛型审计，不能把 `PointNormal` evidence 外推到所有模板实例。 | 新建 point-type expansion phase，补每个 gate 的 fallback 和 board evidence。 |
| direct OMP carryover | 当前没有 OMP direct test 或 threaded board evidence。 | 单独审计 `fpfh_omp.hpp` 调用链、线程调度和 weighted helper 是否共享。 |
| immediate pair-feature RVV implementation | Phase 003 审计显示数学 helper 存在，但 bin-stability 和 histogram scatter 语义未闭合；直接实现会把近似误差和冲突累加风险混在一起。 | 先补 caller-specific bin-stability test、boundary-margin fallback 和 scatter staging design。 |

## 默认恢复动作

`next_phase_default`: `topic_closeout_ready_for_topic_only_commit`。

用户已确认当前 topic 可以结束并进入提交流程。本 topic 默认停在
`topic_closeout_ready_for_topic_only_commit`：

- 提交 production diff、test-rvv FPFH 资产、Phase 002 / Phase 003 文档、evaluation、roadmap、matrix、
  `doc-rvv/features/fpfh-RVV.zh.md` 和 features 队列表更新。
- 不默认提交 `test-rvv/features/fpfh/log/**` raw logs；被文档引用的 manifest / Doctor / summary 属于可审查证据边界，但提交仍需用户明确要求。
- 不把 `component_spfh_signature` 的 near-threshold 结果写成优化成功；Phase 003 已说明恢复 pair-feature 需要先补
  bin-stability 和 scatter staging 证据。
