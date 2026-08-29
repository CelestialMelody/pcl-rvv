# Trajkovic 2D Response Grid Evaluation

## 范围和目标源码

目标源码是 `keypoints/include/pcl/keypoints/impl/trajkovic_2d.hpp`。本 topic 评估
`TrajkovicKeypoint2D::detectKeypoints()` 的 response grid（响应图）是否适合 RVV
生产接入。production public entry（真实公开入口）是
`TrajkovicKeypoint2D<PointXYZI, PointXYZI>::compute()`。

本轮最终采用范围冻结为：`PointXYZI`、默认 `IntensityFieldAccessor`（强度字段访问器）、
organized full-cloud public `compute()`、`window_size == 3`、EIGHT_CORNERS、单 float intensity
字段和 AoS（结构数组）布局。FOUR_CORNERS、非 RVV 构建、其它点型、其它 accessor 和非 3x3
window 继续走标量 fallback（回退路径）。

## 函数级结论

最终 EvidenceDecision（证据决策）为 `production-adopted-narrow-scope`。EIGHT_CORNERS 的
response grid 已接入 production RVV 路径，并由 QEMU correctness（QEMU 正确性验证）、反汇编归属、
Milkv-Jupiter 板卡 repeated benchmark（重复性能测试）和 Evidence Doctor（证据体检）闭合。

NMS（非极大值抑制）阶段保持标量。当前 end-to-end public `compute()` 在 EIGHT_CORNERS 两个规模上
仍有 1.725x 和 1.442x mean B/A，因此无需把排序、occupancy map 或输出 `push_back` 扩成 RVV 来支撑
本阶段采纳。

## 标量流程与 RVV 流程对照

| 阶段 | 标量路径 | RVV 采用状态 |
| --- | --- | --- |
| 初始化 | `initCompute()` 拒绝非 organized、indices 和非法 window | 不改变 |
| response grid | 双层 `j/i` 循环读取中心点和邻域 intensity，按 4/8 corners 公式写 `response_` | EIGHT_CORNERS 在 3x3 `PointXYZI` 默认 accessor 下按行做 VL chunk（可变向量长度分块），跨步读取 intensity 并写响应 |
| threshold | `first_threshold_` 以下不写响应，保留默认 0 | RVV 用 mask（掩码）只写通过阈值的 lane，并对接近阈值的响应做标量复核以稳定 NMS 输出 |
| FOUR_CORNERS | 原标量公式 | 保持标量 fallback；board summary 中只作为 control（控制项）检查 checksum 和分流开销 |
| NMS / output | 排序、occupancy、`push_back`、`keypoints_indices_` 更新 | 保持标量，保证输出顺序和对象状态不改变 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `TrajkovicKeypoint2D::detectKeypoints` | production public path | 真实 response grid 与 NMS 主流程 | `compute()` | RVV helper 或标量 response loop、NMS tail | production boundary（生产边界） | `keypoints/include/pcl/keypoints/impl/trajkovic_2d.hpp` |
| `kTrajkovic2DPointXYZIRVVGated` | production dispatch / fallback | 限定 `PointXYZI`、默认 accessor 和 float intensity layout | `detectKeypoints()` | RVV helper | fallback coverage（回退覆盖） | `keypoints/include/pcl/keypoints/impl/trajkovic_2d.hpp` |
| `trajkovic2DResponseGridRVV` | production RVV helper | 批量计算 EIGHT_CORNERS response grid | `detectKeypoints()` | `response_` / 标量复核 / NMS | production direct correctness + asm | `keypoints/include/pcl/keypoints/impl/trajkovic_2d.hpp` |
| `trajkovic2DScalarResponseAt` | production scalar helper | 对接近阈值的响应做标量公式复核，避免 NMS 排序被浮点细差扰动 | RVV helper | `response_` | numerical boundary（数值边界） | `keypoints/include/pcl/keypoints/impl/trajkovic_2d.hpp` |
| `src/test_trajkovic_2d_response_grid.cpp` | correctness gate | 对拍 public `compute()` 输出、keypoint indices 和 fallback | `run_test_compare` | QEMU std/RVV 日志 | correctness gate（正确性验收） | `test-rvv/keypoints/trajkovic_2d_response_grid/src/` |
| `src/bench_trajkovic_2d_response_grid.cpp` | production bench wrapper | 真实 public entry 的 Std/RVV 计时与 checksum | board `run_bench_compare` | summary / manifest | board performance（板卡性能） | `test-rvv/keypoints/trajkovic_2d_response_grid/src/` |
| `script/generate_trajkovic_2d_evidence_manifest.py` | analysis script | 把 repeated board log 转为 manifest，并把 FOUR_CORNERS 降级为 fallback control | `record_board_evidence_state` | `log/board/evidence_manifest.json` | Evidence Doctor input（证据体检输入） | `test-rvv/keypoints/trajkovic_2d_response_grid/script/` |
| `log/board/repeated-summary.md` | evidence output summary | 保存 board 5-run 统计和 case role | board repeated target | evaluation / doc-rvv | production evidence summary（生产证据摘要） | `test-rvv/keypoints/trajkovic_2d_response_grid/log/board/` |
| `doc-rvv/keypoints/trajkovic_2d_response_grid-RVV.zh.md` | production topic doc | 长期记录 adopted production behavior（已采纳生产行为） | reviewer / future worker | source、tests、evidence | long-term production doc（长期生产文档） | `doc-rvv/keypoints/` |

## 实现方式审计

| 实现维度 | 当前状态 | 证据 | 边界 / 恢复条件 |
| --- | --- | --- | --- |
| dispatch / fallback | adopted | `run_test_compare`、production 源码 | `__RVV10__`、`PointXYZI`、默认 accessor、3x3 和 EIGHT_CORNERS 才命中 RVV；其它路径回退标量。 |
| layout / traits gate | adopted narrow scope | `kTrajkovic2DPointXYZIRVVGated`、QEMU correctness | 当前使用 exact `PointXYZI`，不外推到泛型 intensity 点型。 |
| formula / FMA | adopted non-FMA explicit tree | QEMU correctness、asm | 使用显式 `vfadd/vfsub/vfmul/vfdiv/vfmin` 公式；未把 FMA contraction（融合乘加收缩）作为独立候选。 |
| scalar reconciliation | adopted | checksum match、gtest 输出对拍 | 只对接近 / 通过 `first_threshold_` 的点做标量复核，保护 NMS 排序和 public output。 |
| scalar tail | adopted | public output 对拍、board end-to-end | NMS 保持标量；当前 EIGHT_CORNERS end-to-end 正向，不需要本阶段 RVV 化。 |
| production scope | adopted narrow scope | board repeated + Evidence Doctor | 只覆盖 EIGHT_CORNERS `PointXYZI` 3x3 public `compute()`。 |

## 测试计划和 Bench 计划

| 测试 / target | 层级 | 作用 | 当前结果 |
| --- | --- | --- | --- |
| `run_test_compare` | production direct correctness | Std/RVV 两个构建分别跑 public `compute()`，覆盖 FOUR_CORNERS、EIGHT_CORNERS、tail width 和非 3x3 fallback | passed；两个构建各 4 个 gtest 全通过 |
| `check_production_rvv_asm` | asm attribution（反汇编归属） | 确认 RVV 指令出现在 `PointXYZI` public `detectKeypoints()` 内联范围 | passed；包含 `vlse32.v`、`vfmul.vv`、`vfmin.vv`、`vfdiv.vv`、`vmfge.vf`、masked `vse32.v` |
| `run_board_trajkovic_2d_repeated` | production public board bench | 5-run 比较真实 public entry 的 std/RVV 计时和 checksum | passed；EIGHT_CORNERS 稳定正向，checksum match |
| `run_board_evidence_doctor` | Evidence Doctor | 复核 manifest、checksum、run count、B/A 分布和 metadata | passed；Errors=0 / Warnings=0 / Suggestions=0 |
| `evidence_status` | evidence registry（证据登记） | 检查 summary、manifest、doctor 与文档引用是否 fresh | passed；registry fresh |

## 验证结果

### Correctness 和 QEMU

`make -C test-rvv/keypoints/trajkovic_2d_response_grid run_test_compare` 通过。QEMU 只作为 correctness、
路径和日志形状证据，不作为性能结论。

### 反汇编

`make -C test-rvv/keypoints/trajkovic_2d_response_grid check_production_rvv_asm` 通过。编译器把
`trajkovic2DResponseGridRVV` 内联到
`TrajkovicKeypoint2D<PointXYZI, PointXYZI, IntensityFieldAccessor<PointXYZI>>::detectKeypoints()`；
验收目标检查实际机器码中的关键 RVV 指令，而不是要求保留未内联 helper 符号。

### 板卡生产证据

板卡命令：

```bash
SSH_AUTH_SOCK=$SSH_AUTH_SOCK make -C test-rvv/keypoints/trajkovic_2d_response_grid run_board_trajkovic_2d_repeated
```

当前 board summary：

| case | role | runs | mean std ms | mean rvv ms | mean B/A | median B/A | checksum |
| --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| `eight_corners_320x240` | production-public | 5 | 50.232920 | 29.147240 | 1.725x | 1.743x | match |
| `eight_corners_641x481_tail` | production-public | 5 | 264.472800 | 183.589400 | 1.442x | 1.450x | match |
| `four_corners_320x240` | production-fallback-control | 5 | 29.871220 | 29.467840 | 1.014x | 1.000x | match |
| `four_corners_641x481_tail` | production-fallback-control | 5 | 202.176400 | 200.361200 | 1.009x | 1.003x | match |

`B/A = Std build ms / RVV build ms`。FOUR_CORNERS 在 RVV build 中按设计回退标量，因此它的近 1x
结果只证明 checksum 和分流开销边界，不参与 RVV 采纳判断。

Evidence paths：

- `test-rvv/keypoints/trajkovic_2d_response_grid/log/board/repeated-summary.md`
- `test-rvv/keypoints/trajkovic_2d_response_grid/log/board/evidence_manifest.json`
- `test-rvv/keypoints/trajkovic_2d_response_grid/log/board/evidence_doctor.md`
- `test-rvv/keypoints/trajkovic_2d_response_grid/log/evidence_registry.json`

## 生产接入后的最终证据更新

| 项 | 状态 | 证据 |
| --- | --- | --- |
| `production_patch_scope` | adopted | `keypoints/include/pcl/keypoints/impl/trajkovic_2d.hpp` 新增 RVV helper、scalar reconciliation 和 public dispatch。 |
| `covered_path` | adopted narrow scope | EIGHT_CORNERS / `PointXYZI` / default accessor / 3x3 / organized full-cloud public `compute()`。 |
| `fallback_matrix` | adopted | FOUR_CORNERS、非 RVV 构建、非 `PointXYZI`、非默认 accessor 和非 3x3 window 回退标量；`run_test_compare` 覆盖主要回退语义。 |
| `production_direct_tests` | passed | `run_test_compare` 通过，board checksum match。 |
| `production_asm` | passed | `check_production_rvv_asm` 通过，RVV 指令归属到 public `detectKeypoints()` 内联范围。 |
| `production_board_bench` | positive | EIGHT_CORNERS mean B/A 为 1.725x 和 1.442x。 |
| `decision_delta` | confirmed and narrowed | 早期计划覆盖 4/8 corners；实际采纳只保留 EIGHT_CORNERS RVV，FOUR_CORNERS 因 public output 风险和收益不足保持标量。 |

## 生产接入判断

本轮用户授权“板卡上的测试结果如果显示有收益即可采纳”。当前 EIGHT_CORNERS production-public
证据满足采纳条件：correctness 通过、反汇编通过、板卡 repeated 正向且 Evidence Doctor 无 findings。
因此 production patch 被记录为 adopted production behavior，并创建长期主题文档
`doc-rvv/keypoints/trajkovic_2d_response_grid-RVV.zh.md`。

## 未覆盖范围

- 泛型 intensity 点型没有闭合。当前 exact `PointXYZI` gate 是阶段性窄范围生产接入，不是
  PointXYZI-like 或任意带 intensity 点型的最终泛型结论。
- RGB 灰度 accessor 没有闭合。它不是单 float intensity 跨步加载，不能复用当前 `vlse32.v` 证据。
- `window_size > 3` 没有闭合。更大窗口会改变邻域 offset 和公式成本，需要独立测试与板卡证据。
- NMS RVV 化当前不建议继续推进。已有 EIGHT_CORNERS end-to-end 收益足够，且 NMS 涉及排序、
  occupancy 和输出顺序，语义风险高于当前收益需求。
