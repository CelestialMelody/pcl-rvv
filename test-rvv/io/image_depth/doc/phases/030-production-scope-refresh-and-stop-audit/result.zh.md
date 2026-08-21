# Phase 030 结果：production scope refresh and stop audit

## 执行范围

本阶段只更新 topic-local 文档，没有修改 production（生产源码）或 test-rvv C++ 行为。阶段目标是把 phase 020 的 downsample `vlse16` 结果并入 PI1，并审计剩余候选是否仍有当前授权范围内的高优先级未阻塞动作。

| 范围 | 状态 | 说明 |
| --- | --- | --- |
| PI1 scope refresh | done | `010-production-integration-plan/plan.zh.md` 已从 contiguous-only 刷新为 contiguous + integer downsample。 |
| raw path audit | done | `fillDepthImageRaw()` 当前保持 profile-gated deferred，不作为本轮未授权默认动作。 |
| OpenNI legacy audit | done | `openni_camera/openni_depth_image.cpp` 是同构 legacy file，但涉及另一个 production file；等待通用 `image_depth.cpp` production direct 稳定后再复核。 |
| production patch | not_started | 继续到 PI2 需要用户明确授权修改 `io/src/image_depth.cpp`。 |

## 计划动作回填

| 计划动作 | 状态 | 证据路径 | 结论 |
| --- | --- | --- | --- |
| 修订 PI1 | done | `010-production-integration-plan/plan.zh.md` | PI2 候选现在同时覆盖 contiguous 与整数倍 downsample；PI4 必须保留 downsample long-tail 解释或扩大 runs。 |
| 更新 roadmap / evaluation | done | `doc/optimization-roadmap.zh.md`、`doc/image_depth-evaluation.zh.md` | 默认恢复动作是用户授权后进入 PI2；raw 和 OpenNI 不再作为当前未授权默认动作。 |
| 更新 phase index | done | `doc/phases/README.zh.md` | 030 记录为当前 stop-audit phase，下一步边界可从 phase suite 恢复。 |

## raw path 审计

`io/src/image_depth.cpp::DepthImage::fillDepthImageRaw()` 的 full-size tight row 条件直接调用 `memcpy`。当前上游 `openni2_grabber.cpp`、`openni_grabber.cpp` 和 `oni_grabber.cpp` 对 raw depth 的可见调用多为目标原始分辨率，默认会命中或接近该 fast path（快路径）。非 memcpy 分支只复制 `uint16_t` 并处理 invalid sentinel（无效值哨兵），当前没有 profile（性能剖析）证明它是主成本。

结论：`fillDepthImageRaw()` 不进入当前 production probe，也不在未获 production 授权前继续做 RVV helper。恢复条件是 production probe 完成后，或 profile 指向非 memcpy raw conversion loop。

## OpenNI legacy parity 审计

`io/src/openni_camera/openni_depth_image.cpp` 的 `fillDepthImage()` / `fillDisparityImage()` 与通用 `io/src/image_depth.cpp` 公式同构，区别在 wrapper API、异常宏和 `HAVE_OPENNI` 条件编译。该路径属于 legacy production file（旧生产文件），不是当前 topic 的 production source。

结论：OpenNI legacy parity 保持 deferred。恢复条件是通用 `image_depth.cpp` production direct evidence（真实生产入口证据）稳定后，再决定是否复用同一 helper family 或单独建 legacy parity phase。

## Evidence 与 registry

本阶段没有生成新 benchmark（性能测试）或 Evidence Doctor（证据体检）输入。当前性能结论仍依赖：

- phase 000 contiguous summary：`log/board/repeated_contiguous/summary.md`
- phase 020 downsample summary：`log/board/repeated_downsample/summary.md`
- registry：`log/evidence_registry.json`

验证时应运行 `make evidence_status`，确认这些摘要证据仍为 fresh。

## Continue / Stop Decision

`continue_stop_decision = turn_stop_deferred with stop_condition_hit`。

停止条件不是板卡不可用，也不是证据失败；当前板卡证据可用且 registry 已有 freshness 机制。真实停止条件是下一步 high-priority action 为 PI2 production patch，会修改 `io/src/image_depth.cpp`，需要用户明确授权 production integration loop（生产接入闭环）。

下一默认动作：用户授权 PI2 后，按修订后的 `010-production-integration-plan/plan.zh.md` 修改 `io/src/image_depth.cpp`，并连续推进 PI3-PI5；PI5 无论证据正负都暂停等待用户确认采纳或回滚。
