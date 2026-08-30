# SIFT Keypoint Phase Index

| phase | status | 默认恢复动作 | plan | result |
| --- | --- | --- | --- | --- |
| 000-current-state-and-gaps | completed | 已把 Gaussian weight loop（高斯权重循环）诊断边界闭合；后续只作为 Phase 010 的前置证据读取 | `000-current-state-and-gaps/plan.zh.md` | `000-current-state-and-gaps/result.zh.md` |
| 010-production-full-gaussian-rvv | completed | 当前生产接入证据已闭合；若要继续，先创建新的 post-adoption profile / extrema phase | `010-production-full-gaussian-rvv/plan.zh.md` | `010-production-full-gaussian-rvv/result.zh.md` |

## 当前默认状态

`current_decision`: `production-adopted-narrow-scope`。当前 adopted 范围是
`PointXYZI -> PointWithScale`、organized dense synthetic `public_sift_keypoint_320x240`
公开入口 case、`Scalar=float` 和 Milkv-Jupiter 板卡 evidence（证据）。

## 证据入口

- 生产公开入口 repeated board（重复板卡测试）summary:
  `test-rvv/keypoints/sift_keypoint/log/board/repeated_phase010_public_compute/summary.md`
- public output trace（公开入口输出跟踪）:
  `test-rvv/keypoints/sift_keypoint/log/board/public_trace_phase010_public_compute/public_trace_compare.md`
- Evidence Doctor（证据体检）:
  `test-rvv/keypoints/sift_keypoint/log/board/repeated_phase010_public_compute/evidence_doctor.md`
- evidence registry（证据登记表）:
  `test-rvv/keypoints/sift_keypoint/log/evidence_registry.json`

## 下一阶段条件

当前没有在本轮授权范围内、无需新增 profile（剖析）或范围扩展即可继续的 high-priority action。
如要继续扩大 SIFT topic，默认下一 phase 应先冻结以下之一：

| next phase | 触发条件 | 必需证据 |
| --- | --- | --- |
| `020-post-adoption-profile` | 需要证明 adopted patch 后 `findScaleSpaceExtrema()` 或其它阶段成为新瓶颈 | public profile、correctness、asm、board summary、Evidence Doctor |
| `020-point-type-expansion` | 要覆盖 `PointXYZ`、`PointXYZRGB/RGBA`、自定义 intensity 字段或其它输出组合 | 点型 gate 审计、fallback test、public trace、board repeated |
| `020-larger-public-workloads` | 要覆盖 641x481 或真实 workload | 输入语义说明、public trace、board repeated、Evidence Doctor |
