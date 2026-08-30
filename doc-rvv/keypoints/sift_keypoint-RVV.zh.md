# SIFT Keypoint RVV 优化说明

## 当前状态

`keypoints/include/pcl/keypoints/impl/sift_keypoint.hpp` 已在 `computeScaleSpace()` 中接入窄范围
RVV production path（生产路径）。当前 `EvidenceDecision`（证据决策）是
`production-adopted-narrow-scope`：在 `PointXYZI -> PointWithScale`、organized dense synthetic
`public_sift_keypoint_320x240` 公开入口 case、`Scalar=float` 和 Milkv-Jupiter 板卡上，RVV 版本的
真实 `SIFTKeypoint::compute()` 快于当前标量路径。

这份长期文档只说明当前 adopted production behavior（已采纳生产行为）。Phase 计划、历史尝试、
diagnostic（诊断）取舍和后续恢复队列的主归属仍在
`test-rvv/keypoints/sift_keypoint/doc/phases/`、`doc/sift_keypoint-evaluation.zh.md` 和
`doc/optimization-roadmap.zh.md`。

## 函数入口和标量路径

公开入口通过 `SIFTKeypoint::compute()` 进入 octave（倍频程）检测流程：

1. `detectKeypoints()` 负责 downsample（下采样）、search tree（搜索树）更新和最终 output assembly
   （输出组装）。
2. `detectKeypointsForOctave()` 为每个 octave 生成 scales（尺度）、DoG matrix
   （Difference of Gaussian，高斯差分矩阵）和 extrema（极值点）。
3. `computeScaleSpace()` 对每个输入点调用 `tree.radiusSearch()`，随后对每个 scale 遍历已按距离排序的
   neighbor（邻域点），计算 Gaussian weight 并得到 filter response（滤波响应）。
4. `findScaleSpaceExtrema()` 在 DoG matrix 上做 nearest-k local extrema scan（近邻局部极值扫描）。

原标量 `computeScaleSpace()` 对每个 neighbor 执行：

```text
if dist_sqr <= 9 * sigma^2:
  w = exp(dist_sqr * -0.5 / sigma^2)
  numerator += intensity * w
  denominator += w
else:
  break
```

`radiusSearch()`、DoG 差分写入、extrema scan 和输出组装仍保持原标量语义。

## 当前采用的优化方式

RVV 路径只接管 Gaussian weight 的批量计算，不改变公开 API，也不改变 `__RVV10__` 关闭时的行为。

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| RVV weight staging（权重暂存） | adopted | `nn_dist` 连续存储，适合 VL chunk（可变向量长度分块）批量计算 `expf_RVV_f32m2` | public trace 0 diff；board median 1.309x | 只暂存 weight，不改变后续标量状态 |
| scalar tail（标量尾段）累加 | adopted | 保持 numerator / denominator 的原邻域顺序，避免改变浮点规约树和输出顺序 | `public_trace_compare.md` 中 `in_order_mismatch_count=0` | 性能仍包含这段标量 tail |
| production helper split（生产 helper 拆分） | accepted exception | `computeScaleSpace()` 是 header-only protected template helper，当前 RVV 只接管内层纯 weight 计算 | phase 010 result 和 public 证据 | 暂不拆 `*_Std` / `*_RVV` helper；若后续扩大到 extrema、更多点型或多条 RVV family，再单独做 helper split phase |
| full vector reduction（完整向量规约） | rejected | 历史尝试导致 public trace 顺序不一致 | 116 vs 116 keypoints 但曾有 `in_order_mismatch_count=51` | 只有设计保序规约并重做 public trace 才能重开 |
| `findScaleSpaceExtrema()` RVV | not_now | 当前没有采纳后 profile（剖析）证明它是新瓶颈 | 无 production evidence | 需要新 phase |
| 点型 / workload 扩展 | deferred | 当前证据只覆盖一个 public case | 当前 board summary | 需要单独 point-type 或 workload phase |

### VL chunk 内部流程

在 RVV 构建中，`computeScaleSpace()` 先把 neighbor 的 intensity field（强度字段）预取到
`nn_values`，因为原 `getFieldValue_()` 通过点索引访问对象字段，不是连续 f32 数组。每个 scale 内部：

1. 用 `vle32` 从连续的 `nn_dist` 读取距离平方。
2. 用 mask（掩码）查找第一个超过 cutoff 的 lane（向量通道）。
3. 对仍在 cutoff 内的 lane 计算 `dist * exponent_scale`。
4. 调用 `pcl::expf_RVV_f32m2()` 得到 Gaussian weight。
5. 把 weight 写入 `nn_weights`，再让后续 scalar tail 按原顺序累加。

这个分工让 RVV 覆盖逐邻域的数学热点，同时把 early break（提前停止）、浮点累加顺序和 DoG 输出保持在原语义边界内。

## 覆盖范围与 fallback

| 范围 | 当前状态 | 说明 |
| --- | --- | --- |
| 编译宏 | adopted | 只有 `__RVV10__` 且 `__riscv_vector` 存在时启用 RVV include 和 intrinsic（内建函数）分支 |
| 非 RVV 构建 | scalar fallback | 预处理分支完全保留原标量循环 |
| public API | unchanged | 没有新增公开函数、参数或类成员 |
| 点类型 | narrow evidence | 当前 production evidence 只覆盖 `PointXYZI -> PointWithScale` |
| intensity 字段 | narrow evidence | 当前路径通过现有 `getFieldValue_()` 读取强度；其它字段布局未单独验证 |
| `Scalar` | `float` evidence | SIFT scale-space 当前使用 `float` matrix 和 `float` scale |
| 输入布局 | organized dense synthetic 320x240 | 不外推到真实业务 cloud、非 organized 或更大规模 |
| search / extrema | scalar | `radiusSearch()` 和 `findScaleSpaceExtrema()` 没有 RVV 化 |

因为 production 代码位于模板头文件中，当前文档不能把这次 positive 结果写成全部模板实例的泛型结论。
如果要扩大到其它 `PointInT`、其它 output、其它 intensity 字段或自定义点类型，需要先完成点类型 gate、
fallback test、public trace、反汇编和板卡 repeated evidence。

## Traceability Map（可追踪性地图）

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `computeScaleSpace()` | production RVV boundary | 批量计算 Gaussian weight，保留标量累加 tail | `SIFTKeypoint::compute()` 间接调用 | DoG matrix / extrema scan | production boundary | `keypoints/include/pcl/keypoints/impl/sift_keypoint.hpp` |
| `run_test_compare` | correctness target（正确性目标） | Std/RVV gtest 对拍 | topic Makefile | gtest logs | correctness gate | `test-rvv/keypoints/sift_keypoint/Makefile` |
| `compare_sift_public_outputs.py` | analysis script（分析脚本） | 对比 Std/RVV public trace | `compare_public_trace` | phase result / evaluation | public correctness gate | `test-rvv/keypoints/sift_keypoint/script/compare_sift_public_outputs.py` |
| `summary.md` | board output summary（板卡摘要） | production-public repeated board 统计 | `record_evidence_state_public` | evaluation / Handoff | board performance evidence | `test-rvv/keypoints/sift_keypoint/log/board/repeated_phase010_public_compute/summary.md` |
| `evidence_doctor.md` | Evidence Doctor | 暴露 Error / Warning / Suggestion | `record_evidence_state_public` | evaluation / Handoff | evidence validation | `test-rvv/keypoints/sift_keypoint/log/board/repeated_phase010_public_compute/evidence_doctor.md` |
| `sift_keypoint-evaluation.zh.md` | evaluation（函数级评估） | 候选取舍、生产接入判断和后续边界 | topic README | reviewer / next worker | decision audit | `test-rvv/keypoints/sift_keypoint/doc/sift_keypoint-evaluation.zh.md` |

## 数值算例

假设某个 scale 的 `sigma_sqr=4`，则 cutoff 为 `36`，`exponent_scale=-0.125`。邻域距离平方和强度为：

| neighbor | `dist_sqr` | intensity | weight |
| ---: | ---: | ---: | ---: |
| 0 | 1 | 10 | `exp(-0.125)` |
| 1 | 4 | 20 | `exp(-0.5)` |
| 2 | 49 | 30 | 超出 cutoff，停止 |

RVV chunk 会为前两个 lane 计算 weight 并写入 `nn_weights`；scalar tail 再按 neighbor 0、neighbor 1 的顺序累加：

```text
numerator = 10 * exp(-0.125) + 20 * exp(-0.5)
denominator = exp(-0.125) + exp(-0.5)
filter_response = numerator / denominator
```

如果用 vector reduction 改变累加树，浮点舍入和后续 extrema tie（极值相等判断）可能变化；当前实现因此保留标量累加。

## Bench 与证据

| 证据 | 路径 | 结果 | 说明 |
| --- | --- | --- | --- |
| production-public repeated board | `test-rvv/keypoints/sift_keypoint/log/board/repeated_phase010_public_compute/summary.md` | 5-run median speedup 1.309x，min 1.304x，max 1.310x，`B/A < 1 = 0/5` | 性能结论只来自板卡 |
| public output trace | `test-rvv/keypoints/sift_keypoint/log/board/public_trace_phase010_public_compute/public_trace_compare.md` | 116 / 116 keypoints，顺序一致，`x/y/z/scale` 最大差异为 0 | 证明当前 public case 输出语义一致 |
| Evidence Doctor | `test-rvv/keypoints/sift_keypoint/log/board/repeated_phase010_public_compute/evidence_doctor.md` | Errors=0，Warnings=0，Suggestions=2 | Suggestion 为环境 metadata 和 binary identity 建议 |
| diagnostic prerequisite | `test-rvv/keypoints/sift_keypoint/log/board/repeated_phase000_scale_space_profile_prerequisite/summary.md` | synthetic helper median 4.273x 到 5.327x | 只解释为什么进入 production probe |

QEMU（仿真器）只用于 correctness 和日志形状，不用于性能排序。反汇编目标
`check_sift_keypoint_rvv_asm` 用于确认 RVV build 里存在 `expf_RVV_f32m2`、load 和 `vsetvl` 相关路径；
它不单独证明性能。

## 正确性与高效性证据链

| 层级 | 当前证据 | 结论 | 限制 |
| --- | --- | --- | --- |
| correctness | `run_test_compare` + public trace compare | 当前 public case 输出计数、顺序和字段一致 | 不覆盖所有点型 / workload |
| path / asm | `check_sift_keypoint_rvv_asm` | RVV binary 命中预期数学和向量访存路径 | 反汇编不是性能证据 |
| performance | repeated board summary | public RVV path 快于 public scalar path | 只覆盖当前 case 和板卡 |
| boundary | phase 010 result + evaluation | EvidenceDecision 保持窄范围 | 不外推到 extrema、泛型点型、真实输入 |
| risk | roadmap | 后续扩展有明确恢复条件 | 没有 profile 前不继续改 extrema |

## Production Closeout

| 项 | 最终状态 | 证据 |
| --- | --- | --- |
| 生产补丁范围 | `sift_keypoint.hpp` 中新增 RVV guarded include 和 `computeScaleSpace()` RVV 分支 | 当前 diff；`doc/phases/010-production-full-gaussian-rvv/result.zh.md` |
| public API | unchanged | 生产头文件没有新增公开接口 |
| 采纳机制 | RVV weight staging + scalar tail | public trace 0 diff；board median 1.309x |
| 回退策略 | 非 RVV 构建使用原标量循环 | 预处理分支和 `run_test_compare` |
| 不采纳机制 | full vector reduction | 历史 trace mismatch，当前未保留 |
| 后续默认动作 | stop_for_review；如继续需新 phase | `doc/optimization-roadmap.zh.md` |

## 后续方向

当前没有在已授权范围内、无需新增 profile 或范围扩展即可继续的高优先级动作。后续可重开三类 phase：

- `020-post-adoption-profile`：先证明 `findScaleSpaceExtrema()` 或其它阶段已经成为 adopted patch 后的新瓶颈。
- `020-point-type-expansion`：为其它 `PointInT`、intensity 字段布局或 output 组合补 fallback、trace、asm 和 board evidence。
- `020-larger-public-workloads`：为更大 synthetic case 或真实 workload 补 production-public repeated board 和 Evidence Doctor。
