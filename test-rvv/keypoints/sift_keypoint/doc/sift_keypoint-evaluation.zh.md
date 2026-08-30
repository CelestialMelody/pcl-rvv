# SIFT Keypoint RVV evaluation

## 范围和目标源码

目标源码是 `keypoints/include/pcl/keypoints/impl/sift_keypoint.hpp`。当前 production
接入范围只覆盖 `computeScaleSpace()` 的 Gaussian weight loop（高斯权重循环）；
`findScaleSpaceExtrema()` 的 local extrema scan（局部极值扫描）保持标量。

最终 `EvidenceDecision`（证据决策）为 `production-adopted-narrow-scope`，覆盖
`PointXYZI -> PointWithScale`、organized dense synthetic `public_sift_keypoint_320x240`
公开入口 case、`Scalar=float` 和 Milkv-Jupiter 板卡 evidence（证据）。其它点型、真实 workload
和更大规模需要后续 phase 单独批准。

## 函数 / 函数族作用速览

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | 与主流程关系 | RVV 判断 |
| --- | --- | --- | --- | --- |
| `detectKeypoints()` | octaves 入口，负责 downsample、search tree 更新和 output 组装 | 输入 cloud -> 输出 keypoints | 公共入口 | 通过 `computeScaleSpace()` 间接命中 RVV |
| `detectKeypointsForOctave()` | 组装单个 octave 的 scales、DoG 和 extrema 结果 | octave 输入 -> keypoint index / scale | 组织 scale-space | 保持原调用链 |
| `computeScaleSpace()` | `radiusSearch` 后计算各 scale 的 Gaussian filter response（高斯滤波响应） | 邻域、scales -> DoG matrix | production RVV 主边界 | adopted narrow scope |
| `findScaleSpaceExtrema()` | DoG matrix 上的邻域极值扫描 | DoG matrix -> extrema indices | 仍是后段标量状态机 | not_now，需要新 profile |

## 标量流程与 RVV 流程对照

- 标量侧在每个 point / scale 内遍历 `radiusSearch` 返回的 sorted neighbors
  （已排序邻域），遇到距离超过 `9 * sigma^2` 后 early break（提前停止），用
  `std::exp(dist_sqr * -0.5 / sigma^2)` 得到 weight，再按邻域顺序累加 numerator /
  denominator。
- RVV 路径先把 neighbor 的 intensity field（强度字段）预取到连续 `nn_values`，
  每个 scale 用 VL chunk（可变向量长度分块）批量读取 `nn_dist`、检查 cutoff、调用
  `expf_RVV_f32m2` 计算 weight 并写入 `nn_weights`。
- 累加仍保留 scalar tail（标量尾段），按原邻域顺序消费 `nn_values` 和 `nn_weights`。
  这样避免 full vector reduction（完整向量规约）改变浮点规约树和输出顺序。
- `radiusSearch`、`nearestKSearch`、`findScaleSpaceExtrema()` 和 output assembly（输出组装）
  都保持标量；当前证据不声称这些阶段已经优化。

## Traceability Map（可追踪性地图）

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `computeScaleSpace()` RVV branch | production RVV boundary | 批量计算 Gaussian weight，保留标量累加 tail | `SIFTKeypoint::compute()` 间接调用 | DoG matrix / extrema scan | production boundary | `keypoints/include/pcl/keypoints/impl/sift_keypoint.hpp` |
| `src/test_sift_keypoint.cpp` | correctness tests | helper 对拍和 public smoke | `run_test_compare` | gtest output | correctness gate | `test-rvv/keypoints/sift_keypoint/src/test_sift_keypoint.cpp` |
| `src/bench_sift_keypoint.cpp` | bench wrapper | diagnostic 和 public case timing | board / QEMU targets | summary generator | performance evidence input | `test-rvv/keypoints/sift_keypoint/src/bench_sift_keypoint.cpp` |
| `compare_sift_public_outputs.py` | analysis script | 对比 Std/RVV public output trace | `compare_public_trace` | phase result / evaluation | public correctness gate | `test-rvv/keypoints/sift_keypoint/script/compare_sift_public_outputs.py` |
| public board summary | evidence output summary | 生产公开入口 repeated board 摘要 | `record_evidence_state_public` | `doc-rvv` / Handoff | board performance evidence | `test-rvv/keypoints/sift_keypoint/log/board/repeated_phase010_public_compute/summary.md` |

## 实现方式审计

| 实现维度 | 当前状态 | 证据 | 边界 / 恢复条件 |
| --- | --- | --- | --- |
| dispatch / fallback（分流 / 回退） | adopted | `__RVV10__` + `__riscv_vector` 宏包住 RVV 分支；非 RVV 构建走原标量循环 | public API 不变；未覆盖编译配置自然 fallback |
| staging / scalar tail（暂存 / 标量尾段） | adopted | public trace 0 diff，board median 1.309x | weight 暂存，原顺序累加 |
| production helper split（生产 helper 拆分） | accepted exception | `computeScaleSpace()` 是 header-only protected template helper；RVV 只替换内层纯 weight 计算，外层标量流程和公开入口保持原结构 | 当前不拆 `*_Std` / `*_RVV` helper，避免在生产头文件里为窄范围补丁扩大模板接口面；若后续扩展到 extrema 或更多点型，再新建 helper split phase |
| full vector reduction | rejected | 历史 public trace mismatch：`in_order_mismatch_count=51` | 需新保序规约计划才能重开 |
| point type / layout | deferred with stop condition | 当前证据只覆盖 `PointXYZI -> PointWithScale` | 其它点型需要单独 phase |
| extrema scan | not_now | 无 post-adoption profile 证明它是新瓶颈 | 需要 public profile 后再判断 |

## 验证结果

| 层级 | 命令 / 路径 | 结果 | 证据边界 |
| --- | --- | --- | --- |
| correctness（正确性） | `make -C test-rvv/keypoints/sift_keypoint run_test_compare` | Std/RVV gtest 对拍通过 | QEMU / 本地 correctness，不是性能结论 |
| public trace | `test-rvv/keypoints/sift_keypoint/log/board/public_trace_phase010_public_compute/public_trace_compare.md` | 116 / 116 keypoints，`in_order_mismatch_count=0`，`x/y/z/scale` 最大差异为 0 | 只覆盖 `public_sift_keypoint_320x240` |
| asm attribution（反汇编归属） | `make -C test-rvv/keypoints/sift_keypoint check_sift_keypoint_rvv_asm` | RVV binary 中命中 `expf_RVV_f32m2`、load 和 `vsetvl` 相关路径 | 证明路径存在，不单独证明收益 |
| production board | `test-rvv/keypoints/sift_keypoint/log/board/repeated_phase010_public_compute/summary.md` | 5-run median speedup 1.309x，min 1.304x，max 1.310x，`B/A < 1 = 0/5` | 只证明当前 public RVV path 快于当前 public scalar path |
| Evidence Doctor | `test-rvv/keypoints/sift_keypoint/log/board/repeated_phase010_public_compute/evidence_doctor.md` | Errors=0，Warnings=0，Suggestions=2 | Suggestion 不阻塞采纳，但提示后续补环境 metadata 和二进制身份 |

## 生产接入判断

`production_patch_scope`: 只修改 `sift_keypoint.hpp`，在 `computeScaleSpace()` 内新增 RVV
guarded include 和 RVV weight 计算分支；不改 public API，不改 `detectKeypoints()`、
`detectKeypointsForOctave()` 或 `findScaleSpaceExtrema()` 的公开语义。

`covered_path`: `PointXYZI -> PointWithScale`，organized dense synthetic 320x240 public case，
`Scalar=float`，Milkv-Jupiter board。

`fallback_matrix`: 非 RVV 构建、未命中宏的构建、其它未验证点型 / workload / 规模继续使用原标量语义。
当前代码没有新增运行期点型 gate；因此文档结论必须保持窄范围，不能写成模板泛型全覆盖。

`production_board_bench`: 当前采用
`test-rvv/keypoints/sift_keypoint/log/board/repeated_phase010_public_compute/summary.md`
作为 production-public 性能证据。QEMU timing（QEMU 计时）不进入性能排序。

`decision_delta`: Phase 000 的 diagnostic 结果把 topic 推进到 production probe；Phase 010 的
production-public repeated board 证明确认窄范围 public path 有收益，因此采纳当前保守 production patch。

## 后续方向

当前不建议在没有新 profile 的情况下继续改 `findScaleSpaceExtrema()`。它涉及 nearest-k neighborhood
（近邻邻域）、tie 语义和输出顺序；如果采纳后 public profile 显示它成为新瓶颈，再创建独立 phase。
点型扩展和更大 workload 也需要单独证据，不能由当前 320x240 case 外推。
