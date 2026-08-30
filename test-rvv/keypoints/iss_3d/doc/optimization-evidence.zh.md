# ISS 3D Optimization Evidence

| candidate family | status | code / target | evidence | boundary |
| --- | --- | --- | --- | --- |
| f32 accumulation scatter | rejected with evidence | historical test helper only | 早期 board smoke 有性能信号但 checksum mismatch。 | 与 production double scatter 语义不一致，不继续。 |
| f64 vector reduction scatter diagnostic | attempted_positive | `include/impl/iss_3d_scatter.hpp`、`board_repeated` | `iss_3d_phase000_scatter_f64_rerun1` positive；QEMU correctness 和 asm 通过。 | 只证明 component ablation。 |
| production scatter helper | removed_no_production | historical `keypoints/include/pcl/keypoints/impl/iss_3d.hpp` probe、guarded `board_repeated_production` | micro-opt 后 `iss_3d_phase010_production_public_reduction_shape` median 1.014x，decision bucket neutral。 | 真实 public compute 收益不足；当前提交不保留 production patch。 |
| production reduction shape micro-opt | historical_attempted | historical shape gate | RED saw 10，修正后 saw 7；QEMU 和 asm 通过。 | 只减少重复规约开销，没有改变 public decision bucket；随 production probe 一起移除。 |
| contiguous neighbor fast path | rejected for current production | diagnostic contiguous case 正向，但 production search 不保证连续邻域。 | 没有低成本连续性判定和 public direct 证据。 | 不作为下一步默认动作。 |
| broader search / EVD / NMS vectorization | not_now | none | production public 说明 scatter 不是足够大的端到端收益点。 | 需要 profile 或另开 topic。 |

## 当前采用的优化方式

没有 adopted production optimization（已采纳生产优化）。当前 production source 保持原标量路径；历史 production probe 通过 correctness 和 asm，但 production board evidence（板卡生产证据）为 neutral，因此不进入提交候选。

## 不采纳原因

diagnostic helper 的局部收益不能直接覆盖 public compute。真实生产计时边界包含 search、EVD、NMS 和 output，scatter 的收益被稀释。当前最强 production run 只有 1.014x median，且 synthetic public checksum 为 0，说明该输入没有产出关键点，输出语义覆盖较弱。

## 恢复条件

- 用户明确接受 near-threshold 风险并决定重新引入 production patch。
- 新 public case 产生非零 keypoint 输出，并在 repeated board 中达到更明确的 weak-positive 或 positive bucket。
- profile 证明 scatter 在目标 workload 中仍是高占比热点。
