# transformation_estimation_2D Phase Loop

本目录记录 `test-rvv/registration/transformation_estimation_2D` 的可恢复优化阶段。这里的
`README.zh.md` 只作为当前入口和提交前导航；历史脉络集中到
[`history.zh.md`](history.zh.md)，逐阶段事实仍以各 phase 的 `plan.zh.md` /
`result.zh.md`、[`optimization-matrix.zh.md`](optimization-matrix.zh.md) 和 evidence summary
（证据摘要）为准。

## 当前恢复入口

当前最新完成阶段是 `112-source-indexed-pointxyzi-adoption-closeout`。该阶段按用户确认把
Phase 110 source-indexed exact `PointXYZI -> PointXYZI` positive production probe 写成
adopted / retained，并完成接入后文档、registry 和提交边界同步。

上一阶段 `111-source-indexed-normal-negative-case-investigation` 只审计
Phase 106 的 source-indexed generic Normal 类负向 case，不修改 production gate。结论为：
`PointNormal` / `PointXYZINormal` 相关 source-indexed generic widening 仍不能接入生产，
Phase 106 的 `PointNormal->PointNormal 256K` 仍以 `7/20` below-1 和 Doctor Error 为主证据。

Phase 110 用于复核 source-indexed exact `PointXYZI -> PointXYZI` RVV dispatch 是否可以作为新的
有界生产候选；Phase 112 已按用户确认把它转为 `adopted / retained`。Phase 110
独立 20-run board repeated 已生成：4K / 64K / 256K median B/A 为
`3.979x / 3.547x / 3.602x`，三个规模 `B/A<1=0/20`，Board Doctor `0/3/0`。结论支持保留
当前 exact gate，但不扩大到 source-indexed generic widening。

Phase 109 是已完成的 dual-indexed exact 方差复核：64K / 256K 稳定 positive，4K median
positive 但有 1/20 below-1 和长尾 Warning，因此保留已接入 exact dispatch，同时继续保留 4K caveat。

Phase 108 是已完成的提交前文档整理；它不改变 Phase 107 的生产结论。
Phase 107 production patch（生产补丁）已完成接入后 correctness（正确性）、
QEMU smoke（QEMU 小型验证）、asm attribution（反汇编归属）、board repeated（板卡重复测试）、
Evidence Doctor（证据体检）、registry（证据登记表）和长期 `doc-rvv` 同步。

当前 production patch 保留：

| row source | 当前状态 | 证据边界 |
| --- | --- | --- |
| ordered-cloud-pair | adopted / retained | traits-gated PointXYZ-like generic，Phase 107 board 16/16 positive。 |
| source-indexed-cloud-pair | adopted / retained | exact `PointXYZ -> PointXYZ`，Phase 107 public board 4K/64K/256K 为 `4.157x / 4.814x / 4.615x`。 |
| source-indexed-cloud-pair | adopted / retained | exact `PointXYZI -> PointXYZI`，Phase 110 public board 4K/64K/256K 为 `3.979x / 3.547x / 3.602x`；Phase 112 已采纳。 |
| dual-indexed-cloud-pair | adopted / retained | exact `PointXYZ -> PointXYZ`，Phase 109 20-run family A/B 为 `1.080x / 1.661x / 1.646x`；4K 有 `1/20` below-1 和长尾 caveat。 |

当前 production patch 不保留：

- source-indexed generic widening：Phase 103/104 仍是 guarded probe；Phase 106 20-run public
  variance 为 negative，不能 clean-adopt。Phase 111 的 Normal 类负向归因记录为 48B AoS stride
  下 source indexed gather、target strided load 和通用 load3 多取 `z` 的访存形态风险；用户已决定
  不再推进 2D-only load2、exact Normal specialization、selected `x/y` materialization 或
  chunk/profile ablation。
- correspondence direct RVV dispatch：Phase 107 试接入后 family A/B negative，已退回到标量
  `ConstCloudIterator` 路径。
- dual-indexed / correspondence generic point-type production dispatch：Phase 100 / 101 诊断为
  negative。

## 阅读路径

| 需求 | 入口 |
| --- | --- |
| 当前生产补丁范围和证据链 | `../transformation_estimation_2D-evaluation.zh.md`、`../../../../doc-rvv/registration/transformation_estimation_2D-RVV.zh.md` |
| 当前 Phase 110 PointXYZI probe 结果 | [`110-source-indexed-pointxyzi-exact-public-probe/result.zh.md`](110-source-indexed-pointxyzi-exact-public-probe/result.zh.md) |
| Phase 112 PointXYZI 采纳收尾 | [`112-source-indexed-pointxyzi-adoption-closeout/result.zh.md`](112-source-indexed-pointxyzi-adoption-closeout/result.zh.md) |
| Phase 111 Normal 类负向样本审计 | [`111-source-indexed-normal-negative-case-investigation/result.zh.md`](111-source-indexed-normal-negative-case-investigation/result.zh.md) |
| Phase 109 dual-indexed 20-run variance 结果 | [`109-dual-indexed-exact-public-variance/result.zh.md`](109-dual-indexed-exact-public-variance/result.zh.md) |
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

当前已知结果：Phase 110 接入后 correctness Std/RVV `84/84` pass，
source-indexed PointXYZI public QEMU Doctor `0/0/0`，board Doctor `0/3/0`，`evidence_status`
fresh。Phase 110 board 20-run summary 路径为
`log/board/source_indexed_pointxyzi_public_phase110_repeated/summary.md`。

## 提交边界

本次提交采用 topic-only（仅主题）策略：

- 提交 production patch、测试调整、topic-local docs、压缩后的 phase index / history /
  optimization matrix、必要关键 phase 文档和长期 `doc-rvv`。
- 不默认提交所有未跟踪 phase `plan/result` 详文；历史事实以 `history.zh.md`、matrix、roadmap、
  evaluation 和 `doc-rvv` 保持可恢复，确需保留单个 phase 详文时再显式加入提交候选。
- 本次不提交 `log/**`；文档只保留 run label、summary / manifest / Evidence Doctor 路径和 registry
  freshness 结果，后续需要 evidence logs 时再单独审计脱敏和提交边界。
- 不提交本地 build 输出、私有 `config.mk`、raw board logs、私有地址或未被文档引用的临时日志。
- 不自动回滚 Phase 091 adopted patch，不把 Phase 103/104 guarded probe 写成 adopted；
  Phase 110 / 112 `PointXYZI -> PointXYZI` 只写 exact adopted，不写成 generic adoption。

## 停止规则

当前 topic 已完成 Phase 112 adoption closeout。Phase 110 exact `PointXYZI -> PointXYZI`
已按用户确认采纳；generic / Normal / correspondence 方向仍不接入。

如继续优化，默认再考虑：

- source-indexed Normal generic widening：Phase 111 已审计为不建议接入；若继续必须另开 exact
  Normal bounded probe 或 profiling phase。
- correspondence new bounded candidate：只有新同边界候选和完整证据链才能恢复。
- point-type inventory expansion：仅在需要把未逐类型上板的 traits 点型写成覆盖范围时另开 phase。
