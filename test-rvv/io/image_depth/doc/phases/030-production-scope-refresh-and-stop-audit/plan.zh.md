# Phase 030 计划：production scope refresh and stop audit

## 阶段意图和边界

本阶段只刷新 topic-local production integration plan（生产接入计划）和恢复队列，不修改 `io/src/image_depth.cpp` 或 `io/src/openni_camera/openni_depth_image.cpp`。目标是把 phase 020 的 downsample `vlse16` 诊断结果并入 PI1 生产接入边界，并复核 remaining candidates（剩余候选）是否仍有当前授权范围内的未阻塞动作。

本阶段覆盖：

- 修订 `010-production-integration-plan/plan.zh.md`，把 contiguous 与 downsample 两类 `partial-production-candidate` 分清。
- 审计 `fillDepthImageRaw()` 是否应该在未获 production 授权前继续做 RVV 诊断。
- 审计 OpenNI legacy parity（旧 OpenNI 同构入口对齐）是否属于当前 `io/src/image_depth.cpp` topic 的可继续动作。
- 更新 roadmap、matrix、phase README 和 evaluation 的默认恢复动作。

本阶段不覆盖：

- PI2 production patch（生产补丁）。
- production direct tests（真实生产入口测试）。
- 新增 raw path RVV helper。
- 修改 OpenNI legacy production source。

## 当前状态清单

| 项目 | 当前状态 | 路径 |
| --- | --- | --- |
| phase 000 contiguous | `partial-production-candidate` | `000-current-state-and-diagnostic-scaffold/result.zh.md` |
| phase 020 downsample | `partial-production-candidate` with long-tail warnings | `020-stride-load-downsample-diagnostic/result.zh.md` |
| PI1 plan | 只覆盖 contiguous，需要刷新 | `010-production-integration-plan/plan.zh.md` |
| production source | 未修改 | `io/src/image_depth.cpp` |
| Evidence registry | fresh | `log/evidence_registry.json` |

## 审计问题

| candidate / action | 审计问题 | 本阶段判据 |
| --- | --- | --- |
| PI1 scope refresh | downsample 是否应纳入后续 bounded production probe | 若 phase 020 correctness、asm、board 和 doctor 已闭合，则纳入候选，但仍要求用户授权 PI2。 |
| `fillDepthImageRaw()` | 是否当前继续做 raw RVV 诊断 | 若源码主入口多为 full-size tight memcpy，且缺少 profile 指向非 memcpy raw loop，则保持 deferred / profile-gated。 |
| OpenNI legacy parity | 是否当前继续做 legacy 诊断 | 若涉及另一个 production file 且应在通用 `image_depth.cpp` production direct 稳定后再复用，则保持 deferred。 |
| stop condition | 是否可以继续不碰 production | 若剩余 high-priority 动作都需要 production 授权，记录 stop condition，不把 topic 标成 complete。 |

## 文档动作

| 动作 | 产物 | 完成判据 |
| --- | --- | --- |
| 修订 PI1 | `010-production-integration-plan/plan.zh.md` | 明确 contiguous 与 downsample gate、fallback、PI4 repeated 证据要求和 PI5 用户检查点。 |
| 更新 phase suite | `README.zh.md`、`optimization-matrix.zh.md` | 030 进入 complete/stop-audit 状态，剩余动作不伪装成 done。 |
| 更新 roadmap / evaluation | `doc/optimization-roadmap.zh.md`、`doc/image_depth-evaluation.zh.md` | 默认恢复动作为“用户授权后 PI2”；raw / OpenNI 保持 evidence-gated deferred。 |

## 验证计划

本阶段不改 C++ 行为，fresh verification（新鲜验证）以文档和证据一致性为主：

- `make evidence_status`
- `git diff --check -- test-rvv/.gitignore test-rvv/io/image_depth doc-rvv/library-screening/io/io-function-evaluation-queue.zh.md`
- 尾随空白扫描
- 路径限定 `git status --short --untracked-files=all`

## 继续 / 停止条件

若 PI1 刷新后剩余 high-priority 动作都需要修改 production source，本阶段应输出 `turn_stop_deferred with stop_condition_hit`：需要用户明确授权 PI2。若审计发现 test-rvv 内仍有能改变生产接入决策的未阻塞证据缺口，则继续创建下一 phase。
