# Phase 004 Plan：production-boundary-probe

## 阶段意图和边界

本阶段解释 Phase 002 临时 production dispatch（生产分流）为什么没有把 Phase 001 helper positive 和 Phase 003 component positive 转成 public entry speedup（公开入口加速）。默认范围只限 `test-rvv/registration/transformation_estimation_dual_quaternion` 的测试资产、bench 说明、summary / manifest 解释和 topic-local 文档；不修改 `registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp`，不重新接入 production patch。

本阶段要回答三个问题：

1. 代表点型和基础 gate 是否会挡住 RVV：Phase 003 已证明 `PointXYZ`、`PointXYZI`、`PointXYZRGB` 的 `RVVXYZAoSFloatLayout` 为 true，因此该解释不再优先。
2. public wrapper（公开入口包装层）与 test-support helper 的计时边界差异是否足以解释 helper positive / public neutral 的冲突。
3. 若后续重开 production integration loop（生产接入闭环），需要哪些 path-hit（路径命中）、fallback（回退路径）、asm attribution（反汇编归因）和 board repeated（重复板卡测试）证据才能重新进入 PI1。

## 当前状态清单

| 项目 | 当前状态 | 路径 |
| --- | --- | --- |
| production 源码 | TEDQ 无 diff，保持标量 | `registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp` |
| Phase 001 helper evidence | board repeated positive，4K / 64K / 256K median B/A 约 2.014x / 2.066x / 2.097x | `log/board/rvv_accum_full_cloud_repeated/summary.md` |
| Phase 002 production-public evidence | board repeated neutral，4K / 64K / 256K median B/A 约 1.001x / 1.002x / 1.001x；doctor Errors=1、Warnings=2、Suggestions=3 | `log/board/production_public_full_cloud_repeated/summary.md` |
| Phase 003 component evidence | accumulation-only strict A/B positive，median B/A 约 2.076x / 2.102x / 2.096x；doctor clean | `log/board/component_ablation_repeated/summary.md` |
| QEMU correctness | Std/RVV 各 10 tests passed，含 representative xyz AoS gate probe | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| evidence registry | 已登记 Phase 003 summary 和 QEMU correctness；本阶段开始前需复查 fresh | `log/evidence_registry.json` |

## 假设与候选动作

| hypothesis | 要验证的现象 | 证据动作 | 可能结论 |
| --- | --- | --- | --- |
| H1 path-hit ambiguity | Phase 002 RVV binary 未稳定命中预期 production RVV helper，或 asm attribution 不足以证明 hot path | 设计 test-only path-hit counter / gtest 方案，或定义生产重开时必须保留的 path-hit evidence contract | 若无法在不改 production 下证明，下一轮 PI1 必须把 path-hit gate 作为必做项 |
| H2 public wrapper overhead | public `ConstCloudIterator` 入口比 direct helper 有额外模板 / iterator 成本，改变 public Std/RVV 总体口径 | 复用 Phase 003 public/helper mixed-boundary 表，必要时增加更窄的 wrapper boundary case-filter | 若 public baseline 与 helper baseline 不同，helper positive 只能作为 component 线索 |
| H3 production patch shape cost | 临时 production helper 的 gate、分流、寄存器压力或内联形态抵消了 C1/C2 前端收益 | 静态审计 SVD / LLS 成熟 topic 的 public entry 分层，列 TEDQ 下一次 PI1 的 helper shape requirements | 若接法成本不可控，保持 no-production |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production-boundary path-hit probe | ordered-cloud-pair | `PointXYZ` / `float` / x,y,z AoS | test-rvv 设计，不改 production | planned path-hit test contract | no new timing unless case-filter is added | not_required unless production patch is reintroduced | planned contract only | not_applicable until summary exists | planned |
| public/helper boundary audit | ordered-cloud-pair | `PointXYZ` / `float` | existing bench wrapper | `make run_test_compare` | Phase 003 `component-ablation` context | already available | not production attribution | component doctor clean | in_progress |
| production reentry PI1 gate | ordered-cloud-pair production | representative xyz AoS / `Scalar=float` | future production integration loop only | production direct + fallback tests required | production-public repeated required | required before adoption | production symbol attribution required | required | deferred until explicit PI1 |

## 实现和测试动作

| action | 内容 | 产物 | 完成判据 |
| --- | --- | --- | --- |
| A1 freshness check | 运行 `make evidence_status`，确认 Phase 003 summary、doctor、registry 和文档引用 fresh | evidence status 输出 | 无未解释 `unregistered_change` / `unregistered_file` / `stale_doc_pending_refresh` |
| A2 boundary evidence audit | 从 Phase 002 / 003 summary 中整理 public、helper、component、solve 的计时边界差异 | `result.zh.md` 或 evaluation 更新 | 明确哪些表是 strict A/B，哪些只是 mixed-boundary cross-check |
| A3 path-hit contract | 写出下一次 PI1 必须满足的 path-hit / fallback / asm / board evidence contract；必要时补 test-only gate helper，但不接 production | topic-local docs / optional test support | reviewer 能判断重开 production 前缺哪些证据 |
| A4 production reentry decision | 判断是否需要用户授权进入 PI1，或保持 no-production 并转 row-source expansion | phase result / roadmap / matrix | `continue_stop_decision` 明确，不能把 component positive 写成 production-ready |

## Evidence Doctor 和 registry 规则

本阶段默认复用 Phase 003 的 component summary / manifest / doctor，不新增 board repeated。若新增 bench case-filter 或 summary，必须同步：

- 生成 topic-local manifest。
- 运行 Evidence Doctor。
- 调用 evidence registry record。
- 更新 README / benchmark-and-evidence / optimization-evidence 的 summary-only allowlist。

## 板卡复跑预算和决策桶

本阶段默认不复跑板卡，因为当前问题先是 production boundary contract（生产边界合同）而不是新性能数字。若新增或重跑 board evidence，沿用 5-run、20 iterations、5 warm-up 的 bounded rerun budget；decision bucket 仍按 summary script 输出的 `positive` / `weak_positive` / `neutral` / `negative` / `unstable` 解释。

## 继续 / 停止条件

若 A1-A4 只需 topic-local 文档和测试资产即可完成，本阶段应继续闭合 result，不因“需要以后 production patch”提前停止。若结论要求重改 production 源码、扩大到 public API、重跑真实 production direct board evidence 或引入新的 PI1 范围，则停在 `turn_stop_deferred`，并把下一步写成“请求用户 / reviewer 授权 production integration loop”。

## 文档更新清单

更新：

- `doc/phases/004-production-boundary-probe/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/transformation_estimation_dual_quaternion-evaluation.zh.md`
- `doc/benchmark-and-evidence.zh.md`（仅当新增 target / summary）

不创建 `doc-rvv/registration/transformation_estimation_dual_quaternion-RVV.zh.md`，因为没有 adopted production behavior（已采用生产行为）。
