# transformation_estimation_2D Phase Loop

本目录记录 `test-rvv/registration/transformation_estimation_2D` 的可恢复优化阶段。这里的
`README.zh.md` 只作为当前入口和提交前导航；历史脉络集中到
[`history.zh.md`](history.zh.md)，逐阶段事实仍以各 phase 的 `plan.zh.md` /
`result.zh.md`、[`optimization-matrix.zh.md`](optimization-matrix.zh.md) 和 evidence summary
（证据摘要）为准。

## 当前恢复入口

当前入口是 `108-phase-doc-compaction`，任务是提交前文档整理；它不改变 Phase 107 的生产结论。
Phase 107 production patch（生产补丁）已完成接入后 correctness（正确性）、
QEMU smoke（QEMU 小型验证）、asm attribution（反汇编归属）、board repeated（板卡重复测试）、
Evidence Doctor（证据体检）、registry（证据登记表）和长期 `doc-rvv` 同步。

当前 production patch 保留：

| row source | 当前状态 | 证据边界 |
| --- | --- | --- |
| ordered-cloud-pair | adopted / retained | traits-gated PointXYZ-like generic，Phase 107 board 16/16 positive。 |
| source-indexed-cloud-pair | adopted / retained | exact `PointXYZ -> PointXYZ`，Phase 107 public board 4K/64K/256K 为 `4.157x / 4.814x / 4.615x`。 |
| dual-indexed-cloud-pair | adopted / retained | exact `PointXYZ -> PointXYZ`，Phase 107 family A/B 为 `1.085x / 1.691x / 1.678x`，4K 有 `1/5` below-1 caveat。 |

当前 production patch 不保留：

- source-indexed generic widening：Phase 103/104 仍是 guarded probe；Phase 106 20-run public
  variance 为 negative，不能 clean-adopt。
- correspondence direct RVV dispatch：Phase 107 试接入后 family A/B negative，已退回到标量
  `ConstCloudIterator` 路径。
- dual-indexed / correspondence generic point-type production dispatch：Phase 100 / 101 诊断为
  negative。

## 阅读路径

| 需求 | 入口 |
| --- | --- |
| 当前生产补丁范围和证据链 | `../transformation_estimation_2D-evaluation.zh.md`、`../../../../doc-rvv/registration/transformation_estimation_2D-RVV.zh.md` |
| 阶段历史为什么这么多 | [`history.zh.md`](history.zh.md) |
| 候选族、row source、点型和证据状态 | [`optimization-matrix.zh.md`](optimization-matrix.zh.md) |
| 后续还能尝试什么 | `../optimization-roadmap.zh.md` |
| 测试、bench、QEMU、board 和 registry 入口 | `../testing-overview.zh.md`、`../benchmark-and-evidence.zh.md` |
| 当前 Handoff Packet（交接包） | `../../../../tmp/rvv-work-logs/registration/transformation_estimation_2D/current-handoff/current-handoff.zh.md` |

## 当前提交状态

提交前已再次确认：

```bash
make -C test-rvv/registration/transformation_estimation_2D evidence_status
git diff --check -- registration/include/pcl/registration/impl/transformation_estimation_2D.hpp test-rvv/registration/transformation_estimation_2D tmp/rvv-work-logs/registration/transformation_estimation_2D doc-rvv/registration/transformation_estimation_2D-RVV.zh.md
```

当前已知结果：`record_qemu_correctness_state` 重新登记后 Std/RVV `84/84` pass，
`evidence_status` fresh，`git diff --check` pass。

## 提交边界

本次提交采用 topic-only（仅主题）策略：

- 提交 production patch、测试调整、topic-local docs、phase docs 和长期 `doc-rvv`。
- 本次不提交 `log/**`；文档只保留 run label、summary / manifest / Evidence Doctor 路径和 registry
  freshness 结果，后续需要 evidence logs 时再单独审计脱敏和提交边界。
- 不提交本地 build 输出、私有 `config.mk`、raw board logs、私有地址或未被文档引用的临时日志。
- 不自动回滚 Phase 091 adopted patch，不把 Phase 103/104 guarded probe 写成 adopted。

## 停止规则

当前 topic 已进入提交后状态；如继续优化，默认新建独立 phase：

- dual-indexed exact 20-run variance：用于更严格处理 4K caveat。
- correspondence new bounded candidate：只有新同边界候选和完整证据链才能恢复。
- source-indexed generic negative-case investigation：只追查 Phase 106 negative case，不扩大生产 gate。
