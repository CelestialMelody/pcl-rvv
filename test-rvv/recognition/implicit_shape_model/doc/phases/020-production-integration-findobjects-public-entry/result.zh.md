# Phase 020: production integration of `findObjects()` public entry result

## 阶段结论

Phase 020 完成。`findObjects()` 中 descriptor-to-cluster nearest assignment 已接入生产源码：
`__RVV10__` 构建通过 `findNearestClusterIndexRVV` 走 RVV（RISC-V Vector，可变长度向量扩展），
非 RVV 构建通过 `findNearestClusterIndexStd` 走原标量语义。公开 API、模型字段、vote 输出和
`trainISM()` 没有变化。

Production direct repeated board（真实生产路径重复板卡测试）显示 weak positive（弱正向）：
`public_find_objects_descriptor_assignment` median `1.060x`，min `1.040x`，max `1.070x`，
`B/A < 1` 为 `0/5`。Evidence Doctor（证据体检）为
`Errors=0 / Warnings=0 / Suggestions=2`。本轮 prompt 明确允许“板卡有收益即可采纳”，因此当前
production patch 进入窄范围 adopted production behavior（已采纳生产行为）。

## 执行事实

| action | result | evidence |
| --- | --- | --- |
| production helper review | done | `recognition/include/pcl/recognition/impl/implicit_shape_model.hpp` |
| topic correctness | passed | `make -C test-rvv/recognition/implicit_shape_model run_test_compare` |
| upstream public-entry correctness | passed | `make -C test-rvv/recognition/implicit_shape_model run_upstream_test_compare` |
| QEMU public-entry smoke debug | passed after setting test feature `KSearch=1` | `run_bench_std ... --case-filter public_find_objects_descriptor_assignment --iterations 1 --warmup-iterations 0` |
| asm gate | passed | `make -C test-rvv/recognition/implicit_shape_model check_ism_rvv_asm` |
| board availability | passed with injected `SSH_AUTH_SOCK` | `SSH_AUTH_SOCK=/tmp/ssh-iEjvVUej0dT1/agent.101441 make ... check_board_ssh` |
| production-direct board repeated | collected 5/5 runs | `log/board/repeated_phase020_public_entry_findobjects_production_direct/summary.md` |
| manifest | generated | `log/board/repeated_phase020_public_entry_findobjects_production_direct/evidence_manifest.json` |
| Evidence Doctor | `Errors=0，Warnings=0，Suggestions=2` | `log/board/repeated_phase020_public_entry_findobjects_production_direct/evidence_doctor.md`、`.json` |
| registry | refreshed after doc closeout | `log/evidence_registry.json` |
| production topic doc | created | `doc-rvv/recognition/implicit_shape_model-RVV.zh.md` |

## 板卡结果

| case | runs | median speedup | min | max | p10 | p90 | B/A < 1 | decision |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `public_find_objects_descriptor_assignment` | 5 | `1.060x` | `1.040x` | `1.070x` | `1.044x` | `1.066x` | `0/5` | weak_positive |

运行参数：

```bash
--case-filter public_find_objects_descriptor_assignment --clusters 184 --descriptors 512 --iterations 80 --warmup-iterations 5
```

checksum policy（校验策略）使用 semantic fingerprint（语义指纹）：
`semantic:public_votes=494:votes_match=True:peak_density_match=True:peak_fingerprint_match=True`。
raw bit checksum 保留为 `7645179244906730525`，用于漂移复核。

## Evidence Doctor 解释

Doctor 无 Error 和 Warning，因此本阶段没有需要降级的异常。两个 Suggestion 不阻塞当前结论：

- `environment_metadata_missing`：缺少 device、taskset、governor、freq、temperature 等环境字段。
- `binary_identity_missing`：缺少 binary hash 或等价二进制身份字段。

处理动作：当前 5-run 方向一致且无长尾反转，保留 weak positive 结论；后续如果扩大范围、出现方向反转或
reviewer 要求复核，应补环境 metadata 和 binary identity 后重跑。

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | production direct |
| A/B boundary | public overload |
| 当前决策问题 | 当前 public RVV path 是否快于当前 public scalar path，并是否采纳当前小型 production patch |
| diagnostic 是否可外推到 production | Phase 010 只能作为进入 PI 的理由；最终结论以 Phase 020 production direct 为准 |
| comparison-boundary / baseline mismatch 风险 | Phase 020 已在 public overload 边界内比较 Std/RVV；仍不覆盖训练和其它点型 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已完成 bounded production probe；结果 weak positive 且无 Error / Warning |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | no；本阶段不是不同 RVV family 选择，没有已有 adopted RVV family 需要替换 |

## Phase scope 与扩展队列

- validated_scope：`findObjects()` public entry 中 descriptor assignment 的当前 board 证据，使用
  `FeatureSize=153`、`pcl::PointXYZ` / `pcl::Normal`、Eigen `VectorXf` / `MatrixXf`、clusters=`184`、
  descriptors=`512`。这只是代表性证据，不是生产门禁。
- unvalidated_scope：`trainISM()`、KMeans、feature estimator 主成本、`calculateSigmas()`、
  `calculateWeights()`、vote density、其它 `FeatureSize`、其它点型 / layout、其它目标硬件。
- point_type_expansion_queue：若要扩大到其它点型或 `FeatureSize`，必须新建 phase，补 public-entry
  correctness、fallback、bench、asm、board repeated 和 Evidence Doctor。
- phase_closeout_boundary：只关闭 `findObjects()` descriptor assignment 的 production direct 条目。

## doc-suite parity 审计

| area | current shape scan | quality bar / supplemental calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | README 已有当前结论、阅读顺序、命令和证据白名单 | `doc-suite-quality-bar.zh.md` | adopted | `README.zh.md` 已刷新 | none |
| testing overview | target 分类含 public-entry correctness / board / registry | target granularity audit | adopted | `doc/testing-overview.zh.md` | none |
| correctness tests | gtest + upstream public-entry target 分层 | correctness role | adopted | `doc/correctness-tests.zh.md` | none |
| benchmark and evidence | case-filter、summary、manifest、Doctor、registry 已列出 | benchmark/evidence role | adopted | `doc/benchmark-and-evidence.zh.md` | none |
| optimization evidence | adopted / deferred candidate 状态已更新 | optimization evidence role | adopted | `doc/optimization-evidence.zh.md` | none |
| test-support code map | production helper、test、bench、script 可定位 | code map role | adopted | `doc/test-support-code-map.zh.md` | none |
| evaluation | production decision、Traceability Map 和 fallback matrix 已更新 | evaluation production role | adopted | `doc/implicit_shape_model-evaluation.zh.md` | none |
| production topic doc | 正式 `doc-rvv` 已创建 | production doc closeout gate | adopted | `doc-rvv/recognition/implicit_shape_model-RVV.zh.md` | none |
| phase index / matrix | Phase 020 结果和 adopted matrix 已记录 | phase suite role | adopted | `doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md` | none |
| artifact tracking | topic docs 和 `doc-rvv` 将作为 topic artifact 审查；raw logs 默认排除 | artifact tracking gate | adopted | `git status --short --untracked-files=all -- <topic paths>` | none |

## Optimization matrix 更新

`production direct findObjects descriptor assignment` 从 `planned` 更新为 `adopted production behavior / narrow`。
`sigma pairwise max-dot`、`vote density Gaussian sum` 和 full `trainISM()` path 保持 deferred 或 not_now；
它们需要新的 phase，不属于 Phase 020 的自动继续范围。

## Continue / Stop Decision

`continue_stop_decision`：停止并整理。停止条件命中 `optimization-phase-loop` 的第 8 条：当前结论已满足
closeout 条件，且剩余方向属于新的入口、训练 path、数学语义或泛型扩展，不是 Phase 020 同 scope 的
未阻塞动作。

`next_phase_default`：`ready_for_review` for current narrow scope。若继续当前 topic，建议单独选择
`030-findobjects-scope-expansion`、`030-sigma-production-shaped-profile` 或
`030-density-math-boundary-audit`，并先写新的 phase plan。
