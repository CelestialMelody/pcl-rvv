# transformation_estimation_2D Phase Loop

本目录记录 `test-rvv/registration/transformation_estimation_2D` 的可恢复优化阶段。这里的
`README.zh.md` 只作为当前入口和提交前导航；历史脉络集中到
[`history.zh.md`](history.zh.md)，逐阶段事实仍以各 phase 的 `plan.zh.md` /
`result.zh.md`、[`optimization-matrix.zh.md`](optimization-matrix.zh.md) 和 evidence summary
（证据摘要）为准。

## 当前恢复入口

当前正在执行阶段是 `114-test-support-responsibility-split-and-agent-rule-update`。该阶段不修改
production source，只按 responsibility-first（职责优先）规则拆分 TE2D test support（测试支撑）
内部头文件：`include/te2d.h` 继续是稳定聚合入口，内部职责落到 `include/impl/te2d_*.hpp`。
本阶段只证明测试支撑结构和 correctness / QEMU smoke（正确性 / QEMU 小型验证）无语义回归；
不新增 RVV 性能优化、不跑板卡性能、不扩大 production gate。

上一阶段 `113-doc-rvv-closeout-and-log-hygiene` 已完成并提交为
`4ba8e63fd registration: clean RVV 2D docs and log tracking`。Phase 113 不修改 production source，
只补齐 production `doc-rvv` closeout gate，并把已被 Git 跟踪的 topic generated logs 从索引中移除，
保留本地 evidence files，由 `test-rvv/.gitignore` 接管。

上一阶段 `112-source-indexed-pointxyzi-adoption-closeout` 已完成。该阶段按用户确认把
Phase 110 source-indexed exact `PointXYZI -> PointXYZI` positive production probe 写成
adopted / retained。对应生产源码已提交为
`d40e67467 registration: adopt RVV 2D source-indexed PointXYZI path`，不要自动 amend（修补提交）、
回滚或把 Phase 103/104 guarded probe 写成 adopted。

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
| Phase 114 测试支撑职责拆分 | [`114-test-support-responsibility-split-and-agent-rule-update/plan.zh.md`](114-test-support-responsibility-split-and-agent-rule-update/plan.zh.md) |
| Phase 113 production doc closeout / log hygiene | [`113-doc-rvv-closeout-and-log-hygiene/plan.zh.md`](113-doc-rvv-closeout-and-log-hygiene/plan.zh.md) |
| Phase 111 Normal 类负向样本审计 | [`111-source-indexed-normal-negative-case-investigation/result.zh.md`](111-source-indexed-normal-negative-case-investigation/result.zh.md) |
| Phase 109 dual-indexed 20-run variance 结果 | [`109-dual-indexed-exact-public-variance/result.zh.md`](109-dual-indexed-exact-public-variance/result.zh.md) |
| 阶段历史为什么这么多 | [`history.zh.md`](history.zh.md) |
| 候选族、row source、点型和证据状态 | [`optimization-matrix.zh.md`](optimization-matrix.zh.md) |
| 后续还能尝试什么 | `../optimization-roadmap.zh.md` |
| 测试、bench、QEMU、board 和 registry 入口 | `../testing-overview.zh.md`、`../benchmark-and-evidence.zh.md` |
| 当前 Handoff Packet（交接包） | `../../../../tmp/rvv-work-logs/registration/transformation_estimation_2D/current-handoff/current-handoff.zh.md` |

## 当前提交状态

`d40e67467` 已提交 production `PointXYZI` exact gate，`4ba8e63fd` 已提交 Phase 113 doc/log hygiene。
当前 Phase 114 提交前仍需确认：

```bash
make -C test-rvv/registration/transformation_estimation_2D evidence_status
git diff --check -- test-rvv/registration/transformation_estimation_2D doc-rvv/registration/transformation_estimation_2D-RVV.zh.md tmp/rvv-work-logs/registration/transformation_estimation_2D
```

当前已知基线：Phase 114 拆分后 `run_test_compare` 和 `record_qemu_correctness_state`
均已重新编译 Std/RVV 两侧，均为 `84/84` pass。`evidence_status` 为 fresh，YAML parse
输出 `ok`，旧入口引用扫描和 `git diff --check` 均通过。提交前还需做 staged set 审计。
Phase 110 board 20-run summary 路径仍为 `log/board/source_indexed_pointxyzi_public_phase110_repeated/summary.md`。

## 提交边界

Phase 114 Commit B 采用 topic-only（仅主题）策略：

- 提交 TE2D test support 职责拆分、Phase 114 plan/result、phase index、roadmap、optimization matrix
  和 topic-local code map / README / evaluation 同步。
- 不提交新的 production patch；`d40e67467` 已保留 adopted source-indexed exact `PointXYZI` gate。
- 不提交 `.agents`；workflow rule update 已由独立 Commit A 完成。
- 不提交 `tmp/rvv-work-logs/**`、`log/**`、`build/**`、raw/generated logs 或无关 SVD / surface /
  library-screening dirty 文件。
- 不提交本地 build 输出、私有 `config.mk`、raw board logs、私有地址或未被文档引用的临时日志。
- 不自动回滚 Phase 091 adopted patch，不把 Phase 103/104 guarded probe 写成 adopted；
  Phase 110 / 112 `PointXYZI -> PointXYZI` 只写 exact adopted，不写成 generic adoption。

## 停止规则

当前 topic 正在完成 Phase 114 test-support structure closeout。完成后默认恢复动作是 reviewer 检查
Commit B；不自动创建 Normal、correspondence、generic widening 或其它新 RVV 性能优化 phase。
