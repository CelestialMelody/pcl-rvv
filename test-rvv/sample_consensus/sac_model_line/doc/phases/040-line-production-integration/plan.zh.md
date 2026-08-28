# Phase 040 Plan: line production integration

## 阶段意图和边界

本阶段把 Phase 000、010 和 030 已经正向的 `PointXYZ + direct indexed indices_ + float xyz AoS`
诊断候选推进到 production integration loop（生产接入闭环）。本阶段只覆盖
`SampleConsensusModelLine<PointT>` 的三个公开入口：

- `countWithinDistance`
- `selectWithinDistance`
- `getDistancesToModel`

生产范围限定为 RVV 构建、`pcl::rvv::RVVXYZAoSFloatLayout<PointT>` 成立、`pcl::index_t`
为 signed 32-bit、点云规模能用 32-bit byte offset（字节偏移）表达、direct indexed
`indices_` 行来源。其它点型、非 xyz float AoS layout、自定义宽 offset、非 RVV 构建和
其它 sample consensus 模型保持标量路径。

## 当前状态清单

| 项 | 当前事实 |
| --- | --- |
| diagnostic count | Phase 000 board repeated median `4.4569x`，Evidence Doctor 0/0/0。 |
| diagnostic select | Phase 010 board repeated median `3.1010x`，Evidence Doctor 0/0/0。 |
| diagnostic getDistances vfsqrt | Phase 030 board repeated median `3.5208x`，Evidence Doctor 0/0/0。 |
| diagnostic getDistances scalar-sqrt | Phase 020 negative，当前实现族拒绝。 |
| production 源码 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp` 仍是纯标量。 |
| Handoff | `tmp/rvv-work-logs/sample_consensus/sac_model_line/current-handoff/current-handoff.zh.md` 指向 PI1。 |

## PI1 scope

`pi2_scope` 冻结为：

- 入口：`countWithinDistance`、`selectWithinDistance`、`getDistancesToModel`。
- 点型 / layout：traits-gated `RVVXYZAoSFloatLayout<PointT>`，当前 production direct bench
  先用 `PointXYZ` 证明。
- `Scalar`：模型系数仍为 `Eigen::VectorXf`，内部 RVV 计算使用 float；`distances` 输出仍写
  `std::vector<double>`。
- row source：当前公开入口已有的 direct indexed `indices_`。
- forbidden expansion：不修改 public API，不改其它 SAC 模型，不扩到 `Scalar=double`、
  非 xyz 点型、其它 row source 或公共 RVV helper。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 000/010/030 是 production-shaped diagnostic（生产形态诊断）；本阶段要补 production direct（真实生产路径证据）。 |
| A/B boundary | 诊断 A/B 是 test helper；production A/B 将改成 public overload（公开入口）Std/RVV。 |
| 当前决策问题 | 三个正向候选是否在真实 production dispatch 下仍快于标量路径。 |
| diagnostic 是否可外推到 production | 只能作为 PI1 输入。必须新增 production RVV helper、fallback test、production asm 和 board repeated。 |
| comparison-boundary / baseline mismatch 风险 | 有。诊断 helper 在测试派生类中；production 入口会多一层 dispatch 和 fallback gate。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已授权 bounded production probe；若 PI5 证据退化或不稳定，保留 patch 并停在用户检查点。 |
| clean adoption 是否需要 production evidence | 需要。当前 prompt override（提示词覆盖）明确要求板卡接入后有收益即可采纳；若 PI5 production direct 证据 positive-stable，本轮继续创建正式 `doc-rvv` 并写 adopted production behavior。 |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED asm gate | `check_line_production_asm.py` + `make check_production_asm` | 当前无 production RVV helper 时失败。 |
| production patch | `sac_model_line.h` / `impl/sac_model_line.hpp` | 公开入口呈现 model validity check -> RVV short-circuit -> Standard fallback。 |
| production direct correctness | `make -C test-rvv/sample_consensus/sac_model_line run_test_compare` | Std/RVV gtest 全部通过。 |
| production asm | `make -C test-rvv/sample_consensus/sac_model_line check_production_asm` | 三个 production helper 均含预期 RVV 指令。 |
| board production evidence | 新增 production repeated target 后用 `SSH_AUTH_SOCK=<ssh-agent-socket>` 采集 | Std/RVV public rows 使用接入后 board 数据。 |
| Evidence Doctor / registry | manifest、doctor、registry | Errors / Warnings / Suggestions 已解释并登记 fresh。 |

## fallback 矩阵

| fallback gate | 预期行为 |
| --- | --- |
| 非 `__RVV10__` 构建 | 只编译并运行 Standard helper。 |
| 点型不满足 `RVVXYZAoSFloatLayout<PointT>` | public entry 直接落回 Standard helper。 |
| `pcl::index_t` 不是 signed 32-bit | `selectWithinDistance` 落回 Standard；count / getDistances 不 reinterpret 写出 index。 |
| 点云规模超过 `rvvMaxU32ByteOffsetElements<PointT>()` | RVV helper 内部落回 Standard。 |
| model coefficients 无效 | 保持现有公开入口副作用边界：count 返回 0，select 和 getDistances 不改写 output。 |

## 继续 / 停止条件

本阶段默认连续推进 PI2-PI5。只有 production correctness 失败、反汇编不能归属、板卡不可达、
Evidence Doctor Error 无法解释、production direct 性能退化 / 不稳定，或需要扩大到未授权范围时停止。
PI5 完成后按本轮 prompt override 判断：若接入后 board evidence positive-stable，则进入 adopted closeout；若退化或不稳定，保留 patch 并停在用户检查点。
