# Trajkovic 2D Response Grid RVV

## 当前状态

`keypoints/include/pcl/keypoints/impl/trajkovic_2d.hpp` 已采用一条窄范围 production RVV
路径：`TrajkovicKeypoint2D<PointXYZI, PointXYZI>::compute()` 在 RVV 构建中、使用默认
`IntensityFieldAccessor`（强度字段访问器）、organized full-cloud 输入、`window_size == 3`
和 EIGHT_CORNERS 时，由 RVV 计算 response grid（响应图）。NMS（非极大值抑制）排序、
occupancy map 和输出写回保持标量。

FOUR_CORNERS、非 RVV 构建、非 `PointXYZI`、非默认 accessor、非 3x3 window、RGB 灰度
accessor 和其它泛型点类型仍走原标量路径。当前结论不能外推到整个模板入口或其它 intensity
布局。

## 稳定证据索引

| 证据 | 路径 / 命令 | 作用 |
| --- | --- | --- |
| production 源码 | `keypoints/include/pcl/keypoints/impl/trajkovic_2d.hpp` | RVV helper、dispatch（分流逻辑）和 fallback（回退路径）的真实实现 |
| 函数级评估 | `test-rvv/keypoints/trajkovic_2d_response_grid/doc/trajkovic_2d_response_grid-evaluation.zh.md` | 候选取舍、Traceability Map（可追踪性地图）和 production decision（生产接入判断） |
| Phase 000 result | `test-rvv/keypoints/trajkovic_2d_response_grid/doc/phases/000-current-state-and-response-grid-production-probe/result.zh.md` | production probe（生产探针）的阶段证据、矩阵和停止条件 |
| 正确性 | `make -C test-rvv/keypoints/trajkovic_2d_response_grid run_test_compare` | Std/RVV 构建各 4 个 public `compute()` gtest 通过 |
| 反汇编 | `make -C test-rvv/keypoints/trajkovic_2d_response_grid check_production_rvv_asm` | RVV 指令归属到 `detectKeypoints()` 内联范围 |
| 板卡摘要 | `test-rvv/keypoints/trajkovic_2d_response_grid/log/board/repeated-summary.md` | Milkv-Jupiter 5-run production-public 性能摘要 |
| Evidence Doctor（证据体检） | `test-rvv/keypoints/trajkovic_2d_response_grid/log/board/evidence_doctor.md` | Errors=0 / Warnings=0 / Suggestions=0 |
| Evidence registry（证据登记表） | `test-rvv/keypoints/trajkovic_2d_response_grid/log/evidence_registry.json` | summary、manifest 和 doctor 当前登记为 fresh |

## 函数语义

`TrajkovicKeypoint2D::detectKeypoints()` 先创建与输入 organized cloud 同尺寸的 `response_`。
内部像素按 `half_window_size_` 去掉边界后逐点计算响应值：FOUR_CORNERS 使用上、下、左、
右四个邻域点；EIGHT_CORNERS 额外使用四个对角邻域点。响应低于 `first_threshold_` 的位置保持
默认 0；随后 NMS 根据 `second_threshold_`、排序和 occupancy map 选择最终 keypoint，并把
`keypoints_indices_` 同步更新。

原标量 EIGHT_CORNERS 路径会为每个像素构造四组 `r`、`B`、`A` 和 `sumAB`，取最小响应或
曲率修正响应。这个逐像素固定 stencil（固定邻域模板）是本次 RVV 接管的主成本；NMS 依赖全局排序、
临界区输出和 occupancy 状态，当前保持标量以保留公开输出语义。

## 覆盖范围与 Fallback

| 维度 | 当前状态 | 证据 | 下一步条件 |
| --- | --- | --- | --- |
| EIGHT_CORNERS / `PointXYZI` / 3x3 | adopted | `run_test_compare`、`check_production_rvv_asm`、board repeated summary | 当前范围已闭合 |
| FOUR_CORNERS | scalar-only | gtest 和 board fallback control checksum match | 若要接入 RVV，需要重新证明 public output 和稳定收益 |
| 非 RVV 构建 | scalar fallback | 条件编译只在 `__RVV10__` 下启用 helper | 无需额外动作 |
| 非 `PointXYZI` 或非默认 accessor | scalar fallback | `kTrajkovic2DPointXYZIRVVGated` exact gate | 泛型扩展需新 phase |
| 非 3x3 window | scalar fallback | dispatch 运行期 gate 为 `window_size_ == 3` | 大窗口需要独立 offset、correctness、asm 和板卡证据 |
| RGB 灰度 accessor | scalar fallback | 当前 RVV 只跨步读取单 float intensity 字段 | 需要灰度语义对拍和新的 load 策略 |
| NMS / output | scalar tail（标量尾段） | public output 对拍和 board end-to-end 正向 | 只有 profile 证明 NMS 成为瓶颈时再开新 phase |

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| dispatch / fallback | adopted | 编译期 exact type gate 加运行期 3x3 / EIGHT_CORNERS gate，未覆盖路径自然回到原标量循环 | production 源码、`run_test_compare` | 不改变 public API（公开接口） |
| 单 float intensity 跨步加载 | adopted | `PointXYZI` 的 intensity 是 AoS（结构数组）中的单 float 字段，适合 `vlse32.v` | asm gate、layout gate | 不覆盖 RGB 灰度 accessor |
| EIGHT_CORNERS response formula | adopted | 固定邻域公式可在每个 VL chunk（可变向量长度分块）内并行计算 | correctness、board summary | 只覆盖 3x3 |
| 标量复核 | adopted | NMS 对 response 浮点细差敏感，通过阈值附近和通过 first threshold 的 lane 用原标量公式复核 | public output checksum match | 增加少量标量工作，但 board 仍正向 |
| FOUR_CORNERS RVV | rejected for current scope | 早期 public output / NMS 风险和近 1x 控制结果不支持同轮采纳 | Phase 000 result、board fallback control | 若 workload 强依赖 FOUR_CORNERS，可单独建 phase |
| NMS RVV | not_now | 当前 EIGHT_CORNERS end-to-end 已明显正向，NMS 涉及排序、occupancy 和输出顺序 | board summary、evaluation | 需要 profile 指向 NMS 主导后再考虑 |
| 泛型点类型扩展 | deferred | exact `PointXYZI` gate 只是阶段性窄范围，泛型 traits / accessor 需要独立批准 | optimization roadmap | 新 phase 需补 traits、fallback、asm、board 和 doctor |

RVV helper 按行 strip-mining（条带化处理）推进。每个 chunk 先跨步加载中心点和 8 个邻域 intensity，
再计算四个方向响应候选，构造 threshold mask（阈值掩码），只把通过 `first_threshold_` 的 lane 写入
`response_`。写入后，标量复核只扫描响应接近或超过阈值的位置，避免 NMS 的排序和 tie 行为被公式
重排带来的浮点细差扰动。

## 标量路径与 RVV 路径差异

| 阶段 | 标量路径 | RVV 路径 | 证据 |
| --- | --- | --- | --- |
| 输入检查 | `initCompute()` 检查 organized、indices 和 window | 不改变 | 上游源码和 gtest |
| response grid | 双层循环逐点调用 `intensity_`，EIGHT_CORNERS 使用临时 vector 保存公式项 | 3x3 EIGHT_CORNERS 按 VL chunk 跨步加载 intensity 并写 `response_` | asm、board |
| `first_threshold_` | 低于阈值时不写响应 | RVV mask 控制写回；阈值附近再标量复核 | gtest、checksum |
| NMS | 排序、occupancy map、critical `push_back` | 保持标量 | public output 对拍 |
| fallback | 原路径处理所有未覆盖条件 | dispatch gate 未命中时进入原路径 | fallback/control tests |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `TrajkovicKeypoint2D::detectKeypoints` | production public path | response grid 与 NMS 主流程 | `compute()` | RVV helper 或标量 response loop | production boundary（生产边界） | `keypoints/include/pcl/keypoints/impl/trajkovic_2d.hpp` |
| `kTrajkovic2DPointXYZIRVVGated` | production dispatch / fallback | 限定 exact `PointXYZI`、默认 accessor 和 float intensity layout | `detectKeypoints()` | `trajkovic2DResponseGridRVV` | fallback coverage（回退覆盖） | `keypoints/include/pcl/keypoints/impl/trajkovic_2d.hpp` |
| `trajkovic2DResponseGridRVV` | production RVV helper | 计算 EIGHT_CORNERS response grid 并做标量复核 | `detectKeypoints()` | `response_` 和 NMS | production direct correctness + asm | `keypoints/include/pcl/keypoints/impl/trajkovic_2d.hpp` |
| `trajkovic2DScalarResponseAt` | scalar reconciliation | 用原标量公式复核阈值附近响应 | RVV helper | `response_` | numerical boundary（数值边界） | `keypoints/include/pcl/keypoints/impl/trajkovic_2d.hpp` |
| `src/test_trajkovic_2d_response_grid.cpp` | correctness gate | 对拍 public `compute()` 输出和 fallback | `run_test_compare` | QEMU std/RVV logs | correctness gate（正确性验收） | `test-rvv/keypoints/trajkovic_2d_response_grid/src/` |
| `src/bench_trajkovic_2d_response_grid.cpp` | bench wrapper | 真实 public entry 的 Std/RVV 计时与 checksum | board target | summary / manifest | board performance（板卡性能） | `test-rvv/keypoints/trajkovic_2d_response_grid/src/` |
| `script/generate_trajkovic_2d_evidence_manifest.py` | analysis script | 将 repeated board log 转成 Evidence Doctor manifest | `record_board_evidence_state` | `log/board/evidence_manifest.json` | Evidence Doctor input | `test-rvv/keypoints/trajkovic_2d_response_grid/script/` |
| `log/board/repeated-summary.md` | evidence output summary | 保存 5-run 统计、case role 和 checksum | board repeated target | evaluation / doc-rvv | production evidence summary（生产证据摘要） | `test-rvv/keypoints/trajkovic_2d_response_grid/log/board/` |

## 数值算例与 VL Chunk

以一行内部像素为例，`width = 320`、`window_size = 3` 时，某个 chunk 从 `(x = 1, y = 1)`
开始，`center_index = y * width + x`。RVV 用同一个 `vl` 读取：

| 向量 | 标量坐标 | 地址关系 |
| --- | --- | --- |
| `center` | `(x, y)` | `center_index` |
| `up` / `down` | `(x, y - 1)` / `(x, y + 1)` | `center_index - width` / `center_index + width` |
| `left` / `right` | `(x - 1, y)` / `(x + 1, y)` | `center_index - 1` / `center_index + 1` |
| diagonals | 四个对角邻域 | `center_index +/- width +/- 1` |

单个 lane 中，若 `center=10`、`up=12`、`down=8`、`left=9`、`right=14`、`upleft=11`、
`upright=13`、`downleft=7`、`downright=15`，则 `r0=(12-10)^2+(8-10)^2=8`，
`r2=(14-10)^2+(9-10)^2=17`；EIGHT_CORNERS 再计算 `r1` 和 `r3` 并取最小候选。
RVV 的每个 lane 做同一组公式，mask 只控制哪些 lane 写回。若响应接近阈值或通过阈值，
标量复核会用同一坐标重新跑原公式，保证后续 NMS 看到的响应与原路径一致。

## Bench 与证据

板卡性能只引用 repeated board summary，不使用 QEMU timing（QEMU 计时）作性能结论。`B/A`
定义为 `Std build ms / RVV build ms`，大于 1 表示 RVV 构建更快。

| case | role | runs | mean std ms | mean rvv ms | mean B/A | median B/A | checksum |
| --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| `eight_corners_320x240` | production-public | 5 | 50.232920 | 29.147240 | 1.725x | 1.743x | match |
| `eight_corners_641x481_tail` | production-public | 5 | 264.472800 | 183.589400 | 1.442x | 1.450x | match |
| `four_corners_320x240` | production-fallback-control | 5 | 29.871220 | 29.467840 | 1.014x | 1.000x | match |
| `four_corners_641x481_tail` | production-fallback-control | 5 | 202.176400 | 200.361200 | 1.009x | 1.003x | match |

EIGHT_CORNERS 两个规模是真实 production-public 证据。FOUR_CORNERS 行只验证 RVV 构建中回退控制项的
checksum 和分流成本，不作为 RVV 主路径收益证据。

## 正确性与高效性证据链

| 层级 | 当前证据 | 结论 | 不能证明的范围 |
| --- | --- | --- | --- |
| correctness（正确性） | `run_test_compare` Std/RVV 两个构建各 4 个 gtest 通过 | public `compute()` 输出、indices 和 fallback 语义在当前测试输入下保持一致 | 不覆盖其它点型、RGB accessor 和更大 window |
| QEMU path evidence（QEMU 路径证据） | QEMU 只运行 correctness 和日志形状 | 能证明二进制可运行、路径可触达 | 不证明目标硬件性能 |
| asm attribution（反汇编归属） | `check_production_rvv_asm` 匹配 `vlse32.v`、`vfmul.vv`、`vfmin.vv`、`vfdiv.vv`、`vmfge.vf`、`vse32.v` | 关键 RVV 指令在 production `detectKeypoints()` 内联范围内 | 不单独证明收益 |
| board performance（板卡性能） | Milkv-Jupiter 5-run summary，EIGHT_CORNERS mean B/A 为 1.725x 和 1.442x | 当前 production-public RVV 路径稳定正向 | 不证明其它入口或其它目标硬件 |
| Evidence Doctor | manifest 检查 Errors=0 / Warnings=0 / Suggestions=0 | 当前摘要没有脚本规则覆盖范围内的证据异常 | reviewer 仍需复核源码和文档边界 |

## Production Closeout

| 项 | 最终状态 | 证据 |
| --- | --- | --- |
| 生产补丁范围 | adopted | `trajkovic_2d.hpp` 新增 RVV helper、scalar reconciliation 和 public dispatch；public API 不变 |
| 覆盖范围 | adopted narrow scope | EIGHT_CORNERS / `PointXYZI` / 默认 accessor / 3x3 / organized full-cloud |
| 不覆盖范围 | scalar fallback / deferred | FOUR_CORNERS、泛型点类型、RGB accessor、大窗口和 NMS |
| fallback 矩阵 | adopted | `run_test_compare` 与 board fallback-control checksum match |
| production direct 证据 | passed | public `compute()` correctness、asm 和 board repeated |
| 证据决策 | `production-adopted-narrow-scope` | 用户本轮授权“板卡有收益即可采纳”，EIGHT_CORNERS 板卡证据正向且 doctor 无 findings |
| 回退边界 | 可回滚单文件 production patch | 若后续宽范围证据失败，保持当前 exact gate 或回滚 `trajkovic_2d.hpp` 的 RVV 分流即可恢复标量 |

## 遗留风险与后续条件

泛型 intensity 点类型仍未闭合。当前 exact `PointXYZI` gate 是阶段局部例外；若要扩大到
`PointXYZINormal` 或其它 single-float intensity 点型，需要读取泛型点类型策略，补 traits / offset /
layout gate、fallback tests、反汇编、板卡 repeated summary 和 Evidence Doctor。

RGB 灰度 accessor 不能复用当前证据。它不是单 float intensity 跨步加载，后续需要独立灰度语义对拍、
load strategy 和 production direct bench。

`window_size > 3` 不在当前采纳范围内。大窗口改变邻域 offset 和公式成本，不能只靠 3x3 EIGHT_CORNERS
结果外推。

NMS RVV 化当前不建议作为默认下一步。现有 production-public EIGHT_CORNERS 已有明显 end-to-end 收益；
NMS 涉及排序、occupancy map 和输出顺序，除非 profile（性能剖析）显示它成为采纳后主瓶颈，否则风险
高于当前收益需求。
