# Phase 010 Result: production-integration-plan

## 结论

本阶段完成 PI1 production integration plan（生产接入计划）冻结，但未修改 production（生产源码）。当前状态是 `PI1 plan ready / PI2 blocked on production authorization`：若用户明确授权修改 `io/src/openni2_grabber.cpp`，下一轮可按 `plan.zh.md` 进入 PI2-PI5；若未授权，本 topic 不能自行把 diagnostic helper 接入生产。

## 冻结范围

| item | result |
| --- | --- |
| production entry | 仅 `OpenNI2Grabber::convertToXYZPointCloud(const DepthImage::Ptr&)`。 |
| candidate | 仅 `PointXYZ` 同尺寸 depth projection。 |
| fallback | 非 RVV 构建、未命中 gate、小规模 / 异常尺寸仍保留 Std path。 |
| forbidden expansion | RGB/RGBA、IR、mismatch、legacy OpenNI、public API 和公共 RVV wrapper。 |
| production doc | `doc-rvv/io/openni2_grabber-RVV.zh.md` 仍不适用，直到 production patch 通过 PI5 且用户确认采纳。 |

## 证据承接

PI1 计划使用 Phase 000 的 production-shaped diagnostic evidence（生产形态诊断证据）作为“可以尝试窄生产探针”的依据，不把它写成 production evidence。Phase 000 板卡数据为 `xyz_depth_full_640x480` median 1.21x；PI4 必须重新跑 production direct repeated board，不得复用该数值作为最终生产结论。

## 新增恢复入口

| target | result |
| --- | --- |
| `make generate_board_openni2_grabber_repeated_evidence_manifest` | 可重新生成 Phase 000 diagnostic JSON manifest。 |
| `make run_board_openni2_grabber_evidence_doctor` | 可重新生成 manifest-based Evidence Doctor。 |
| `make record_board_openni2_grabber_repeated_evidence_state` | 可登记 Phase 000 repeated summary / Doctor 的 freshness。 |
| `make evidence_status` | 当前检查为 fresh。 |
| `make check_openni2_grabber_rvv_asm` | 可检查 Phase 000 diagnostic bench 的关键 RVV 指令。 |

## 停止条件

继续到 PI2 会修改 `io/src/openni2_grabber.cpp`，属于 production patch。根据 RVV workflow，PI2 前需要用户明确授权；当前未获得该授权，因此本阶段合法停止在 PI1 计划完成处。该停止不是证据负向，也不是 topic closeout。

## 下一轮恢复

若用户授权 production patch，直接从 `doc/phases/010-production-integration-plan/plan.zh.md` 的 PI2 草案恢复，先抽 `fillXYZPointCloudStd`，再在 `__RVV10__` 下补 `fillXYZPointCloudRVV`，随后完成 PI3 correctness、PI4 production direct asm / board / Doctor、PI5 用户检查点。
