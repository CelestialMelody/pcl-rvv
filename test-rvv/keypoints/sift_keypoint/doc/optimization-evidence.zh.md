# SIFT Keypoint Optimization Evidence

| candidate family | status | production / test support path | test / evidence | boundary |
| --- | --- | --- | --- | --- |
| `gaussian-weight-loop-rvv` | adopted as prerequisite | `include/impl/sift_keypoint_scale_space.hpp` | `repeated_phase000_scale_space_profile_prerequisite/summary.md`，median 4.273x 到 5.327x | 只覆盖 synthetic scale-space kernel，不是 production evidence |
| `production-public-gaussian-weight-rvv` | adopted | `keypoints/include/pcl/keypoints/impl/sift_keypoint.hpp` | `repeated_phase010_public_compute/summary.md`，median 1.309x；`public_trace_phase010_public_compute/public_trace_compare.md` 0 diff | 只覆盖 `PointXYZI -> PointWithScale` organized dense public case |
| `full-vector-reduction` | rejected | historical `sift_keypoint.hpp` attempt，当前未保留 | public trace 曾出现 `in_order_mismatch_count=51` | 浮点规约树 / 输出顺序风险；需要新保序计划才能重开 |
| `extrema-scan-rvv` | not_now | `include/impl/sift_keypoint_scale_space.hpp` | 暂无 post-adoption profile | 先不与权重循环绑定；需要独立 phase |
| `point-type-expansion` | deferred with stop condition | `keypoints/include/pcl/keypoints/impl/sift_keypoint.hpp` | 当前无其它点型 public trace / board | 需要单独 point-type expansion phase |
