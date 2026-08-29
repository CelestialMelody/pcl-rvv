# Trajkovic 3D FOUR_CORNERS / EIGHT_CORNERS RVV Production 文档

## 当前状态

`keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp` 已采纳两个窄范围 RVV production path（生产路径）：`TrajkovicKeypoint3D::detectKeypoints()` 在 `FOUR_CORNERS` / `EIGHT_CORNERS`、`window_size_=3`、precomputed normals（预计算法线）和 organized dense full cloud（有组织稠密完整点云）条件下，用 RVV 计算 response map（响应图）。其它路径继续走原标量实现。

当前生产性能结论来自真实公开入口 `Detector::compute(output)` 的 repeated board（重复板卡性能测试）：`public_four_corners_320x240` median speedup 为 1.790x，`public_eight_corners_320x240` median speedup 为 1.677x，两者 5 次运行都没有低于 1.0x。QEMU（仿真器）只用于 correctness（正确性）和日志形状，不作为性能证据。

## 函数语义和标量路径

`TrajkovicKeypoint3D` 面向 organized cloud（有组织点云）生成 3D keypoint response。调用方可以预先提供 normals；如果没有提供，`initCompute()` 会用 integral image normal estimator（积分图法线估计器）生成法线。当前 RVV patch 只优化已经有 normals 的 response map，不优化 normal estimation。

`detectKeypoints()` 的标量主流程如下：

1. 为每个输入点准备 `response`。
2. 对内部像素计算 `FOUR_CORNERS` 或 `EIGHT_CORNERS` response。`FOUR_CORNERS` 使用上、下、左、右四个邻域 normal，`EIGHT_CORNERS` 额外使用四个对角邻域 normal；两者都与中心 normal 做 dot product（点积）、squared diff（平方差）、sqrt（平方根）和 quadratic branch（二次分支）计算。
3. 对 response 按强度排序，使用 occupancy map（占用图）做 NMS（非极大值抑制）。
4. 把通过 NMS 的点复制到 `PointOutT`，并把 response 写入 `intensity`。

RVV 只接管第 2 步的 `FOUR_CORNERS` / `EIGHT_CORNERS` response map。第 3、4 步保留标量，因为排序、局部屏蔽和输出顺序是可见语义。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| dispatch（分流逻辑） | adopted | 公开入口只在 `FOUR_CORNERS` / `EIGHT_CORNERS`、3x3、dense input / normals 时进入 RVV，fallback 简单。 | public gtest、production board。 | 非 3x3、non-dense 和未验证布局保持标量。 |
| layout gate（布局门控） | adopted narrow | `PointInT` 用 `RVVXYZAoSFloatLayout` 证明 xyz float AoS；`NormalT` 用 topic-local strong normal AoS gate 证明 normal fields、POD size、stride 和 offset alignment。 | traits gtest、源码 gate。 | 当前证据只覆盖 `PointXYZ + Normal`。 |
| VL chunk（可变向量长度分块） | adopted | 每行内部列按 `vsetvl` 分块，使用跨步加载读取 point / normal 字段，尾部自然由最后一个 VL 处理。 | asm gate、response correctness。 | 无额外 tail staging。 |
| finite mask（有限值掩码） | adopted | point 和 center normal 必须 finite；非法邻域 normal merge 成零向量，对齐标量 `getNormalOrNull()`。 | invalid normal gtest。 | production RVV gate 要求 dense，non-dense public input 回退标量。 |
| NMS 和输出 | scalar-only | 排序、occupancy 和 `push_back` 影响输出顺序。 | public output gtest。 | 只有 profile 证明 NMS 主导时再评估。 |
| `EIGHT_CORNERS` | adopted | 公式更长，但与四邻域共享同一 dispatch、layout gate 和板卡闭环。 | current board 已正向。 |

## 范围决策表

| 范围 | 状态 | 证据 | 下一步 |
| --- | --- | --- | --- |
| `FOUR_CORNERS` + 3x3 + dense organized full cloud | adopted | `public_four_corners_320x240` production board 1.790x；QEMU gtest 通过。 | 保留当前 patch。 |
| `EIGHT_CORNERS` + 3x3 + dense organized full cloud | adopted | `public_eight_corners_320x240` production board 1.677x；QEMU gtest 通过。 | 保留当前 patch。 |
| `FOUR_CORNERS` / `EIGHT_CORNERS` + non-dense input 或 normals | scalar fallback | public invalid control 接近 1.0x，说明没有误命中 RVV；gtest 覆盖 invalid 语义。 | 若要接入，需要单独证明 invalid mask production path。 |
| `FOUR_CORNERS` / `EIGHT_CORNERS` + 非 3x3 window | scalar fallback | 源码 gate 明确不进入 RVV。 | 需要独立规模和 window phase。 |
| normal estimation | scalar / existing implementation | 不在本文件。 | 转入 normal estimator topic。 |
| 更宽点类型或自定义 `NormalT` | deferred | layout gate 允许一部分 traits 类型，但 production evidence 未覆盖。 | point-type expansion phase。 |

## 标量路径与 RVV 路径差异

| 阶段 | 标量行为 | RVV 行为 | 对应证据 |
| --- | --- | --- | --- |
| response 清零 | `response` 初始为 0。 | RVV helper 先清零整张 response。 | gtest `LeavesBorderAndRejectedResponsesZero`。 |
| 字段读取 | 逐点访问 `x/y/z` 和 `normal_x/y/z`。 | `vlse32.v` 跨步加载 AoS 字段。 | asm gate。 |
| invalid handling | `isFinite()` 和 `getNormalOrNull()`。 | mask 检查有限值，非法邻域 normal merge 为零。 | invalid normal gtest。 |
| response 公式 | 标量 dot、sqrt、min 和 branch。 | 向量 dot、sqrt、min 和 masked select。 | response compare 和 public output compare。 |
| NMS / output | 标量排序和输出。 | 未改动。 | public output checksum。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 上游入口 | 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `TrajkovicKeypoint3D::detectKeypoints()` | production public entry | 真实分流点和 fallback 保留点。 | `compute()` | RVV helper 或标量主体 | production boundary | `keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp` |
| `trajkovic3DFourCornersResponseRVV` | production RVV helper | 计算 adopted response map。 | `detectKeypoints()` | 标量 NMS | adopted RVV implementation | `keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp` |
| `trajkovic3DEightCornersResponseRVV` | production RVV helper | 计算 adopted response map。 | `detectKeypoints()` | 标量 NMS | adopted RVV implementation | `keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp` |
| `test_trajkovic_3d.cpp` | correctness gate | response / public / fallback gtest。 | `run_test_compare` | QEMU logs | correctness | `test-rvv/keypoints/trajkovic_3d/src/test_trajkovic_3d.cpp` |
| `bench_trajkovic_3d.cpp` | bench wrapper | diagnostic 和 public board benchmark。 | board targets | summary / manifest | performance | `test-rvv/keypoints/trajkovic_3d/src/bench_trajkovic_3d.cpp` |
| Phase 020 summary | evidence output | 当前 production performance truth。 | `board_repeated_production` | evaluation / 本文 | board performance | `test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase020_eight_corners_production_public/summary.md` |
| Evidence Doctor | evidence validator | 检查 checksum、A/B boundary 和异常信号。 | `evidence_doctor_production` | evaluation / Handoff | evidence health | `test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase020_eight_corners_production_public/evidence_doctor.md` |
| Evaluation | decision audit | 候选取舍、fallback matrix 和未闭合项主归属。 | reviewer | 本文引用 | decision trace | `test-rvv/keypoints/trajkovic_3d/doc/trajkovic_3d-evaluation.zh.md` |

## 数值算例

对一个中心点 `c` 和上下左右四个邻域 normal：`u/d/l/r`，标量路径先计算：

```text
sn1 = (1 - dot(u, c))^2
sn2 = (1 - dot(d, c))^2
r1 = sn1 + sn2
r2 = (1 - dot(r, c))^2 + (1 - dot(l, c))^2
d = min(r1, r2)
```

如果 `d < first_threshold_`，该 lane（向量通道）保持零响应。否则继续计算 `sqrt(sn1)`、`sqrt(sn2)`、`b1`、`b2`、`b` 和 `a`，满足 `b < 0 && b + a > 0` 时写 `r1 - b*b/a`，否则写 `d`。RVV helper 对一个 VL chunk 内的多个列同时执行上述公式，并用 active mask（有效掩码）只写回通过 finite gate 和 threshold 的 lane。

## Bench 与证据

| case | 入口 | 数据 | 计时边界 | 证明点 | 不证明 |
| --- | --- | --- | --- | --- | --- |
| `four_corners_320x240` | test helper | synthetic 320x240 finite points/normals | response map only | Phase 000 局部公式收益。 | 不证明 public compute。 |
| `four_corners_invalid_320x240` | test helper | synthetic invalid point / normal | response map only | invalid neighbor null-normal 语义和局部收益。 | 不证明 dense production path。 |
| `four_corners_641x481_tail` | test helper | 非整 VL 尾部规模 | response map only | tail VL chunk 正确性和收益。 | 不证明 public compute。 |
| `public_four_corners_320x240` | `Detector::compute()` | dense 320x240 with precomputed normals | response + NMS + output | 当前 production adopted 性能。 | 不证明 normal estimation 或其它方法。 |
| `public_eight_corners_320x240` | `Detector::compute()` | dense 320x240 with precomputed normals | response + NMS + output | 当前 production adopted 性能。 | 不证明 normal estimation 或其它方法。 |
| `public_four_corners_invalid_320x240` | `Detector::compute()` | non-dense control | public compute scalar fallback | 回退路径没有明显成本异常。 | 不作为 RVV 主路径收益。 |
| `public_eight_corners_invalid_320x240` | `Detector::compute()` | non-dense control | public compute scalar fallback | 回退路径没有明显成本异常。 | 不作为 RVV 主路径收益。 |
| `public_four_corners_641x481_tail` | `Detector::compute()` | non-dense tail control | public compute scalar fallback | 回退路径和 tail 控制。 | 不作为 RVV 主路径收益。 |
| `public_eight_corners_641x481_tail` | `Detector::compute()` | non-dense tail control | public compute scalar fallback | 回退路径和 tail 控制。 | 不作为 RVV 主路径收益。 |

## 正确性与高效性证据链

| 层级 | 证据 | 结论 |
| --- | --- | --- |
| correctness | `make -C test-rvv/keypoints/trajkovic_3d run_test_compare`，Std/RVV 各 10 个测试通过。 | response helper、public output 和 fallback 行为在当前测试边界内一致。 |
| QEMU path | `test-rvv/keypoints/trajkovic_3d/log/qemu/run_test_rvv.log`。 | QEMU 证明构建、运行和日志形状，不证明真实性能。 |
| asm | `make -C test-rvv/keypoints/trajkovic_3d check_trajkovic_3d_rvv_asm`。 | RVV 指令能归属到当前 helper。 |
| board performance | `test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase020_eight_corners_production_public/summary.md`。 | production public 主路径 median 1.790x / 1.677x。 |
| Evidence Doctor | `test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase020_eight_corners_production_public/evidence_doctor.md`。 | 无 Error/Warning；metadata 建议不阻塞当前结论。 |
| registry | `test-rvv/keypoints/trajkovic_3d/log/evidence_registry.json`。 | 当前 summary / manifest / doctor 已登记，freshness check 可复核。 |

## Production Closeout

| 项 | 最终状态 | 证据 |
| --- | --- | --- |
| production patch | 保留，public API 不变。 | `keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp` diff。 |
| adopted scope | `FOUR_CORNERS` / `EIGHT_CORNERS` 3x3 dense precomputed normals。 | Phase 020 result 和 production board summary。 |
| fallback | 非 RVV、non-dense、非 3x3 和未覆盖点型回退标量。 | 源码 gate、gtest、fallback control logs。 |
| diagnostic 与 production 差异 | Phase 000 response-only 更快，Phase 020 public 仍 positive。 | Phase 000 / Phase 020 summaries。 |
| 后续方向 | point-type expansion、NMS profile 和 normal estimation 另行评估。 | optimization roadmap。 |
