# Trajkovic 3D RVV 函数级评估

## 范围和结论

目标源码是 `keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp`。当前已采纳两个窄范围 production RVV（生产 RVV）路径：`TrajkovicKeypoint3D::detectKeypoints()` 在 `FOUR_CORNERS` / `EIGHT_CORNERS`、`window_size_=3`、precomputed normals（预计算法线）、organized dense full cloud（有组织稠密完整点云）且点 / 法线字段满足 float AoS layout（结构数组布局）时，用 RVV 计算 response map（响应图）。NMS（非极大值抑制）和输出构造保持标量。

最终 EvidenceDecision（证据决策）为 `production-adopted narrow scope`。生产 public board（板卡公开入口性能测试）在 `public_four_corners_320x240` 上 5-run median speedup 为 1.790x、在 `public_eight_corners_320x240` 上 5-run median speedup 为 1.677x，Evidence Doctor（证据体检）为 Errors=0、Warnings=0、Suggestions=4。

## 函数族作用速览

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | 与主流程关系 | RVV 判断 |
| --- | --- | --- | --- | --- |
| `TrajkovicKeypoint3D::initCompute()` | 准备 normals，如果调用者没有提供法线则由 integral image normal estimator（积分图法线估计器）生成。 | 读 `input_`、`normals_`；可能写 `normals_`。 | 前置状态准备。 | 不在本 topic 优化范围内。 |
| `TrajkovicKeypoint3D::detectKeypoints()` | 生成 response map，排序并执行 NMS，输出 `PointOutT` 和 keypoint indices。 | 读 organized input / normals；写 `response` 和输出 cloud。 | 当前 production 入口。 | 接管 `FOUR_CORNERS` 与 `EIGHT_CORNERS` response map。 |
| `trajkovic3DFourCornersResponseRVV` | 生产 RVV helper，按 VL chunk 计算四邻域法线差异和响应值。 | 读 point / normal AoS 字段；写 `response.points`。 | `detectKeypoints()` 内部短路分流。 | 已采纳窄范围。 |
| `trajkovic3DEightCornersResponseRVV` | 生产 RVV helper，按 VL chunk 计算八邻域法线差异和响应值。 | 读 point / normal AoS 字段；写 `response.points`。 | `detectKeypoints()` 内部短路分流。 | 已采纳窄范围。 |
| NMS 排序与 occupancy map | 按响应值排序，屏蔽局部邻域并 push 输出。 | 读 `response` 和 `indices_`；写输出。 | response 之后的标量阶段。 | 暂缓，输出顺序风险高。 |

## 标量流程与 RVV 流程对照

| 阶段 | 标量路径 | RVV 路径 | 证据 |
| --- | --- | --- | --- |
| response 初始化 | `response.resize()` 后逐项置零或默认零值。 | RVV helper 先 `std::fill` 清零。 | gtest border / tiny cloud 覆盖。 |
| finite gate | `isFinite(point)`、center normal finite、neighbor `getNormalOrNull()`。 | 向量 mask 先检查 point 和 center normal，非法邻域 normal merge 为零向量。 | `MatchesScalarForInteriorFiniteAndInvalidNormals`、`TreatsInvalidNeighborNormalsAsNullNormals`。 |
| 四邻域公式 | 计算 up/down/left/right 的 normal diff、平方和、sqrt、quadratic branch。 | 每个 VL chunk 用 strided load 读取字段，向量计算 dot、sqrt、min 和 branch mask。 | response diagnostic correctness、asm 和 board。 |
| 八邻域公式 | 计算对角线邻域的 normal diff、平方和、sqrt、quadratic branch。 | 每个 VL chunk 复用同类 strided load 与 FMA 组织。 | response diagnostic correctness、asm 和 board。 |
| threshold | `d < first_threshold_` 时保持零响应。 | active mask 只对阈值通过 lane 写回。 | `LeavesBorderAndRejectedResponsesZero`。 |
| NMS 和输出 | 标量 sort、occupancy map、push output。 | 保持标量。 | public gtest 和 public bench checksum。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `TrajkovicKeypoint3D::detectKeypoints()` | production public entry | 真实公开入口后的主实现函数。 | `Detector::compute(output)` | response map、NMS、输出构造 | production boundary（生产边界） | `keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp` |
| `trajkovic3DFourCornersRVVSupported` | production dispatch / fallback | 编译期 traits 和 strong AoS layout gate（严格结构数组布局门控）。 | `detectKeypoints()` | RVV helper 或标量主体 | fallback coverage（回退覆盖） | `keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp` |
| `trajkovic3DFourCornersResponseRVV` | production RVV helper | 计算 adopted scope 的 response map。 | `detectKeypoints()` | 标量 NMS | asm attribution（反汇编归属）和 production speedup | `keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp` |
| `trajkovic3DEightCornersResponseRVV` | production RVV helper | 计算 adopted scope 的 response map。 | `detectKeypoints()` | 标量 NMS | asm attribution（反汇编归属）和 production speedup | `keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp` |
| `computeFourCornersResponseStd/RVV` | diagnostic reference | 测试专用 response helper。 | gtest / diagnostic bench | response compare 和 Phase 000 board | production-shaped diagnostic（生产形态诊断） | `test-rvv/keypoints/trajkovic_3d/include/impl/trajkovic_3d_response.hpp` |
| `computeEightCornersResponseStd/RVV` | diagnostic reference | 测试专用 response helper。 | gtest / diagnostic bench | response compare 和 Phase 020 board | production-shaped diagnostic（生产形态诊断） | `test-rvv/keypoints/trajkovic_3d/include/impl/trajkovic_3d_response.hpp` |
| `test_trajkovic_3d.cpp` | correctness gate | response、public output、fallback 测试。 | `run_test_compare` | QEMU logs | correctness gate（正确性验收） | `test-rvv/keypoints/trajkovic_3d/src/test_trajkovic_3d.cpp` |
| `bench_trajkovic_3d.cpp` | bench wrapper | diagnostic 和 public benchmark。 | board targets | summary / manifest | board performance（板卡性能） | `test-rvv/keypoints/trajkovic_3d/src/bench_trajkovic_3d.cpp` |
| `generate_trajkovic_3d_evidence_manifest.py` | analysis script | 生成 manifest 和 summary。 | evidence Make targets | Evidence Doctor / registry | evidence manifest（证据清单） | `test-rvv/keypoints/trajkovic_3d/script/generate_trajkovic_3d_evidence_manifest.py` |
| Phase 020 summary | evidence output | production public 5-run 摘要。 | board repeated target | evaluation / doc-rvv | current performance truth（当前性能事实） | `test-rvv/keypoints/trajkovic_3d/log/board/repeated_phase020_eight_corners_production_public/summary.md` |
| production topic doc | documentation section | 长期 production 行为说明。 | reviewer / maintainer | future recovery | long-term adopted behavior（长期采纳行为） | `doc-rvv/keypoints/trajkovic_3d-RVV.zh.md` |

## 实现方式审计

| 实现维度 | 当前状态 | 证据 | 边界 / 恢复条件 |
| --- | --- | --- | --- |
| dispatch / fallback | adopted | `method_ == FOUR_CORNERS || method_ == EIGHT_CORNERS` 且 `window_size_ == 3 && input.is_dense && normals.is_dense` 时 RVV，否则标量。 | Non-dense、其它 window 和非 RVV 构建保持标量。 |
| layout / traits gate | adopted narrow | `RVVXYZAoSFloatLayout<PointInT>` 和 topic-local `trajkovic3DNormalAoSFloatLayout<NormalT>`，分别证明 xyz / normal 字段、POD size、stride 和 offset alignment。 | 生产证据只覆盖 `PointXYZ + Normal`；更宽点型需要新 phase。 |
| formula / FMA | adopted | RVV 使用 `vfmul` / `vfmacc` 表达 dot 和差值，checksum 与 public output 对齐。 | 没有单独做 FMA on/off 消融；当前不是 family-selection 问题。 |
| scalar tail | adopted | NMS、sort、occupancy 和输出构造保留标量。 | 若 profile 证明 NMS 成本主导，另开 phase。 |
| diagnostic to production | closed | Phase 000 diagnostic positive，Phase 010 public positive，Phase 020 public positive。 | 最终性能结论只采用 public board 证据。 |

## 测试和 Bench 计划结果

| 类型 | 命令 / 路径 | 结果 | 边界 |
| --- | --- | --- | --- |
| QEMU correctness | `make -C test-rvv/keypoints/trajkovic_3d run_test_compare` | Std/RVV 各 10 个 gtest 通过。 | QEMU 不作为性能结论。 |
| ASM | `make -C test-rvv/keypoints/trajkovic_3d check_trajkovic_3d_rvv_asm` | 命中 `vlse32.v`、`vfmul.vv`、`vfsqrt.v`。 | 证明指令存在和归属，不证明性能。 |
| Diagnostic board | `log/board/repeated_phase000_four_corners_response/summary.md` | response-only median 2.317x 到 3.596x。 | 只作为进入 production probe 的理由。 |
| Production board | `log/board/repeated_phase020_eight_corners_production_public/summary.md` | public compute median 1.790x / 1.677x，0/5 低于 1.0x。 | 当前 production 性能主证据。 |
| Evidence Doctor | `log/board/repeated_phase020_eight_corners_production_public/evidence_doctor.md` | Errors=0，Warnings=0，Suggestions=4。 | 建议补环境和 binary metadata，不阻塞当前采纳。 |

## Fallback Matrix

| 场景 | 当前行为 | 证据 |
| --- | --- | --- |
| 非 RVV 构建 | 无 `__RVV10__` 时编译不到 RVV helper，执行原标量主体。 | Std gtest / bench binary。 |
| `EIGHT_CORNERS` | adopted narrow scope | Phase 020 已在 public compute 上正向。 | 回退只保留在 non-dense、非 3x3 或未验证布局。 |
| 非 3x3 window | 不进入 RVV 分流。 | 源码 gate。 |
| `input.is_dense == false` 或 `normals.is_dense == false` | 回退标量，保留原 `isFinite` 和 `getNormalOrNull()` 语义。 | public invalid / tail raw board control 接近 1.0x；gtest 覆盖 invalid response 语义。 |
| tiny cloud | 输出为空或无 response 写回。 | `PublicComputeTinyCloudFallsBackWithoutResponses`。 |
| 未证明点型或 layout | traits/layout gate 不满足时编译期不进入 RVV 分支。 | `FourCornersGateCoversPointXYZAndNormal` 覆盖当前命中类型；未覆盖类型留待 expansion phase。 |

## 生产接入后的最终证据更新

`production_patch_scope`: 修改 `keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp`，不改变 public API（公开接口）。新增内部 RVV helper、finite mask、normal load 和 response dispatch；原标量代码仍保留并作为 fallback。

`covered_path`: `PointXYZ` input、`PointXYZI` output、`pcl::Normal` normals、`float` AoS、organized dense 320x240 public compute with precomputed normals。

`production_board_bench`: `public_four_corners_320x240` repeated board median 1.790x，min 1.762x，max 1.820x；`public_eight_corners_320x240` repeated board median 1.677x，min 1.613x，max 1.705x。

`decision_delta`: Phase 000 的 response-only speedup 更高，因为不包含 NMS 和输出构造。Phase 020 public evidence 仍 positive，因此当前 production patch 采纳，并把双 public case 作为 production 结论。

## 未闭合项和后续选择

| 方向 | 当前状态 | 原因 | 恢复条件 |
| --- | --- | --- | --- |
| 更宽点类型 / `NormalT` | deferred | traits gate 已有基础，但当前 production evidence 只覆盖 `PointXYZ + Normal`。 | point-type expansion phase 覆盖代表点型、fallback、public bench 和 Evidence Doctor。 |
| normal estimation | not_applicable here | normal 生成属于 features/integral image normal estimator，不是本文件 response map。 | 若 public profile 显示 normal estimation 主导，转到对应 topic。 |
| NMS RVV | deferred | sort、occupancy map 和输出顺序风险高。 | 需要 profile 证明 NMS 成本主导，并补输出顺序 correctness。 |
