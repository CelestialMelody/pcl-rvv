# Phase 080 result: public compute profile audit

## 当前结论

本阶段完成 public compute profile audit（公开入口性能剖析审计）。板卡 repeated benchmark（重复性能测试）
显示未改 production（生产源码）的公开入口在 RVV build 下有稳定正向：

- `public_gasd_shape_compute`：5-run median `1.220x`，range `1.210x-1.220x`，checksum 一致，Evidence Doctor
  `0E/0W/2S`。
- `public_gasd_color_compute`：5-run median `1.110x`，range `1.100x-1.120x`，checksum 一致，Evidence Doctor
  `0E/0W/2S`。

这个结果说明完整公开入口在当前 RISC-V / RVV build 环境下有可测的正向 build-level profile signal
（构建层级性能信号）。它不推翻 Phase 050 / Phase 060 对当前手写 staged shape family（分阶段暂存形态）的负向结论，
也不能作为 production patch（生产补丁）采纳证据。本阶段没有修改
`features/include/pcl/features/impl/gasd.hpp`。

## 计划执行回填

| action | status | evidence / command | result |
| --- | --- | --- | --- |
| P1 shape public compute repeated board | done | `make -C test-rvv/features/gasd board_repeated REPEATED_BOARD_TAG=phase080_public_shape_compute ... REPEATED_BOARD_CASE_NAME=public_gasd_shape_compute` | 5-run summary 已生成 |
| P2 shape Evidence Doctor | done | `make -C test-rvv/features/gasd evidence_doctor_repeated REPEATED_BOARD_TAG=phase080_public_shape_compute ...` | `0E/0W/2S` |
| P3 color public compute repeated board | done | `make -C test-rvv/features/gasd board_repeated REPEATED_BOARD_TAG=phase080_public_color_compute REPEATED_BOARD_REMOTE_TAG=phase080_public_color_compute ... REPEATED_BOARD_CASE_NAME=public_gasd_color_compute` | 5-run summary 已生成 |
| P4 color Evidence Doctor | done | `make -C test-rvv/features/gasd evidence_doctor_repeated REPEATED_BOARD_TAG=phase080_public_color_compute ...` | `0E/0W/2S` |
| P5 文档回填 | done | 本文、README、phase index、roadmap、matrix | 默认恢复动作仍为用户审查 |

## 证据摘要

| case | role | board summary | Evidence Doctor | interpretation |
| --- | --- | --- | --- | --- |
| `public_gasd_shape_compute` | production-public profile | `log/board/repeated_phase080_public_shape_compute/summary.md` | `log/board/repeated_phase080_public_shape_compute/evidence_doctor.md` | 稳定正向，但 production 未改，不能归因到当前手写 candidate |
| `public_gasd_color_compute` | production-public profile | `log/board/repeated_phase080_public_color_compute/summary.md` | `log/board/repeated_phase080_public_color_compute/evidence_doctor.md` | 稳定弱正向，默认 color interpolation 是 `INTERP_NONE`，不能外推到 quadrilinear |

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | production-public profile，不是 production patch evidence |
| A/B boundary | public benchmark wrapper；两侧 production source 相同，RVV build 可包含编译器 / Eigen / 构建宏差异 |
| 当前决策问题 | 是否值得继续手写 RVV staged shape family，或是否需要另开 profile-guided family |
| diagnostic 是否可外推到 production | 只能外推为公开入口有 build-level 正向信号；不能外推为当前手写 staged shape family 可采纳 |
| comparison-boundary / baseline mismatch 风险 | yes；Phase 060 比较的是 test-only staged helper，Phase 080 比较的是未改 production 的 public build |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 仍允许，但需要用户显式授权 PI1 scope 和 production direct 证据计划 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes；本阶段不进入 clean adoption |

## 对后续优化价值的判断

Phase 080 不支持继续沿当前 staged shape family 做 production patch：

- Phase 050 / 060 已证明手写 projection / trilinear staging 一旦接近 Eigen histogram write 边界就稳定退化。
- Phase 080 的 public 正向来自未改 production 的 build-level 差异，不能证明手写 staged helper 可以穿透生产边界。
- color public compute 只有 `1.110x` median，且默认 `color_interp_` 是 `INTERP_NONE`；它不足以默认启动
  `INTERP_QUADRILINEAR` 手写 RVV phase。

仍可考虑的下一步只有 profile-guided（性能剖析驱动）的新 topic 内 follow-up：

| option | value | recommendation |
| --- | --- | --- |
| production segment profile | 量化 alignment transform、`transformPointCloud`、normalization、sample loop、histogram write、copy 的占比 | 只有用户想继续深挖 GASD 时再做 |
| bounded production probe | 在 PI1 scope 下真实接入很小 production path 并做 direct tests | 不建议默认做；需要显式授权 |
| color quadrilinear follow-up | 验证 `INTERP_QUADRILINEAR` 的 hue 维度插值和写回 | 只有用户或 workload profile 指定该模式时再做 |

## Continue / stop decision

`continue_stop_decision`：stop for user review。

`stop_condition_hit`：当前 staged shape family 仍不建议继续；Phase 080 已补公开入口 profile，结果没有给出可自动推进的
新手写 RVV candidate。板卡可用，工具未阻塞。

`next_phase_default`：`stop_for_user_review_no_production_closeout`。

`reopen_condition`：用户明确要求继续 GASD 深挖时，建议先开 production segment profile phase，而不是直接接入当前
staged shape helper。

## Doc suite closeout audit

| area | current shape scan | quality bar | decision | evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | `README.zh.md` 列出当前结论、phase 入口、summary / Doctor 路径和提交边界 | topic navigation 需要能恢复当前 EvidenceDecision 和证据白名单 | adopted | Phase 000-080 路径已在 README 中列出 | none |
| testing overview | `README.zh.md#常用命令` 合并说明 correctness、QEMU、asm、board repeated 和 Doctor target | 当前 target 数量有限，可以合并但必须说明边界 | adopted | `Makefile`、`board.mk` 和 phase result 均引用相同 target | none |
| correctness tests | `doc/gasd-evaluation.zh.md#当前诊断证据链` 合并说明 12 个 gtest 家族 | correctness role 需说明输入、被测路径和证明范围 | adopted | `run_qemu_smoke` Std/RVV 均 12/12 通过 | none |
| benchmark and evidence | `README.zh.md#阅读路径`、本 result 和 `log/evidence_registry.json` | 需要 case-filter、board summary、Doctor、manifest 和提交边界 | adopted | Phase 000-080 summary-level 证据已登记；raw logs 排除 | none |
| optimization evidence | `doc/phases/optimization-matrix.zh.md` | candidate family 到 board / Doctor / decision 的映射应集中可查 | adopted | Phase 000-080 均有矩阵行 | none |
| optimization roadmap | `doc/optimization-roadmap.zh.md` | roadmap 必须保留恢复条件和拒绝 / 暂缓路线 | adopted | 默认恢复动作是 `stop_for_user_review_no_production_closeout` | none |
| test-support code map | `doc/gasd-evaluation.zh.md#Traceability Map` | reviewer 应能定位聚合头、internal helper、src 和 script | adopted | Traceability Map 覆盖 production、candidate、test、bench 和 manifest script | none |
| phase index / results | `doc/phases/README.zh.md` + Phase 000-080 plan/result | 多阶段 topic 必须有 phase index 和每阶段 plan/result | adopted | Phase 000-080 均有 plan/result | none |
| evaluation diagnostic | `doc/gasd-evaluation.zh.md` | no-production closeout 需要诊断证据链和 production 接入判断 | adopted | Phase 050/060 负向和 Phase 080 profile-only 边界已写入 | none |
| production topic doc | `doc-rvv/features/gasd-RVV.zh.md` 不存在 | 无 adopted production behavior 时不创建长期生产文档 | not_applicable with evidence | `features/include/pcl/features/impl/gasd.hpp` 无 diff，未进入 PI5 采纳 | none |
| artifact tracking | topic-local docs 为未跟踪提交候选；summary evidence 在 ignored `log/` 下 | closeout 前必须列入 to-be-staged artifact 或排除 | adopted | `log/evidence_registry.json` 明确 summary-only 提交候选和 raw/build 排除 | commit phase 用 `git add` / `git add -f` 精确选择 |

## Commit boundary

本 topic 建议进入 topic-plus-summary-evidence（主题资产 + 摘要证据）提交流程：

- topic assets：`test-rvv/features/gasd/Makefile`、`board.mk`、`include/`、`src/`、`script/` 和 `doc/`。
- queue / doc status：`doc-rvv/library-screening/features/features-retained-candidate-rescreen.zh.md` 中 GASD 行。
- summary evidence：`log/evidence_registry.json` 以及 registry 中列出的 `summary.md`、`evidence_doctor.md`
  和 `evidence_manifest.json`。
- excluded：`build/`、`log/qemu/`、`log/board/*/run_*/`、临时历史 probe 日志和 board raw `.log`。
