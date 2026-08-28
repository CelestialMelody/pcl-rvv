# Phase 080: stick production integration result

## 执行范围

本阶段按 `plan.zh.md` 把 `SampleConsensusModelStick<PointT>` 的三个距离相关公开入口接入 RVV（RISC-V Vector，可变长向量扩展）生产路径：

- `countWithinDistance`
- `selectWithinDistance`
- `getDistancesToModel`

实际范围与计划一致。公开 API（应用程序接口）没有变化；`optimizeModelCoefficients`、`projectPoints` 和 `doSamplesVerifyModel` 保持原实现。生产分流只覆盖 direct indexed `indices_`、`pcl::rvv::RVVXYZAoSFloatLayout<PointT>` 通过、`pcl::index_t` 为 32-bit signed、输入点云规模可用 32-bit byte offset 表达的路径；其它路径自然回退到 Standard helper（标量辅助函数）。

## 动作回填

| action | 状态 | 命令 / 产物 | 结论 |
| --- | --- | --- | --- |
| RED production asm gate | done | `script/check_stick_production_asm.py`；接入前 `make check_production_asm` 失败 | 反汇编门禁能捕捉缺失的 production RVV helper。 |
| PI2 production patch | done | `sample_consensus/include/pcl/sample_consensus/sac_model_stick.h`、`sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp` | 三个公开入口改为模型有效性检查、RVV short-circuit（短路分流）、Standard fallback（标量回退）。 |
| PI3 production direct correctness | done | `make -C test-rvv/sample_consensus/sac_model_stick run_test_compare` | Phase 080 时 Std/RVV 两个构建各 10 个 gtest 通过；Phase 090 后当前 aggregate 为 11/11，并补代表点型 correctness。 |
| PI4 asm attribution | done | `make -C test-rvv/sample_consensus/sac_model_stick clean_bench_rvv && make -C test-rvv/sample_consensus/sac_model_stick check_production_asm` | `countWithinDistanceRVV`、`selectWithinDistanceRVV`、`getDistancesToModelRVV` 均有 RVV 指令归属。 |
| PI4 board repeated | done | `SSH_AUTH_SOCK=/run/user/$(id -u)/keyring/ssh make -C test-rvv/sample_consensus/sac_model_stick collect_production_repeated_board_evidence` | 5-run 板卡重复采集完成，每轮 RVV gtest 10/10 通过，三条 public entry 均稳定正向。 |
| PI5 EvidenceDecision | done | `make -C test-rvv/sample_consensus/sac_model_stick run_production_board_evidence_doctor`、`record_production_board_evidence_state` | Evidence Doctor 为 Errors=0、Warnings=0、Suggestions=0。按用户口径，post-integration board 正收益足以采纳。 |

## Production direct board 结果

证据主路径：

- manifest（证据清单）：`test-rvv/sample_consensus/sac_model_stick/doc/phases/080-stick-production-integration/production-repeated-evidence-manifest.json`
- Evidence Doctor Markdown：`test-rvv/sample_consensus/sac_model_stick/doc/phases/080-stick-production-integration/production-repeated-evidence-doctor.md`
- Evidence Doctor JSON：`test-rvv/sample_consensus/sac_model_stick/doc/phases/080-stick-production-integration/production-repeated-evidence-doctor.json`
- registry（证据登记表）：`test-rvv/sample_consensus/sac_model_stick/log/evidence_registry.json`

| public entry | Std median ms | RVV median ms | speedup min / median / max | RVV 指令数 | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `countWithinDistance` | 1.745164 | 0.420826 | 4.1331x / 4.1729x / 4.1756x | 27 | positive-stable |
| `selectWithinDistance` | 2.137120 | 0.646606 | 3.2085x / 3.3023x / 3.3860x | 36 | positive-stable |
| `getDistancesToModel` | 1.991717 | 0.774076 | 2.5722x / 2.5883x / 2.7872x | 44 | positive-stable |

五轮 checksum 均为 `16723220023521`。板卡输出出现远端 Makefile clock skew（时钟偏移）警告；本阶段每轮都清理并重建 Std/RVV bench，checksum 和 gtest 结果一致，Evidence Doctor 没有报告异常，因此该警告不改变当前 decision bucket。后续若复核绝对耗时，应先修正远端文件时间或重新同步构建目录。

## Evidence Doctor 和 registry

`production-repeated-evidence-doctor.md` 报告：

```text
Errors=0，Warnings=0，Suggestions=0
```

`record_production_board_evidence_state` 已把 manifest、Markdown doctor 和 JSON doctor 三个摘要证据文件登记到 `log/evidence_registry.json`，run label 为 `stick-phase080-production-repeated-board`。在本 result 和长期 `doc-rvv` 引用这些路径后，`production_evidence_status` 应作为 freshness gate（新鲜度检查）重跑。

## Diagnostic 到 production 回填审计

| question | answer |
| --- | --- |
| evidence role | Phase 080 使用 `production_direct`，不再依赖 Phase 000/020/040 的 production-shaped diagnostic 作为采纳证据。 |
| A/B boundary | Std build 与 RVV build 都计时同一个 public overload（公开重载），timer boundary 是公开入口调用本身。 |
| 当前决策问题 | RVV-vs-scalar production adoption（生产采纳）。 |
| diagnostic 是否可外推到 production | 诊断证据只作为进入 Phase 080 的理由。采纳结论来自 Phase 080 post-integration board 数据。 |
| comparison-boundary / baseline mismatch 风险 | 当前 manifest 中 baseline 和 candidate 都是 public overload，checksum 一致，未发现边界错配。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段 production direct 为 positive-stable，未触发弱 / 负 / 中性 / 不稳定处理。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前 stick 没有已采纳的替代 RVV family；本阶段不是 RVV-family-selection（实现族选择），无需额外 RVV-vs-RVV A/B。 |

## Optimization matrix 更新

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production-stick-three-entry-rvv | direct indexed `indices_` | traits-gated xyz AoS `PointT`；生产证据为 `PointXYZ`、`Eigen::VectorXf`、float xyz AoS | `countWithinDistance`、`selectWithinDistance`、`getDistancesToModel` public entry | done: `run_test_compare` Std/RVV 各 10/10；非 RVV 构建自然使用 Standard helper | done: production public 5-run board | done: count/select/getDistances median 4.1729x / 3.3023x / 2.5883x | done: 三个 production RVV helper 均归属 RVV 指令 | done: 0/0/0 | adopted | `090-stick-point-type-expansion` 可作为后续扩展，但需要新 phase scope。 |

## Doc-suite parity closeout

| area | current shape scan | quality bar / supplemental calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | 已列 topic-local 文档、Phase 080 和 production 证据白名单。 | README 只承担入口导航和常用命令。 | adopted | `README.zh.md` | none |
| testing overview | 已区分 correctness aggregate、细分 alias、board repeated、doctor / registry 和 production direct。 | `doc-suite-quality-bar.zh.md` 要求 target 粒度审计来自当前 topic。 | adopted | `doc/testing-overview.zh.md` | none |
| correctness tests | 当前 11 个 gtest 包含 candidate 回归、public production direct 语义和代表点型 correctness。 | 每个测试族说明输入、断言和边界。 | adopted | `doc/correctness-tests.zh.md` | none |
| benchmark and evidence | bench 只输出三条 public entry，Phase 080 manifest 是当前性能证据。 | 性能结论来自 board，不使用 QEMU timing。 | adopted | `doc/benchmark-and-evidence.zh.md` | none |
| optimization evidence | 已把 test-only candidate 和 production adopted family 分层。 | candidate 取舍和生产采纳分开。 | adopted | `doc/optimization-evidence.zh.md` | none |
| test-support code map | 已列 production helper、test source、bench source、manifest script 和 evidence output。 | 复杂 topic 需要 Traceability Map。 | adopted | `doc/test-support-code-map.zh.md` | none |
| evaluation | 已更新 production closeout、证据链和文档归属。 | S2/S11 决策审计主归属。 | adopted | `doc/sac_model_stick-evaluation.zh.md` | none |
| production topic doc | 新增正式长期文档。 | 只保存采纳后的 production 行为、证据链和长期边界。 | adopted | `doc-rvv/sample_consensus/sac_model_stick-RVV.zh.md` | none |
| phase suite | Phase 080 plan/result 和 optimization matrix 已更新。 | phase result 保存本阶段事实和恢复动作。 | adopted | `doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md` | none |
| artifact tracking | 新增 doc / test / script 和 summary 证据均在当前 topic 扫描范围内，但多数仍是 untracked（未跟踪）状态；`log/evidence_registry.json` 被 `test-rvv/.gitignore` 的 `**/log/**` 忽略。 | 提交前使用路径限定 `git status --short --untracked-files=all`，再精确 stage（加入暂存区）当前 topic 产物；ignored registry 和需要提交的 summary evidence 必须用 `git add -f`。 | pending-submit-staging | reviewer artifact tracking finding；当前未进入 commit phase | 提交前列出 allowed files，并排除 raw board logs、QEMU logs、build 输出和无关 topic。 |

## Continue / stop decision

Phase 080 当前范围已闭合，并按用户给出的采纳口径进入 `adopted production behavior`。当前 topic 内仍有一个有价值但更宽的后续方向：`090-stick-point-type-expansion`，用于把 `PointXYZ` 代表性证据扩展到 `PointXYZI`、RGB/RGBA 和 normal-like xyz AoS 点型。该方向会扩大点类型证据范围，需要新 phase plan、专门 correctness、fallback、asm、board repeated 和 Evidence Doctor；本阶段不把它写成已经证明。

当前默认停止条件：本阶段矩阵已闭合，剩余动作属于新的 point type expansion scope。本轮可以停在 production closeout / review 边界。
