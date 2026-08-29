# Harris 2D RVV 优化说明

## 当前状态

`HarrisKeypoint2D<PointXYZI, PointXYZI>` 的 organized response map（有组织响应图）路径已采用 RVV 优化。当前生产范围是 `Scalar=float`、organized dense input（有组织密集输入）、public `compute()`、`nonmax=false`。RVV 构建在 `__RVV10__ && __riscv_vector && input_->is_dense` 时由 `responseHarris/Noble/Lowe/Tomasi()` 分流到 `responseRVV()`；其它构建或非 dense 输入保持标量 response helper。

该文档只描述已采纳的 production behavior（生产行为）。诊断阶段计划、历史 bench 和候选取舍主归属在 `test-rvv/keypoints/harris_2d/doc/`。

## 函数语义

Harris 2D 要求输入点云是 organized cloud（有组织点云），并且不支持 subset indices（子集索引）。`detectKeypoints()` 先按图像宽高计算 intensity（强度）在 row / column 方向的导数矩阵，再调用 `responseHarris()`、`responseNoble()`、`responseLowe()` 或 `responseTomasi()` 生成每个点的 corner response（角点响应）。当 `nonmax_` 为 false 时，完整 response map 直接作为输出；当 `nonmax_` 为 true 时，后续还会排序、维护 occupancy map（占用图）并筛选局部极大值。

标量 response helper 对每个点调用 `computeSecondMomentMatrix()`，在窗口内累加 `ix*ix`、`ix*iy` 和 `iy*iy`，再根据 method 计算 Harris、Noble、Lowe 或 Tomasi response。本轮同时修复了 `computeSecondMomentMatrix()` 中 `width` / `height` 函数内 static 缓存问题，避免同进程连续处理不同尺寸输入时复用旧尺寸。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| dispatch / fallback | adopted | dense RVV 路径局部、公开 API 不变，fallback 简单 | QEMU 7 tests pass | 非 dense 和非 RVV 构建走标量 |
| direct intensity stride store | adopted | 直接写回 `PointOutT::intensity`，减少中间 response buffer 和末尾拷贝 | phase020 board positive | 只证明 `PointXYZI` AoS intensity layout |
| boundary scalar tail | adopted | 边界像素窗口裁剪复杂，保留标量更稳 | correctness tests pass | 只占边界行列，不主导当前 case |
| NMS | not_now | sort、occupancy map 和 OpenMP critical 输出顺序风险高 | source audit | profile 证明 NMS 主导后另开 phase |
| generic `IntensityT` | deferred | 任意点类型 intensity accessor 和字段 offset 未审计 | not_run | 需要泛型点类型扩展 phase |

`responseRVV()` 先初始化输出点的 `x/y/z/intensity`，并用标量路径处理边界像素。内部像素按 VL chunk（可变向量长度分块）连续读取 `derivatives_rows_` 和 `derivatives_cols_`，累加 `covar_xx/covar_xy/covar_yy`，再按 method 计算 response。Tomasi 使用向量 `sqrt`，Harris/Noble/Lowe 使用 det / trace 相关公式。最终通过 `vsse32` 按 AoS（结构数组）stride 直接写到相邻输出点的 intensity 字段。

一个 3x3 Harris 内部点的概念流程是：对当前 lane 的邻域读取 `ix/iy`，累加 4 个窗口采样点贡献，得到 `trace=covar_xx+covar_yy` 和 `det=covar_xx*covar_yy-covar_xy*covar_xy`，再计算 `0.04 + det - 0.04*trace*trace`。RVV 版本把相邻像素作为 lane 并行处理；边界点仍由标量 `computeSecondMomentMatrix()` 执行相同窗口裁剪。

## 范围和 fallback

| 范围 | 当前行为 | 证据 / 原因 |
| --- | --- | --- |
| 非 RVV 构建 | 不声明也不编译 `responseRVV()`，保持标量 | Std QEMU 7 tests pass |
| RVV 构建 + dense organized input | response helper 分流到 `responseRVV()` | RVV QEMU 7 tests pass；board repeated positive |
| RVV 构建 + non-dense input | `input_->is_dense` gate 不满足，走标量 | production source gate |
| non-organized input | `initCompute()` 拒绝 | 原语义保持 |
| subset indices | `initCompute()` 拒绝 | 原语义保持 |
| NMS enabled | response map 可受 RVV 间接受益，NMS 本身保持标量 | 未做 NMS 专项 bench |
| 泛型点类型 / `IntensityT` | 当前不声明已证明 | 需要 traits / accessor / layout 证据 |
| `Scalar=double` | 不适用当前 `PointXYZI` float response 证据 | 未测试 |

## Bench 与证据

当前 production direct board（真实生产入口板卡）证据来自：

- summary：`test-rvv/keypoints/harris_2d/log/board/repeated_phase020_direct_intensity_stride_store_public_entry/summary.md`
- manifest：`test-rvv/keypoints/harris_2d/log/board/repeated_phase020_direct_intensity_stride_store_public_entry/evidence_manifest.json`
- Evidence Doctor：`test-rvv/keypoints/harris_2d/log/board/repeated_phase020_direct_intensity_stride_store_public_entry/evidence_doctor.md`
- run label：`harris_2d_phase020_direct_intensity_stride_store_public_entry_repeated`

| case | 入口 / 参数 | median speedup | B/A < 1 | 证明点 |
| --- | --- | ---: | ---: | --- |
| `harris2d_harris_320x240` | public `compute()`，Harris，3x3，320x240 | 1.070x | 0/5 | 常规 response map |
| `harris2d_tomasi_320x240` | public `compute()`，Tomasi，3x3，320x240 | 1.090x | 0/5 | 含向量 `sqrt` 的公式 |
| `harris2d_noble_tail_641x481` | public `compute()`，Noble，5x5，641x481 | 1.180x | 0/5 | tail lanes 和较大窗口 |

QEMU（仿真器）只用于 correctness（正确性）和路径形状验证，不作为性能结论。反汇编 gate 使用 `make -C test-rvv/keypoints/harris_2d check_harris_2d_rvv_asm`，manifest 记录 `asm_rvv_line_count=883`。

## 正确性与高效性证据链

| 证据层 | 当前结果 | 路径 / 命令 |
| --- | --- | --- |
| correctness | Std/RVV 均 7 tests passed；包含 public entry 和多尺寸回归 | `make -C test-rvv/keypoints/harris_2d run_test_compare` |
| path / asm | RVV bench 二进制包含目标 RVV float 指令，归属到 public compute 内联 `responseRVV()` | `make -C test-rvv/keypoints/harris_2d check_harris_2d_rvv_asm` |
| performance | 板卡 5-run 三组 case median 1.07x / 1.09x / 1.18x，均无反向 run | phase020 summary |
| Evidence Doctor | Errors=0，Warnings=0，Suggestions=6 | phase020 `evidence_doctor.md` |
| registry | summary / manifest / doctor 已登记 fresh | `test-rvv/keypoints/harris_2d/log/evidence_registry.json` |

Evidence Doctor 的 Suggestions 来自环境 metadata 和 binary identity 缺失；当前没有 Warning 或 Error。若后续要发布更严格的性能报告，应补 taskset、governor、freq、temperature 和 binary hash。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `detectKeypoints()` | production public entry | 计算导数、response 和可选 NMS | `Keypoint::compute()` | response helpers | production boundary | `keypoints/include/pcl/keypoints/impl/harris_2d.hpp` |
| `responseRVV()` | production RVV helper | 内部像素 RVV response，直接写 intensity | response helpers | output / NMS branch | adopted RVV path | `keypoints/include/pcl/keypoints/impl/harris_2d.hpp` |
| `computeSecondMomentMatrix()` | production scalar helper | 边界像素和标量 response 的二阶矩 | response helpers | formulas | correctness source | `keypoints/include/pcl/keypoints/impl/harris_2d.hpp` |
| `src/test_harris_2d.cpp` | correctness tests | diagnostic 和 public direct 对拍 | `run_test_compare` | QEMU logs | correctness gate | `test-rvv/keypoints/harris_2d/src/test_harris_2d.cpp` |
| `src/bench_harris_2d.cpp` | bench wrapper | public-entry timing 和 correctness 行 | board targets | manifest script | performance evidence | `test-rvv/keypoints/harris_2d/src/bench_harris_2d.cpp` |
| `generate_harris_2d_evidence_manifest.py` | analysis script | repeated logs 转 summary / manifest | `record_evidence_state_repeated` | Evidence Doctor / registry | summary producer | `test-rvv/keypoints/harris_2d/script/generate_harris_2d_evidence_manifest.py` |
| phase020 result | phase closeout | 阶段计划回填和 EvidenceDecision | reviewer / worker | evaluation / doc-rvv | decision audit | `test-rvv/keypoints/harris_2d/doc/phases/020-direct-intensity-stride-store/result.zh.md` |
| evaluation | decision doc | 候选取舍、问题记录和 fallback matrix | reviewer / worker | doc-rvv references | decision audit | `test-rvv/keypoints/harris_2d/doc/harris_2d-evaluation.zh.md` |

## 结论与后续方向

当前采纳范围内没有未阻塞的进一步优化动作。`direct-IntensityT-rvv` 和 NMS RVV 都需要扩大验证范围：前者需要泛型点类型 / accessor 证据，后者需要 profile 和输出顺序 oracle。默认后续状态是 ready for review；若要继续扩展，应从 `test-rvv/keypoints/harris_2d/doc/optimization-roadmap.zh.md` 的 `030-point-type-accessor-expansion` 或 `040-nms-profile-or-rvv` 新建 phase。
