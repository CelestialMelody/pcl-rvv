# Integral Image Normal Phase Index

| phase | status | default recovery | plan | result |
| --- | --- | --- | --- | --- |
| `000-current-state-and-map-prep-diagnostic` | `completed` | Historical diagnostic evidence; current numbers are in `log/board/repeated-summary.md` | `000-current-state-and-map-prep-diagnostic/plan.zh.md` | `000-current-state-and-map-prep-diagnostic/result.zh.md` |
| `010-pi1-production-integration-plan` | `superseded_by_phase050` | PI1 plan drove the bounded Phase 050 production probe | `010-pi1-production-integration-plan/plan.zh.md` | superseded |
| `020-average-3d-gradient-diff-buffer-diagnostic` | `completed` | Weak size-dependent diagnostic; do not enter production patch from this phase | `020-average-3d-gradient-diff-buffer-diagnostic/plan.zh.md` | `020-average-3d-gradient-diff-buffer-diagnostic/result.zh.md` |
| `030-average-3d-gradient-production-shaped-profile` | `completed` | Negative / unstable production-shaped diagnostic; do not production-patch diff-buffer | `030-average-3d-gradient-production-shaped-profile/plan.zh.md` | `030-average-3d-gradient-production-shaped-profile/result.zh.md` |
| `040-pcl-integral-image2d-boundary-profile` | `completed` | Weak / unstable exact-PCL production-shaped diagnostic; do not production-patch diff-buffer | `040-pcl-integral-image2d-boundary-profile/plan.zh.md` | `040-pcl-integral-image2d-boundary-profile/result.zh.md` |
| `050-map-prep-production-probe` | `completed/adopted` | Production patch is adopted; public `compute()` board evidence is weak-positive and documented in `doc-rvv/features/integral_image_normal-RVV.zh.md` | `050-map-prep-production-probe/plan.zh.md` | `050-map-prep-production-probe/result.zh.md` |

## 当前恢复说明

Phase 000 的 doc suite（文档套件）已经补齐到 topic-local role 文档：
`testing-overview.zh.md`、`correctness-tests.zh.md`、`benchmark-and-evidence.zh.md`、
`optimization-evidence.zh.md` 和 `test-support-code-map.zh.md`。

Phase 020 已完成 `initAverage3DGradientMethod()` diff_x / diff_y buffer 的 test helper diagnostic。
`avg3d_diff_641x481_tail` 在板卡上稳定正向，但 `avg3d_diff_320x240` 近阈值且有 1/5 退化；因此它不作为
新的 production patch 入口。

Phase 030 已完成 AVERAGE_3D_GRADIENT 的 production-shaped profile（生产形态剖析）。
当前复跑后 `avg3d_profile_641x481_tail` mean speedup 为 1.02x，但仍有 2/5 run 退化；这些异常用于降级
diff-buffer 生产化判断，不代表 checksum 错误。

Phase 040 已补 exact PCL `IntegralImage2D<float,3>` boundary profile（真实 PCL 积分图边界剖析）。
`pcl_avg3d_profile_320x240` mean 1.01x 且 1/5 退化；`pcl_avg3d_profile_641x481_tail` mean 1.04x
但 2/5 退化。Evidence Doctor 输出 Errors=8 / Warnings=22 / Suggestions=8；Error 仍是退化频率和长尾
信号，不是 checksum 或脚本失败。

Phase 050 已把 map-prep 前缀接入 production source（生产源码）并完成 public `compute()` production direct
（真实生产入口）板卡验证。`prod_compute_avg_depth_320x240` median / mean 均为 1.06x，min 为 1.00x；
`prod_compute_avg_depth_641x481_tail` median / mean 均为 1.06x，min 为 0.99x。Evidence Doctor 当前为
Errors=3 / Warnings=21 / Suggestions=8；production-direct 两项为退化频率 warning，不是 checksum 或
correctness error。用户已确认采纳并保留当前 production patch，长期文档为
`doc-rvv/features/integral_image_normal-RVV.zh.md`。当前没有建议立即继续扩大 production patch 的方向。
