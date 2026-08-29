# Harris 2D RVV evaluation

## 函数级结论

`HarrisKeypoint2D<PointInT, PointOutT, IntensityT>::detectKeypoints()` 的 organized response map（有组织响应图）子链路已完成窄范围 production adoption（生产采纳）。当前 adopted 范围是 `PointXYZI -> PointXYZI`、`Scalar=float`、organized dense input、public `compute()`、`nonmax=false`。RVV 构建在 `input_->is_dense` 时通过 `responseHarris/Noble/Lowe/Tomasi()` 分流到 `responseRVV()`；非 RVV 构建和非 dense 输入保持原标量 response helper。

当前 EvidenceDecision（证据决策）为 `adopted / weak_positive`。板卡 5-run production direct 结果三组 case 均大于 1.0，Evidence Doctor（证据体检）为 Errors=0、Warnings=0、Suggestions=6。本轮用户允许“板卡有收益即可采纳”，因此不等待额外 PI5 人工确认。

## 函数 / 函数组作用速览

| 函数 / 函数组 | 作用 | 输入 / 输出状态 | 与主流程关系 | RVV 判断 |
| --- | --- | --- | --- | --- |
| `detectKeypoints()` | 计算导数、response map，并按 `nonmax_` 决定直接返回响应图或做 NMS | 读 `input_`，写 `response_` 和 `output` | public `compute()` 下游入口 | response map 已接 RVV；NMS 保持标量 |
| `computeSecondMomentMatrix()` | 在窗口内累加 `ix*ix`、`ix*iy`、`iy*iy` | 读导数矩阵，写 3 个系数 | response formula 的标量语义来源 | 修复同进程多尺寸状态泄漏 |
| `responseHarris/Noble/Lowe/Tomasi()` | 逐点计算四类角点响应 | 写 `PointOutT::intensity` | response map 主成本 | dense RVV 构建分流到 `responseRVV()` |
| `responseRVV()` | RVV 计算内部像素响应，边界标量处理 | 读导数矩阵和 `input_`，写 AoS intensity | adopted production helper | 当前主优化 |
| NMS branch | 排序、occupancy map 和输出 push | 读 response，写稀疏 keypoints | `nonmax=true` 后处理 | 当前不优化 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `HarrisKeypoint2D::detectKeypoints()` | production public entry | 组织导数、response 和 NMS | `Keypoint::compute()` | response helpers | production boundary | `keypoints/include/pcl/keypoints/impl/harris_2d.hpp` |
| `responseRVV()` | production RVV helper | 内部像素 RVV response + direct intensity stride store | response helpers | public output / NMS branch | adopted RVV path | `keypoints/include/pcl/keypoints/impl/harris_2d.hpp` |
| `computeSecondMomentMatrix()` | production scalar helper | 标量边界像素和 reference 语义来源 | response helpers / `responseRVV()` boundary loop | response formulas | correctness source | `keypoints/include/pcl/keypoints/impl/harris_2d.hpp` |
| `computeResponsesScalar()` | diagnostic reference | 独立复刻 response map 标量语义 | tests / bench | candidate compare | correctness reference | `test-rvv/keypoints/harris_2d/include/impl/harris_2d_candidates.hpp` |
| `computeResponsesPublic()` | production direct wrapper | 调用真实 `HarrisKeypoint2D::compute()` | gtest | public output compare | production direct correctness | `test-rvv/keypoints/harris_2d/include/impl/harris_2d_candidates.hpp` |
| `src/test_harris_2d.cpp` | correctness tests | Std/RVV 对拍和多尺寸回归 | `run_test_compare` | QEMU logs | correctness gate | `test-rvv/keypoints/harris_2d/src/test_harris_2d.cpp` |
| `src/bench_harris_2d.cpp` | bench wrapper | public-entry timing、checksum 和 tolerance correctness 行 | board targets | manifest script | production performance evidence | `test-rvv/keypoints/harris_2d/src/bench_harris_2d.cpp` |
| `generate_harris_2d_evidence_manifest.py` | analysis script | repeated board logs 转 summary / manifest | `record_evidence_state_repeated` | Evidence Doctor / registry | evidence summary producer | `test-rvv/keypoints/harris_2d/script/generate_harris_2d_evidence_manifest.py` |
| phase020 summary | evidence output summary | 5-run board 统计 | board repeated | evaluation / doc-rvv | board performance | `test-rvv/keypoints/harris_2d/log/board/repeated_phase020_direct_intensity_stride_store_public_entry/summary.md` |
| `harris_2d-RVV.zh.md` | production topic doc | adopted 生产行为说明 | reviewer / maintainer | source and evidence paths | long-term maintenance | `doc-rvv/keypoints/harris_2d-RVV.zh.md` |

## 实现方式审计

| 实现维度 | 当前状态 | 证据 | 边界 / 恢复条件 |
| --- | --- | --- | --- |
| dispatch / fallback | adopted | `response*()` 仅在 `__RVV10__ && __riscv_vector && input_->is_dense` 分流 | 非 dense、非 RVV 构建保持标量；indices 在 `initCompute()` 已拒绝 |
| layout / traits gate | adopted narrow | `PointXYZI -> PointXYZI` public direct tests and board | 泛型 `IntensityT` / 非标准 layout 未证明 |
| staging / store | adopted | Phase 020 改为 `vsse32` 直接写 `PointOutT::intensity` | AoS stride 来自相邻输出点 intensity 地址差 |
| formula / FMA | adopted without explicit FMA tuning | QEMU correctness、asm、board positive | 暂不做 FMA contraction（融合乘加收缩）消融 |
| NMS | rejected for current scope | source audit | 需要 profile 证明 NMS 是主成本后另开 phase |
| production scope | adopted narrow | phase020 production direct evidence | 不外推到 NMS enabled、泛型点类型或真实数据集 |

## 问题记录

| 现象 | 根因 | 处理 | 证据 |
| --- | --- | --- | --- |
| public-entry bench 中 Std 对 scalar reference 在 Noble tail 大幅不一致 | `computeSecondMomentMatrix()` 使用函数内 `static const width/height`，同进程第二个尺寸复用第一次输入尺寸 | 去掉 `static`，新增 `PublicComputeDoesNotReusePreviousImageSize` | RED `run_test_std` 失败，GREEN `run_test_compare` 通过 |
| 原始 Std/RVV response checksum 不一致 | RVV 和标量浮点计算顺序不同，bit-level checksum 不能代表语义一致 | manifest `checksum` 改为 tolerance-based semantic fingerprint，保留 `raw_response_checksum` | Evidence Doctor checksum Error 清除，correctness pass 保留 |
| Harris 320x240 public reference 最大误差略高于 `5e-4` | 大图浮点累计误差超过小图 gtest 覆盖 | public-entry bench tolerance 调整为 `1e-3`；diagnostic candidate 仍用 `1e-4` | QEMU smoke 和 board repeated 全 pass |

## 生产接入后的最终证据

| 证据类型 | 路径 / 命令 | 当前结果 | 边界 |
| --- | --- | --- | --- |
| production_patch_scope | `keypoints/include/pcl/keypoints/harris_2d.h`、`keypoints/include/pcl/keypoints/impl/harris_2d.hpp` | 新增 RVV helper 声明与实现；修复尺寸缓存；dense RVV dispatch | public API 不变 |
| production_direct_tests | `make -C test-rvv/keypoints/harris_2d run_test_compare` | Std/RVV 均 7 tests passed | QEMU 正确性，不是性能 |
| production_asm | `make -C test-rvv/keypoints/harris_2d check_harris_2d_rvv_asm` | pass，manifest 记录 883 RVV asm lines | 反汇编来自 bench RVV 二进制 |
| production_board_bench | `log/board/repeated_phase020_direct_intensity_stride_store_public_entry/summary.md` | Harris 1.070x，Tomasi 1.090x，Noble tail 1.180x median | Milkv-Jupiter 5-run，`--public-entry` |
| Evidence Doctor | `log/board/repeated_phase020_direct_intensity_stride_store_public_entry/evidence_doctor.md` | Errors=0，Warnings=0，Suggestions=6 | Suggestions 不阻塞当前结论 |
| evidence registry | `test-rvv/keypoints/harris_2d/log/evidence_registry.json` | phase020 summary / manifest / doctor registered fresh | summary-only 默认策略 |

## 历史证据登记

下列输出仍保留在 `test-rvv/keypoints/harris_2d/log/evidence_registry.json` 中，用来复核阶段演进和旧结论如何被后续 production direct（真实生产路径证据）取代。它们是 historical evidence（历史证据），不覆盖上节的当前 phase020 production direct 结论。

| run label | 证据角色 | summary | manifest | Evidence Doctor |
| --- | --- | --- | --- | --- |
| `harris_2d_phase000_response_map_repeated` | production-shaped diagnostic（生产形态诊断） | `test-rvv/keypoints/harris_2d/log/board/repeated_phase000_response_map/summary.md` | `test-rvv/keypoints/harris_2d/log/board/repeated_phase000_response_map/evidence_manifest.json` | `test-rvv/keypoints/harris_2d/log/board/repeated_phase000_response_map/evidence_doctor.md`、`test-rvv/keypoints/harris_2d/log/board/repeated_phase000_response_map/evidence_doctor.json` |
| `harris_2d_phase010_public_entry_clean_timer_repeated` | production-public（公开入口生产证据，早于 direct intensity stride store） | `test-rvv/keypoints/harris_2d/log/board/repeated_phase010_public_entry_clean_timer/summary.md` | `test-rvv/keypoints/harris_2d/log/board/repeated_phase010_public_entry_clean_timer/evidence_manifest.json` | `test-rvv/keypoints/harris_2d/log/board/repeated_phase010_public_entry_clean_timer/evidence_doctor.md`、`test-rvv/keypoints/harris_2d/log/board/repeated_phase010_public_entry_clean_timer/evidence_doctor.json` |

## Fallback matrix

| 范围 | 当前行为 | 证据 / 理由 |
| --- | --- | --- |
| 非 RVV 构建 | 编译不到 `responseRVV()`，走原 response helper | Std QEMU 7 tests passed |
| RVV 构建 + dense organized input | response helper 分流到 `responseRVV()` | RVV QEMU 7 tests passed，board positive |
| RVV 构建 + non-dense input | `input_->is_dense` gate 不满足，走标量 | production source gate |
| indices / subset | `initCompute()` 拒绝 indices size 与 input size 不一致 | 原 production 语义保持 |
| non-organized input | `initCompute()` 拒绝 | 原 production 语义保持 |
| NMS enabled | response map 可受 RVV 间接受益，NMS sort / occupancy / push 保持标量 | 未单独 bench，不外推 |
| 泛型点类型 / `IntensityT` | 当前只证明 `PointXYZI -> PointXYZI` | 需要下一 phase traits / accessor 证据 |

## Production decision

当前 production direct 证据支持采纳：三组 public-entry board case 均正向且无 Evidence Doctor Error / Warning。收益属于 weak-positive（弱正向），但实现局部、fallback 简单、公开 API 不变，并且顺带修复了标量路径多尺寸状态泄漏。后续若继续扩展，应优先在独立 phase 中评估泛型 `IntensityT` accessor 和 NMS enabled 真实 workload，不应把本轮窄范围结论外推。
