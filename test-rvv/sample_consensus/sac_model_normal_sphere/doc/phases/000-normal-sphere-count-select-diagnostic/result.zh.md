# Phase 000: normal-sphere count/select 诊断结果

## S0 偏好和边界

| 字段 | 本阶段采用值 |
| --- | --- |
| preferences_loaded | 已读取 `.agents/config/defaults.yaml`；`.agents/local/user-preferences.yaml` 不存在；当前 prompt 覆盖为 RVV worker、开启 `sac_model_normal_sphere` topic、板卡可用并持续推进。 |
| 注释策略 | 配置解析出的测试资产和 diagnostic（诊断代码）使用中文说明；production（生产源码）未修改。 |
| 文档策略 | topic-local evaluation（函数级评估）、phase result（阶段结果）、roadmap（路线图）和 matrix（矩阵）承载诊断证据；无 adopted production behavior（已采纳生产行为），不创建 `doc-rvv/sample_consensus/sac_model_normal_sphere-RVV.zh.md`。 |
| 证据策略 | `summary-only`；raw board log 默认 local-only（仅本机保留），可提交候选只引用 summary、manifest、doctor 和 registry。 |
| 提交策略 | 本阶段不提交；若用户后续要求提交，topic 资产和证据摘要应与无关 dirty diff 隔离。 |

## 阶段范围

| 项 | 内容 |
| --- | --- |
| validated_scope | `PointXYZ + Normal`、`PointXYZI + Normal`，direct indexed `indices_`，`countWithinDistance` 和 `selectWithinDistance` 的测试专用 production-shaped diagnostic（生产形态诊断）。 |
| same-chain 支撑 | `getDistancesToModel` 在 Phase 000 中只作为同构链路正确性和 dense double store（稠密 double 写回）观察项，不作为生产候选。 |
| unvalidated_scope | `PointXYZRGB/RGBA`、自定义 xyz 点型、非标准 normal layout、`PointNT` 非 `pcl::Normal`、`Scalar=double`、production dispatch（生产分流）和 production direct（真实生产路径证据）。 |
| phase_closeout_boundary | 本阶段只关闭当前测试专用候选的窄范围诊断条目；不能关闭模板泛型入口或生产接入。 |

## 执行结果

| 证据类型 | 命令 / 路径 | 结果 | 边界 |
| --- | --- | --- | --- |
| TDD RED | `make -C test-rvv/sample_consensus/sac_model_normal_sphere run_test_std` | 初始 scaffold 阶段曾因候选聚合头缺失失败，用于证明测试先于 helper 完成。 | 只证明测试目标能捕捉缺失测试资产。 |
| QEMU correctness（QEMU 正确性） | `make -C test-rvv/sample_consensus/sac_model_normal_sphere run_test_compare` | Std/RVV 均通过 3 个 gtest：公开入口参考、`PointXYZI` layout、球心退化方向。 | QEMU 不证明真实性能。 |
| 反汇编归属 | `make -C test-rvv/sample_consensus/sac_model_normal_sphere dump_bench_rvv`；`build/asm/riscv/bench_sac_model_normal_sphere_rvv.full.asm` | 当前候选 helper 附近可见 `vfsqrt.v`、`vfmacc.vv/vf`、`vfdiv.vv`、`vluxei32.v`、`vmflt.vf`、`vcpop.m` 和 dense store 的 `vse64.v`。 | 反汇编说明 RVV 指令存在并可归属到测试专用候选，不证明 production helper。 |
| Board smoke（板卡小型验证） | `make -C test-rvv/sample_consensus/sac_model_normal_sphere board_smoke` | `PointXYZ + Normal` 通过 gtest；candidate select `2.856x`、count `6.177x`、getDistances `5.182x`。 | 单次板卡 smoke，不是 repeated board 稳定性结论。 |
| Board smoke | `make -C test-rvv/sample_consensus/sac_model_normal_sphere board_smoke BENCH_ARGS='65536 200 PointXYZI' OUTPUT_DIR_BOARD=log/board-pointxyzi REMOTE_BOARD_OUTPUT_DIR=<REMOTE_BOARD_OUTPUT_DIR>/log/board-pointxyzi` | `PointXYZI + Normal` 通过 gtest；candidate select `2.375x`、count `5.126x`、getDistances `4.504x`。 | 只覆盖 `PointXYZI` source layout 与独立 `Normal`。 |
| Evidence Doctor（证据体检） | `doc/phases/000-normal-sphere-count-select-diagnostic/board-evidence-doctor.md` | `Errors=0`、`Warnings=6`、`Suggestions=0`；6 个 Warning 均为 `low_run_count`。 | 可用于 Phase 000 初筛；不能写成强 production performance（生产性能）证据。 |
| Evidence registry（证据登记表） | `make -C test-rvv/sample_consensus/sac_model_normal_sphere record_evidence_state` 与 `evidence_status` | registry check 为 `fresh`。 | 当前 summary / manifest / doctor 已登记；raw logs 不在默认提交边界。 |

## 板卡摘要

| case | point type | std ms | rvv ms | speedup | bucket | evidence role |
| --- | --- | ---: | ---: | ---: | --- | --- |
| `diagnostic candidate selectWithinDistance` | `PointXYZ + Normal` | 9.8632 | 3.4534 | 2.856x | `positive` | production-shaped diagnostic |
| `diagnostic candidate countWithinDistance` | `PointXYZ + Normal` | 14.6506 | 2.3717 | 6.177x | `positive` | production-shaped diagnostic |
| `diagnostic candidate getDistancesToModel` | `PointXYZ + Normal` | 14.1026 | 2.7212 | 5.182x | `positive` | same-chain diagnostic support |
| `diagnostic candidate selectWithinDistance` | `PointXYZI + Normal` | 9.9449 | 4.1866 | 2.375x | `positive` | production-shaped diagnostic |
| `diagnostic candidate countWithinDistance` | `PointXYZI + Normal` | 14.6162 | 2.8514 | 5.126x | `positive` | production-shaped diagnostic |
| `diagnostic candidate getDistancesToModel` | `PointXYZI + Normal` | 14.2398 | 3.1615 | 4.504x | `positive` | same-chain diagnostic support |

公开入口行是 mixed-boundary cross-check（混合边界交叉检查）：production 还没有 RVV dispatch，因此 public select/count 的 `1.00x` 附近结果只说明现有公开入口在 Std/RVV build 下仍是标量上下文；public getDistances 的 `1.20x` 左右结果也不能替代候选或生产证据。

## Evidence Doctor 解释

当前 Evidence Doctor 没有 Error，因此 Phase 000 的 summary 可以作为诊断取舍依据。6 个 `low_run_count` Warning 来自单次 board smoke：它们要求文档把结论降级为初筛正向，而不是 repeated board（重复板卡测试）稳定性或 production performance。由于 candidate select/count 的 speedup 距离 `1.0x` 阈值较远，且本阶段只决定是否继续 Phase 010，不触发自动 5-run 复跑。

## Diagnostic 到 Production 错配审计

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic；候选复用真实 input、indices、normal cloud 和公开入口数据形态，但只通过测试专用 subclass 进入。 |
| A/B boundary | Std build 中同一 test helper 的 scalar fallback 对 RVV build 中同一 test helper 的 RVV path。 |
| 当前决策问题 | 判断 normal-sphere 公式、gather、mask count 和 select 输出是否值得继续做实现族消融与 PI1 审计。 |
| 是否可外推到 production | 不能直接外推。当前证据只说明窄点型、direct indexed row source 和测试专用 helper 成立。 |
| baseline mismatch 风险 | 存在。public overload 没有 RVV dispatch，candidate helper 也不是 production detail helper。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 若后续消融或 repeated board 变弱，只能说明当前 diagnostic boundary 不支持该候选；是否允许生产探针需重新审计 fallback、dispatch 和同边界 A/B。 |
| clean adoption 是否需要 production boundary 内 A/B | 需要。进入 production integration loop（生产接入闭环）后必须补真实公开入口 correctness、fallback、asm 和板卡证据，并在 PI5 暂停给用户确认。 |

## 阶段结论

Phase 000 决策为 `positive diagnostic / partial-production-candidate for narrow PI1 consideration`。这个结论只表示：`PointXYZ + Normal` 与 `PointXYZI + Normal` 的 `countWithinDistance` / `selectWithinDistance` 测试专用 RVV 候选值得继续；它不是 production-ready（可生产采纳），也不是 `doc-rvv` 长期主题文档的触发条件。

未阻塞下一步是 Phase 010：`vcompress-select-ablation`。它应在同一测试专用边界内比较当前 scalar lane writeback（标量向量通道写回）候选与 `vcompress`（RVV 保序压缩写回）候选，证明 select 输出瓶颈是否还能继续降低。
