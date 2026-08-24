# point_cloud_image_extractors 函数级评估

## 当前 EvidenceDecision

当前已采纳三个窄范围 production patch（生产补丁）：`rgb_segment_store_v1`、`scaling_reduction_v1`
和 `production_label_mono16_stride_v0` 均已接入
`io/include/pcl/io/impl/point_cloud_image_extractors.hpp`。真实公开入口的 production-public
（公开入口生产证据）repeated board 为正向：RGB `PointXYZRGB` median 1.54x、RGB `PointXYZRGBA`
median 1.55x、intensity full-range scaling median 1.52x；label mono16 为 weak-positive，median
1.08x、min 1.05x、max 1.09x。Phase 080 与 Phase 090 的 Evidence Doctor 均为
`Errors=0, Warnings=0, Suggestions=0`。用户确认“有收益即可采纳”，因此这些窄范围补丁写成
adopted production behavior（已采用生产行为）。

历史诊断结论仍有效：full-range scaling v0 为负向，normal v0 为负向；`label_mono16_stride_v0`
已由 Phase 090 的 production-public 证据推进为 exact `PointXYZL` / `COLORS_MONO` 生产路径。

## 范围和目标源码

| 文件 | 作用 | 当前状态 |
| --- | --- | --- |
| `io/include/pcl/io/impl/point_cloud_image_extractors.hpp` | organized cloud 转 `PCLImage` 的 header-only 实现 | 当前工作树已有窄范围 `__RVV10__` 分流，RGB/scaling/label mono16 已采纳 |
| `io/include/pcl/io/point_cloud_image_extractors.h` | 公开 extractor 类声明 | 不改 public API |
| `test/io/test_point_cloud_image_extractors.cpp` | 上游单元测试 | 已覆盖 normal/RGB/RGBA/label/scaling/NaN 小样本 |
| `test-rvv/io/point_cloud_image_extractors` | 本 topic diagnostic、bench 和 phase 文档 | 首阶段 scaffold 已建立 |

## 函数 / 函数族作用速览

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | RVV 判断 |
| --- | --- | --- | --- |
| `PointCloudImageExtractor<PointT>::extract` | 校验 organized cloud，调用 `extractImpl` 后按需把 NaN 点涂黑 | 读 `cloud`，写 `PCLImage` | post-pass 可诊断；production 需保持不同 encoding 的步长语义 |
| `PointCloudImageExtractorFromRGBField::extractImpl` | 读取 `rgb` 或 `rgba` 字段，写 `rgb8` | AoS 字段 offset -> RGB 三字节 | 首阶段重点候选 |
| `PointCloudImageExtractorWithScaling::extractImpl` | 读取任意 float 字段，按 no/fixed/full-range 写 `mono16` | 可选 min/max 规约 + 写回 | 首阶段重点候选，但 float-to-uint16 语义需谨慎 |
| `PointCloudImageExtractorFromLabelField::extractImpl` | label 转 mono16、随机 RGB 或 Glasbey LUT | `std::map` / `std::set` 状态明显 | exact `PointXYZL` `COLORS_MONO` 已采纳；random/Glasbey 保持标量 |
| `PointCloudImageExtractorFromNormalField::extractImpl` | normal_x/y/z 映射到 RGB | 三个 float 字段跨步读取 | Phase 060 v0 诊断负向；后续只在用户单独授权 normal production probe 或出现新实现形态时恢复 |

## 标量流程与 RVV 诊断流程

| 阶段 | 标量路径 | 当前诊断候选 | 边界 |
| --- | --- | --- | --- |
| RGB field lookup | `getFieldIndex("rgb")`，缺失时查 `rgba` | 同样使用 PCL field metadata | 保持字段 offset 语义 |
| RGB unpack | 每点 `getFieldValue<uint32_t>` 后移位写三字节 | `vlse32` 跨步读取 `uint32`，向量移位拆包，逐 lane 写输出 | 仍是 test helper，不是 production dispatch |
| scaling full range | 标量 first pass 找 min/max，second pass 写 `uint16_t` | RVV 跨步加载到 chunk，chunk 内标量 min/max；写回阶段用向量 float 计算后写 `uint16_t` | 避免第一阶段直接依赖外部 FRM；反汇编仍需检查转换形态 |
| normal field | 标量逐点读取 `normal_x/y/z`，按 `(value + 1.0) * 127` 转 `uint8_t` | Phase 060 v0 用三路 `vlse32`、float 加乘和 scratch 后逐 lane 写 `rgb8` | correctness 通过但 board negative，不能写成 production evidence |
| label mono16 | 标量逐点读取 `label`，按 `static_cast<unsigned short>` 写 `mono16` | Phase 070 v0 用 `vlse32` 读取 `uint32_t` label、`vnsrl.wi` 保留低 16 位、`vse16` 写回 | 只覆盖 `COLORS_MONO`，不覆盖 RGB color modes |
| NaN post-pass | `pcl::isFinite` 失败时清零当前像素 | 当前候选保持标量 post-pass | 只覆盖 mono16 post-pass |

## 测试和 bench 计划

本节只保留决策审计入口；逐个 TEST、bench label、target 粒度和代码地图分别由 topic-local
role 文档承载，避免 evaluation 复制完整测试工程说明：

- `doc/testing-overview.zh.md`：测试入口分类、target 粒度审计和 QEMU / board 证据边界。
- `doc/correctness-tests.zh.md`：gtest 输入、被测路径、断言和不能证明的范围。
- `doc/benchmark-and-evidence.zh.md`：bench label、case-filter、板卡 repeated、manifest 和 Evidence Doctor。
- `doc/optimization-evidence.zh.md`：candidate family 到 correctness、bench、asm、board 和 decision 的索引。
- `doc/test-support-code-map.zh.md`：测试支撑代码、bench wrapper、script 和 output 的定位地图。

| 测试 / target | 层级 | 作用 |
| --- | --- | --- |
| `make run_test_compare` | correctness | Std/RVV 各跑 15 个 gtest，覆盖 diagnostic、PI2 gate policy、真实 production path-hit、fallback、label RGB mode non-hit 和 NaN post-pass。 |
| `make run_bench_rvv ...` | QEMU smoke | 只证明 RVV binary 可运行、输出日志可解析、checksum 口径存在。 |
| `make dump_bench_rvv` | asm | 检查 `vlse32.v`、`vfmul`、`vfsub`、`vfncvt`、`vse16` 等指令是否出现。 |
| `make board_smoke` / `make collect_board_repeated ...` | board evidence | 板卡 correctness + Std/RVV bench compare，是本阶段唯一性能结论来源。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 证据角色 |
| --- | --- | --- | --- |
| `extractRgbScalar` | test support reference | 调真实 `PointCloudImageExtractorFromRGBField` 生成标量参考 | correctness baseline |
| `extractRgbRvv` | test support candidate | RGB/RGBA 字段跨步读取和拆包 | diagnostic candidate |
| `extractRgbSegmentStoreRvv` | test support candidate | RGB/RGBA 字段跨步读取、收窄和 `vsseg3e8` 三通道写回 | diagnostic candidate |
| `extractScalingScalar` | test support reference | 复刻 `WithScaling` 的 intensity 路径 | correctness baseline |
| `extractScalingRvv` | test support candidate | 跨步加载 intensity、full-range min/max 和写 mono16 | diagnostic candidate |
| `extractScalingFullRangeReductionRvv` | test support candidate | 用 `vfredmin` / `vfredmax` 完成 full-range min/max | diagnostic candidate |
| `extractNormalScalar` | test support reference | 调真实 normal extractor 生成标量参考 | correctness baseline |
| `extractNormalRvv` | test support candidate | normal_x/y/z 跨步加载和 `(value + 1) * 127` 写 `rgb8` | negative diagnostic candidate |
| `extractLabelMono16Scalar` | test support reference | 调真实 label extractor 的 `COLORS_MONO` 分支 | correctness baseline |
| `extractLabelMono16Rvv` | test support candidate | label 跨步加载、低 16 位截断和 `mono16` 写回 | positive diagnostic candidate |
| `src/test_pcie.cpp` | test | Std/RVV 对拍 | correctness |
| `src/bench_pcie.cpp` | bench | 输出 case timing 和 checksum | board performance wrapper |
| `doc/phases/000-current-state-and-diagnostic-scaffold/*` | phase docs | 本阶段计划与结果 | recovery |
| `doc/phases/030-doc-suite-parity/result.zh.md` | phase docs | topic-local doc suite 职责审计 | recovery |

## 当前状态

- RED：`make run_test_rvv` 首次失败于缺少 `pcie.h`，证明测试入口能捕获候选层缺失。
- GREEN：`make run_test_compare` 通过，Std/RVV 各 15 个测试 pass。Phase 080/090 新增真实 production
  direct TEST，证明窄范围 RVV dispatch、fallback、label RGB mode non-hit 和 NaN post-pass。
- QEMU bench smoke：Std/RVV 日志和 checksum 可解析；QEMU timing 不作为性能证据。
- asm：`make dump_bench_rvv` 生成 `build/asm/riscv/bench_pcie_rvv.asm`，可见 RVV 指令。
- board repeated：`log/board/repeated_phase000/summary.md` 显示 RGB `PointXYZRGB` median 1.29x、
  RGB `PointXYZRGBA` median 1.31x、fixed-factor scaling median 1.06x、full-range scaling median 0.91x。
- Evidence Doctor：`log/board/repeated_phase000/evidence_doctor.md` 为 `Errors=1, Warnings=0, Suggestions=0`，
  Error 指向 full-range scaling 5/5 退化。
- Phase 010 board repeated：`log/board/repeated_phase010/summary.md` 显示 `scaling_reduction_v1`
  median 1.54x、min 1.47x、max 1.56x。
- Phase 010 Evidence Doctor：`log/board/repeated_phase010/evidence_doctor.md` 仍为
  `Errors=1, Warnings=0, Suggestions=0`，唯一 Error 仍指向旧 v0 full-range label；v1 label 无退化 finding。
- Phase 020 board repeated：`log/board/repeated_phase020/summary.md` 显示 `rgb_segment_store_v1`
  在 `PointXYZRGB` 上 median 1.74x、在 `PointXYZRGBA` 上 median 1.75x。
- Phase 020 Evidence Doctor：`log/board/repeated_phase020/evidence_doctor.md` 仍为
  `Errors=1, Warnings=0, Suggestions=0`，唯一 Error 仍指向旧 full-range scaling v0；RGB segment labels 无退化 finding。
- Phase 060 board repeated：`log/board/repeated_phase060/summary.md` 显示
  `normal_field_pointnormal_640x480` median 0.61x、min 0.46x、max 0.67x，5/5 低于 1.0。
- Phase 060 Evidence Doctor：`log/board/repeated_phase060/evidence_doctor.md` 为
  `Errors=1, Warnings=1, Suggestions=0`。Error 指向 normal v0 退化频率，Warning 指向长尾；
  checksum 一致，因此是性能负向诊断，不是 correctness failure（正确性失败）。
- Phase 070 board repeated：`log/board/repeated_phase070/summary.md` 显示
  `label_mono16_pointxyzl_640x480` median 1.21x、min 1.18x、max 1.27x。
- Phase 070 Evidence Doctor：`log/board/repeated_phase070/evidence_doctor.md` 为
  `Errors=0, Warnings=0, Suggestions=0`，支持 label mono16 在 diagnostic boundary 下正向。
- Phase 080 production-public repeated board：`log/board/repeated_pi4/summary.md` 显示
  `production_rgb_pointxyzrgb_640x480` median 1.54x、`production_rgb_pointxyzrgba_640x480`
  median 1.55x、`production_scaling_full_range_intensity_640x480` median 1.52x。
- Phase 080 Evidence Doctor：`log/board/repeated_pi4/evidence_doctor.md` 为
  `Errors=0, Warnings=0, Suggestions=0`，证据角色为 production-public。
- Phase 090 production-public repeated board：`log/board/repeated_phase090/summary.md` 显示
  `production_label_mono16_pointxyzl_640x480` median 1.08x、min 1.05x、max 1.09x。
- Phase 090 Evidence Doctor：`log/board/repeated_phase090/evidence_doctor.md` 为
  `Errors=0, Warnings=0, Suggestions=0`，证据角色为 production-public。

## 生产接入判断

当前 production 接入判断为 adopted：RGB 只覆盖 exact `PointXYZRGB` / `PointXYZRGBA`，scaling 只覆盖
exact `PointXYZI` 的 `"intensity"` full-range，label 只覆盖 exact `PointXYZL` 的 `COLORS_MONO`。
`rgb_u32_stride_unpack_v0` 作为 RGB fallback implementation family 保留；`scaling_float_stride_v0`
full-range 已在诊断边界下拒绝，fixed-factor 只保留为弱正向线索。Phase 060 的 normal v0 负向结果
只说明当前 diagnostic helper 形态不支持纳入当前 production scope；它不能直接拒绝未来有边界的
normal production probe。label random / Glasbey、normal field、`pcd2png` 端到端和泛型字段组合均为
未验证范围，需要新的 phase 重新冻结范围和证据。
