# Phase 070 Result: generic-xyz-point-type-expansion

> 历史注释：本文记录 Phase 070 当时的 test-only candidate 结论。Phase 080 后，当前
> production header 已接入 traits-gated generic ordered-cloud-pair dispatch；本文中的
> “production dispatch unchanged” 只对 Phase 070 执行时点成立，当前源码状态和接入后证据
> 以 Phase 080 result 为准。

## 当前结论

本阶段完成了 PointXYZ-like 泛型点型候选的 test-rvv 证据闭环。当前结论是：

```text
generic ordered-cloud-pair candidate: evidence-closed / test-only
production dispatch: unchanged / exact PointXYZ -> PointXYZ retained
next phase: PI1 generic ordered-cloud-pair production integration plan
```

本阶段没有修改或回滚
`registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` 的当前窄范围
production patch。generic candidate 仍只存在于 test-rvv 支撑代码；不能把本阶段代表性点型的
正确性或板卡结果写成整个 PointXYZ-like traits 集合已经接入 production。

source-indexed、dual-indexed 和 correspondence 仍是独立 row-source family，不能继承本阶段
ordered-cloud-pair 的证据。

## 执行范围

| 维度 | 已验证范围 | 未关闭范围 |
| --- | --- | --- |
| row source | ordered-cloud-pair | source-indexed、dual-indexed、correspondence |
| `Scalar` | `float` | `double` RVV |
| point type | `PointXYZ`、`PointXYZI`、`PointNormal`、`PointXYZINormal` | 未逐类型上板的其它 traits-allowed 自定义点型 |
| source / target | 四类 same-type；`PointXYZI -> PointXYZ`、`PointXYZ -> PointXYZI`、`PointNormal -> PointXYZINormal`、`PointXYZINormal -> PointNormal` | 其它混合字段、RGB/RGBA 和自定义 POD 组合 |
| layout | PCL traits 注册的 x/y/z 为单个 `float`，standard-layout POD，stride/offset 满足 AoS 分段加载 | 非 standard-layout、非 float xyz、非 AoS 或未登记 traits 点型 |
| runtime gate | dense、所有 x/y/z finite、点数不少于 16 | 非 dense、非有限和小规模只验证 fallback，不进入 RVV |
| production | 未修改 dispatch | 需要独立 PI1 后的 PI2-PI5 证据链和用户检查点 |

## 计划动作回填

| id | 状态 | 证据 | 结论 |
| --- | --- | --- | --- |
| G1 | done | `include/te2d.h` 和 TE2D 测试支撑内部头、`src/test_te2d.cpp`、`GenericXYZTraitsGateCoversRepresentativePointTypes` | source / target 分别使用 `pcl::rvv::RVVXYZAoSFloatLayout<PointT>`；四类代表性点型的 traits、POD、sizeof、offset 和对齐门控通过。 |
| G2 | done | `include/te2d.h` 和 TE2D 测试支撑内部头、`run_test_compare` | generic candidate 使用 source / target 各自 stride 与 x/y/z offset；Std 构建走标量 fallback，RVV 构建只在 gate 命中时尝试 RVV。 |
| G3 | done | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | Std / RVV 各 24/24；same-type、mixed pair、额外字段、small/non-dense/non-finite fallback 和 `Scalar=double` scalar boundary 均通过。 |
| G4 | done | `log/qemu/generic_xyz_point_types/run_bench_generic_xyz_point_types_rvv.log`、manifest、Doctor | QEMU generic smoke 覆盖 16 cases；只作为路径、日志形状、checksum 和 metadata 证据；Doctor `0/0/0`。 |
| G5 | done | `log/qemu/generic_xyz_point_types/asm_attribution.md/json` | `generic_candidate_lambda_boundary` 有 540 条 RVV 指令归属，含 `vlsseg3e32.v`、`vfmacc`、`vfredosum`；归属保持 test-support generic lambda，不冒充 production symbol。 |
| G6 | done | `log/board/generic_xyz_point_types_repeated/summary.md`、manifest、Doctor | `Milkv-Jupiter`，5 runs × 20 iterations × 5 warmup，16 cases；每 case `B/A<1=0/5`，按具体点型/规模分桶。 |
| G7 | done | `log/evidence_registry.json`、QEMU/board Doctor | QEMU Doctor `0/0/0`；board Doctor `Errors=0, Warnings=3, Suggestions=2`，Warning/Suggestion 已在本文和 Handoff 解释。 |
| G8 | done | 本 result、roadmap、matrix、evaluation、Handoff、PI1 plan | 只形成新的 PI1 production integration plan；本阶段不修改 production dispatch。 |

## Traits Gate 与 source/target 审计

generic gate 使用公共
`pcl::rvv::RVVXYZAoSFloatLayout<PointT>::value`，不在 topic-local 重复定义字段 traits。
该 gate 只描述输入字段语义和 AoS 访存前提，不包含 dense/finite、规模、`Scalar`、row source
或 production dispatch 条件。

source 和 target 分别实例化 gate，并分别读取：

- `pcl::traits::offset<PointT, pcl::fields::x/y/z>`；
- `sizeof(PointT)` 与 `pcl::traits::POD<PointT>::type`；
- source / target 各自 stride 和字段 offset；
- source / target 各自 dense 和 finite 运行期语义。

mixed pair 证明了两侧 stride 不必相同。额外 intensity、normal 字段使用非零有限值，
但改变这些字段不改变矩阵；这只证明当前 2D 算法读取 x/y/z，不代表其它会写回完整点型的
算法可以复用同一 gate。

## Correctness 与 fallback

`run_test_compare` 重跑结果：

| build | tests | result |
| --- | ---: | --- |
| Std | 24/24 | pass |
| RVV | 24/24 | pass |

generic 相关覆盖：

- `PointXYZ`、`PointXYZI`、`PointNormal`、`PointXYZINormal` 四组 same-type；
- 四组 source/target mixed pair；
- 额外字段变化不影响估计矩阵；
- 8 点输入回退；
- `is_dense=false` 且点仍 finite 时回退；
- x/y/z 含 NaN 或 Inf 时回退；
- `TransformationEstimation2D<..., double>` 保持 public scalar boundary。

测试还保留旧的 ordered-cloud-pair public semantics、三类 row-source scalar boundary、
near-cancellation 和 production exact gate regression。

## QEMU 与反汇编证据

QEMU generic smoke 为 16 cases，命令入口：

```bash
make -C test-rvv/registration/transformation_estimation_2D run_qemu_generic_evidence_doctor
make -C test-rvv/registration/transformation_estimation_2D record_qemu_generic_state
```

QEMU 只证明 RVV binary 可运行、case label / iterations / warmup / checksum / manifest
可解析；不证明板卡性能。

generic asm attribution 聚焦 `generic_candidate_lambda_boundary`：

- 归属总计：540 条 RVV 指令；
- 关键指令：`vlsseg3e32.v=28`、`vfmacc=32`、`vfredosum=80`；
- boundary：`runGenericCase<source,target>` 的 test-only lambda；
- shared math：`estimateFused2DCandidate<source,target>` test-support fixture。

因此该证据支持“generic candidate 的 traits-aware RVV 路径确实进入 binary”，不支持
“production public dispatch 已接入泛型点型”。

## 板卡证据

板卡合同为 `Milkv-Jupiter`、5 runs、每 run 20 iterations、5 warmup。摘要：

| case group | median B/A 范围 | `B/A<1` | bucket |
| --- | ---: | ---: | --- |
| `PointXYZ` same-type, 4K/64K/256K | `1.060x-1.091x` | 0/15 | weak_positive |
| `PointXYZI` same-type, 4K/64K/256K | `1.247x-1.496x` | 0/15 | positive |
| `PointNormal` same-type, 4K/64K/256K | `1.040x-1.142x` | 0/15 | weak_positive |
| `PointXYZINormal` same-type, 4K/64K/256K | `1.040x-1.179x` | 0/15 | mixed positive by size; weak_positive/positive per case |
| mixed pairs, 64K | `1.113x-1.328x` | 0/20 | weak_positive/positive per case |

按 case 的代表值必须以
`log/board/generic_xyz_point_types_repeated/summary.md` 为准；本表只做分组摘要。
该结果支持在下一 PI1 中把上述已验证组合列为 production integration candidate，不支持
把所有 traits-allowed 自定义点型或其它 row source 直接纳入。

### Evidence Doctor 异常解释

board Doctor：

```text
Errors=0, Warnings=3, Suggestions=2
```

- `PointXYZINormal -> PointXYZINormal 256K` 的 `min=1.05x`、`median=1.12x`、
  `max=1.22x`：保留长尾，不删除异常值；下一 PI1 继续按具体 point type/size 分桶，
  并保留环境字段缺失风险。
- `PointXYZI -> PointXYZI 4K/64K` 是组内离群：PointXYZI 的 AoS stride/字段 offset
  与其它点型不同，当前按独立 case 报告，不能用组 median 外推。
- `PointNormal -> PointNormal 64K`、`PointXYZINormal -> PointXYZINormal 64K`
  median `1.04x`：属于 near-threshold weak-positive；PI1 必须把它们作为独立
  performance gate，不以“generic overall positive”覆盖。
- 两条 Suggestion 要求扩大 runs 或补环境/静态分析；本阶段已用完计划的 5-run budget，
  decision bucket 对所有 case 均未出现负向 run（`B/A<1=0/5`），因此不无限复跑，
  将它们转为 PI1 的接入后复核动作。

board manifest 同时记录了 source/target point type、row source、size、run count、
binary hash、timer boundary 和 gate；taskset/governor/freq/temperature 仍是
`not_recorded_board_target`，削弱长尾归因能力但没有触发 Error。

## Evidence Registry 与文档套件

generic QEMU 和 board evidence 已通过 topic-local registry target 登记。最终刷新后：

```bash
make -C test-rvv/registration/transformation_estimation_2D evidence_status
```

应保持 `fresh`。生成日志、build 和 registry 按 `summary-only / local-only` 策略处理；
文档只引用 summary、manifest、Doctor 和 asm 摘要路径。

topic-local test support 继续保持：

- `src/` 中 test/bench thin entry；
- `include/te2d.h` 稳定聚合入口；
- `include/te2d.h` 和 TE2D 测试支撑内部头中的 fixtures/reference/candidate；
- `script/` 中 asm、QEMU manifest 和 board summary；
- `log/` 仅本地证据输出。

本阶段没有发现需要迁移旧 `test_support/` 目录或拆分超过 soft limit 的 helper；结构
对齐决策为 `adopted`，generic case registry 和 evidence target 已补齐。

## 矩阵与路线图决策

| candidate | decision |
| --- | --- |
| traits-gated generic xyz ordered-cloud-pair test candidate | `attempted -> evidence-closed / test-only` |
| `PointXYZ` / `PointXYZI` / `PointNormal` / `PointXYZINormal` same-type | correctness + board representative evidence closed per case |
| four mixed source/target pairs | correctness + board representative evidence closed per case |
| source-indexed / dual-indexed / correspondence | unchanged `attempted / diagnostic-only` |
| generic production dispatch | `deferred -> PI1 plan created; no production diff in Phase 070` |
| `Scalar=double` RVV | `deferred` |

## Continue / Stop Decision

本阶段没有命中真实停止条件，已完成计划内 generic evidence bundle。下一阶段不是
`ready_for_review`，而是：

```text
080-generic-xyz-production-integration-plan
```

该阶段只冻结 production integration 的候选范围、fallback/dispatch 合同和 PI2-PI5
证据计划。进入实际 production dispatch 修改前，必须由后续 phase 按该计划补接入后的
correctness、fallback、asm、board performance、Evidence Doctor 和文档刷新；本阶段的
test-only evidence 不能替代接入后证据。

## 指令与偏好

本轮遵循中文优先、test-rvv/diagnostic 详细中文注释、production 注释克制、
`summary-only`、默认不提交、不修改 `.agents`、保留无关 dirty paths 的冻结偏好。
加载并影响本阶段决策的指令来源包括：

- `AGENTS.md`
- `.agents/config/defaults.yaml`
- `.agents/skills/rvv-workflow/SKILL.md`
- `.agents/skills/rvv-workflow/references/short-prompt-entry.zh.md`
- `.agents/skills/rvv-workflow/references/worker-quality-gates.zh.md`
- `.agents/skills/rvv-test/references/optimization-phase-loop.zh.md`
- `.agents/skills/rvv-test/references/evidence-doctor.zh.md`
- `.agents/skills/rvv-test/references/registration-topic-evidence.zh.md`
- 当前 Phase 060 result、Phase 070 plan 和 current Handoff。

`agent_asset_feedback`：本轮未发现需要修改 agent instructions 的可复用缺口，继续
`report-only`。
