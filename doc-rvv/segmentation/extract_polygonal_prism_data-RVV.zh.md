# ExtractPolygonalPrismData::segment RVV 优化说明

## 当前状态

`segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp` 中的 `ExtractPolygonalPrismData<PointT>::segment(PointIndices&)` 已接入有界 RVV（RISC-V Vector，可变长度向量扩展）生产路径。`__RVV10__` 构建下，公开入口先尝试 `segmentRvv`；不满足 gate（会回到标量的准入条件）时调用 `segmentStd`。非 RVV 构建只保留 `segmentStd` 标量路径。

当前采用范围是 polygon scan（多边形扫描）段：plane setup（平面拟合准备）和 `SampleConsensusModelPlane::projectPoints` 保持标量，但计入公开入口 benchmark（性能测试）。RVV 接管投影后的逐点筛选：点到平面有符号距离、高度范围 mask（掩码）、二维 polygon parity（奇偶多边形判定）、多 polygon XOR（多个多边形内外结果异或，用于凹包洞语义）和保序 `output.indices` 压缩。当前点型范围为 `RVVXYZAoSFloatLayout<PointT>` 且 `sizeof(PointT) <= 32`；`PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 已有 production public 板卡证据，`PointXYZINormal` 当前显式回退标量。当前规模阈值为 `indices_->size() >= 32`，低于 32 的输入走标量 fallback。

## 函数语义

`segment` 从 `input_` 与 `indices_` 指定的点中筛出位于 polygonal prism（多边形棱柱）内的点索引。标量路径先根据 `planar_hull_` 估计平面系数，再把 `indices_` 对应的输入点投影到平面；随后每个点先检查到平面的 signed distance（有符号距离）是否落在 `height_limit_min_` 与 `height_limit_max_` 之间，再用投影坐标对 hull polygon 做二维包含性判断。命中点按原扫描顺序写入 `output.indices`。

当 `polygons_` 为空时，标量路径使用整个 hull 投影 polygon；当 `polygons_` 非空时，标量路径会按 `Vertices` 分组构造多个 polygon，并对每个点把各 polygon 的 inside 结果做 XOR（异或）合并，从而支持 concave hull（凹包）洞语义。当前 RVV 生产路径覆盖空 `polygons_`、单个 polygon 和合法多 polygon XOR；active polygon 顶点不足或 polygon 顶点索引越界时回退标量。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| public dispatch | adopted | 公开入口只做 RVV 短路分流和 `segmentStd` fallback，API 不变 | `make run_test_compare`、`make run_board_test` | reviewer 可重点复核 `initCompute` / `deinitCompute` 状态衔接 |
| xyz AoS load | adopted for <=32-byte point types | 只读 `x/y/z`，输出是 index，不构造完整 `PointT`；`sizeof(PointT) <= 32` 的常见点型板卡正向 | `RVVXYZAoSFloatLayout<PointT>` gate；post-gate `PointXYZI` 1.85x、`PointXYZRGB` 1.83x、`PointXYZRGBA` 1.84x | `PointXYZINormal` 和更宽 stride 点型回退标量 |
| dense ordered row source | adopted | 连续扫描时使用 strided load（跨步加载）减少 per-point 标量判断 | production dense board median 1.75x | 只覆盖合法 ordered `indices_` |
| indexed row source | adopted | 非 ordered 但合法 indices 使用 32-bit byte offset gather（离散加载）读取原始点 | production indexed board median 1.75x | invalid index 或 byte offset 超界 fallback |
| polygon parity / XOR | adopted | 对每个 polygon 的每条边按 VL chunk 同时更新 inside mask，多个 polygon 之间再用 XOR 合并，最后 `vcompress` 保序输出 | asm 中有 `vmxor.mm`、`vcompress.vm`；nested dense median 2.18x、nested indexed median 2.13x | degenerate polygon 和非法 polygon 顶点索引回退标量 |
| plane setup / `projectPoints` | scalar tail | 这部分每次调用只做一次或由既有 PCL helper 承担，当前 RVV 证据证明扫描段收益已覆盖其前置成本 | production public bench 已计入完整入口 | 若要优化 `projectPoints` 应另做 component ablation（组件消融） |
| small input | adopted threshold 32 | 32 点 production public confirm5 仍为 positive；低于 32 时 RVV 启动和输出压缩开销更容易吞掉收益 | threshold32 confirm5 median 1.19x、min 1.18x、doctor 0 / 0 / 0；`SegmentRvvDeclinesSmallInputs` 覆盖 16 点 fallback | 低于 32 不采纳 |
| concave hull multi polygon | adopted | Phase 050 production direct correctness 和 nested board repeated 证据均正向 | `SegmentRvvMatchesSegmentStdForNestedPolygons`、`SegmentRvvMatchesSegmentStdForIndexedNestedPolygons`；nested Evidence Doctor 0 / 0 / 0 | 只覆盖合法 polygon；退化 polygon 回退 |
| wide-stride point type | scalar fallback | `PointXYZINormal` 接入前 20-run 中 5/20 低于 1 且长尾明显；当前生产 gate 保守回退 | `SegmentRvvDeclinesPointXYZINormal`；post-gate fallback confirmation median 1.00x | 若要重试，需要 dedicated wide-stride phase |

## RVV 路径

`segmentRvv` 先复用原标量的平面系数计算、viewpoint 翻转和 `projectPoints`。投影完成后，它根据 `model_coefficients` 选择投影平面的两个坐标轴 `k1/k2`，把 hull 投影点转换成一个或多个 active polygon（实际参与判定的二维多边形），然后按 VL chunk（一次 RVV 向量长度分块）处理 `indices_`。

在每个 chunk 内，dense ordered indices 使用 `pcl::rvv_load::strided_load3_f32m2` 从 `input_->points` 读取 `x/y/z`；indexed row source 先加载 `indices_`，用 `byte_offsets_u32m2` 形成 32-bit byte offsets，再用 `indexed_load3_f32m2` gather 原始点坐标。`projected_points` 已经按扫描顺序展开，所以投影坐标始终按 chunk 顺序跨步加载。

距离筛选使用 `a*x + b*y + c*z + d` 生成 height mask。polygon predicate 对每条边构造 crossing mask，并用 `vmxor` 累计单个 polygon 的 inside 状态；当 `polygons_` 包含多组顶点时，再把每个 polygon 的 inside mask 做 XOR 合并，保持凹包洞语义。最后把 height mask 与合并后的 inside mask 相与，使用 `vcompress` 把保留的 source index 压缩到临时缓冲，再按 lane 顺序追加到 `output.indices`。dense 路径写回扫描下标，indexed 路径写回原始 source index，保持标量输出顺序。

## Fallback 矩阵

| gate | production 行为 | 已有证据 |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | `segment` 直接调用 `segmentStd` | Std build 通过 `make run_test_compare` |
| `input_` 或 `planar_hull_` 为空 | `segmentRvv` 返回 false，公开入口回到 `segmentStd` | 源码 gate 保持 public fallback |
| `initCompute()` 失败 | `segmentRvv` 清空输出并返回 true，与原入口一致 | production direct helper 复刻原语义 |
| hull 点数少于 `min_pts_hull_` | fallback 到 `segmentStd`，保留原错误处理 | 源码 gate |
| `RVVXYZAoSFloatLayout<PointT>` 不满足 | 编译期返回 false，fallback 到 `segmentStd` | `PointXYZI` 正向；非覆盖布局不误入 RVV |
| `indices_->size() < 32` | fallback 到 `segmentStd` | `SegmentRvvDeclinesSmallInputs`；Phase 070 32 点 confirm5 支撑阈值降到 32 |
| invalid index 或 32-bit byte offset 超界 | fallback 到 `segmentStd` | `segmentRvv` 检查 index 范围和 cloud size |
| active polygon 顶点不足、polygon 顶点索引越界或 projection 尺寸不一致 | fallback 到 `segmentStd` | `SegmentRvvDeclinesDegeneratePolygons` 和源码 gate |
| `sizeof(PointT) > 32` | fallback 到 `segmentStd` | Phase 060 `PointXYZINormal` 20-run 接入前证据不稳定；post-gate fallback confirmation median 1.00x |

## 范围决策表

| 范围 | 状态 | 证据 | 下一步 |
| --- | --- | --- | --- |
| `PointXYZ` / dense ordered / single polygon / `Scalar=float` distance | adopted | `log/board/repeated-production/summary.md`：5 runs，median 1.75x，min 1.74x，max 1.75x | 无需继续证明当前边界 |
| `PointXYZ` / indexed gather / single polygon / `Scalar=float` distance | adopted | `log/board/repeated-production-indexed/summary.md`：5 runs，median 1.75x，min 1.72x，max 1.75x | 无需继续证明当前边界 |
| `PointXYZ` / dense ordered / nested polygons / `Scalar=float` distance | adopted | `log/board/repeated-production-nested/summary.md`：5 runs，median 2.18x，min 2.11x，max 2.23x | 无需继续证明当前边界 |
| `PointXYZ` / indexed gather / nested polygons / `Scalar=float` distance | adopted | `log/board/repeated-production-nested-indexed/summary.md`：5 runs，median 2.13x，min 2.11x，max 2.17x | 无需继续证明当前边界 |
| `PointXYZI` / dense ordered / single polygon | adopted | `log/board/repeated-production-xyzi-postgate/summary.md`：5 runs，median 1.85x，min 1.77x，max 1.87x | 无需继续证明当前边界 |
| `PointXYZRGB` / dense ordered / single polygon | adopted | `log/board/repeated-production-xyzrgb-postgate/summary.md`：5 runs，median 1.83x，min 1.75x，max 1.88x | 无需继续证明当前边界 |
| `PointXYZRGBA` / dense ordered / single polygon | adopted | `log/board/repeated-production-xyzrgba-postgate/summary.md`：5 runs，median 1.84x，min 1.79x，max 1.86x | 无需继续证明当前边界 |
| `PointXYZINormal` / dense ordered / single polygon | scalar fallback | `log/board/repeated-production-xyzinormal-postgate/summary.md`：5 runs，median 1.00x；doctor 0 / 1 / 1；historical confirm20 doctor 1 / 1 / 0 | 不写作 RVV 收益；只在 dedicated wide-stride phase 重开 |
| `PointXYZ` / dense ordered / single polygon / size 32 | adopted threshold evidence | `log/board/repeated-production-size32-threshold32-confirm5/summary.md`：5 runs，median 1.19x，min 1.18x，max 1.38x；doctor 0 / 0 / 0 | 证明当前规模阈值可降到 32；不外推到低于 32 |
| 多 polygon concave hull XOR | adopted for legal polygons | production direct nested dense / indexed correctness 和板卡证据已覆盖 | degenerate polygon 和非法 polygon 顶点索引仍回退标量 |
| `Scalar=double` 或非 float xyz layout | scalar-only now | RVV helper 只读取 f32 xyz | 需要新的数值与 layout 证据 |

## 数值算例

设平面系数为 `(0, 0, 1, 0)`，高度范围为 `[-0.05, 0.10]`，polygon 为 `[-1.6, 1.6] × [-1.6, 1.6]`。一个 chunk 中的四个点：

| source index | 原始点 `(x,y,z)` | distance | height mask | 投影点 `(x,y)` | in polygon | keep |
| ---: | --- | ---: | --- | --- | --- | --- |
| 10 | `(0.50, 0.50, 0.02)` | `0.02` | true | `(0.50, 0.50)` | true | true |
| 11 | `(0.75, 0.75, 0.16)` | `0.16` | false | `(0.75, 0.75)` | true | false |
| 12 | `(2.00, 0.00, 0.02)` | `0.02` | true | `(2.00, 0.00)` | false | false |
| 13 | `(-0.25, -0.25, 0.02)` | `0.02` | true | `(-0.25, -0.25)` | true | true |

RVV 路径会把 keep mask 为 true 的 lane 通过 `vcompress` 压缩，输出 `[10, 13]`。indexed row source 中，source index 来自 `indices_`；dense ordered 中，source index 等于扫描位置。

## Bench 与证据

| 证据 | 命令 / 路径 | 结果 | 说明 |
| --- | --- | --- | --- |
| correctness | `make run_test_compare` | Std 2 tests、RVV 16 tests pass | 覆盖 diagnostic、production direct、point-type expansion 和 fallback |
| board correctness | `make run_board_test` | 16 tests pass | 目标板卡上功能通过 |
| QEMU smoke | `make run_bench_* ... --path production` | dense / indexed Std/RVV checksum 一致 | QEMU 只证明 build、路径和日志形状，不作为性能结论 |
| asm attribution | `make dump_bench_rvv`；`build/asm/riscv/bench_eppd_rvv.full.asm` | `segmentRvv` 符号内有 `vlse32.v`、`vluxseg3ei32.v`、`vfmacc.vf`、`vmxor.mm`、`vcompress.vm` | 证明生产 helper 含预期 RVV 指令 |
| production dense board | `make run_board_eppd_repeated_production`；`log/board/repeated-production/summary.md` | 5 runs，median 1.75x，min 1.74x，max 1.75x | 真实公开入口 dense path 性能证据 |
| production indexed board | `make run_board_eppd_repeated_production_indexed`；`log/board/repeated-production-indexed/summary.md` | 5 runs，median 1.75x，min 1.72x，max 1.75x | 真实公开入口 indexed path 性能证据 |
| production nested dense board | `make run_board_eppd_repeated_production_nested`；`log/board/repeated-production-nested/summary.md` | 5 runs，median 2.18x，min 2.11x，max 2.23x | 真实公开入口 nested dense 性能证据 |
| production nested indexed board | `make run_board_eppd_repeated_production_nested_indexed`；`log/board/repeated-production-nested-indexed/summary.md` | 5 runs，median 2.13x，min 2.11x，max 2.17x | 真实公开入口 nested indexed 性能证据 |
| production point-type board | `make run_board_eppd_repeated_production_pointtypes`；`log/board/repeated-production-*-postgate/summary.md` | `PointXYZI` 1.85x、`PointXYZRGB` 1.83x、`PointXYZRGBA` 1.84x；`PointXYZINormal` fallback 1.00x | 当前点型范围和宽 stride fallback 证据 |
| production threshold board | `log/board/repeated-production-size32-threshold32-confirm5/summary.md` | 5 runs，median 1.19x，min 1.18x，max 1.38x；Evidence Doctor 0 / 0 / 0 | 当前 `indices_->size() >= 32` 阈值的生产证据 |
| Evidence Doctor | `log/board/repeated-production*/evidence_doctor.md` | adopted production performance 组均为 Errors=0 / Warnings=0 / Suggestions=0；`PointXYZINormal` fallback confirmation 为 0 / 1 / 1 | 未解决 warning 不用于 RVV 采纳 |

## 正确性与高效性证据链

Correctness（正确性）由真实 public entry 对拍和 fallback tests 支撑：RVV 构建下的 `segmentRvv` 输出与 `segmentStd` 一致，覆盖 single polygon、nested polygons、dense / indexed 输入、小规模 fallback、degenerate polygon fallback、非 RVV 构建标量语义、`PointXYZI` / `PointXYZRGB` / `PointXYZRGBA` 点型扩展，以及 `PointXYZINormal` 显式 fallback。

Path / asm evidence（路径 / 反汇编证据）显示 production `segmentRvv` 符号内有跨步加载、gather、FMA（融合乘加）、mask XOR 和压缩写回指令。Performance（性能）结论只引用 Milkv-Jupiter 板卡 repeated summary；QEMU 结果只作为 checksum 和日志形状证据。Boundary（证据边界）为 `RVVXYZAoSFloatLayout<PointT>`、`sizeof(PointT) <= 32`、合法 dense 或 indexed indices、`indices_->size() >= 32`、合法 single / nested polygon。Risk（风险）集中在宽 stride 点型、低于 32 的小规模输入和 `projectPoints` 前置成本。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `ExtractPolygonalPrismData<PointT>::segment` | production public entry | 真实公开入口，执行 RVV dispatch 和 fallback | 用户代码 | `segmentRvv`、`segmentStd` | production boundary | `segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp` |
| `segmentStd` | production Std helper | 原标量主体 | `segment` fallback、测试派生类 | `projectPoints`、`isXYPointIn2DXYPolygon` | scalar baseline / fallback | `segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp` |
| `segmentRvv` | production RVV helper | polygon 扫描段 RVV 化，覆盖 single / nested polygon | `segment` | `pcl::rvv_load`、RVV intrinsic、`vcompress` | adopted production path | `segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp` |
| `src/test_eppd.cpp` | correctness gate | production direct、diagnostic 和 fallback 测试 | `make run_test_compare`、`make run_board_test` | gtest output | correctness / fallback | `test-rvv/segmentation/extract_polygonal_prism_data/src/test_eppd.cpp` |
| `src/bench_eppd.cpp` | bench wrapper | `--path production` 调真实 `segment` | board / QEMU bench targets | repeated summary script | production public performance | `test-rvv/segmentation/extract_polygonal_prism_data/src/bench_eppd.cpp` |
| `generate_eppd_board_evidence_manifest.py` | analysis script | 生成 summary 和 Evidence Doctor manifest | board repeated targets | `test-rvv/script/evidence_doctor.py` | evidence summary | `test-rvv/segmentation/extract_polygonal_prism_data/script/generate_eppd_board_evidence_manifest.py` |
| production board summaries | evidence output summary | 保存 5-run production speedup，包含 single / nested、dense / indexed 和 point-type post-gate | board repeated targets | evaluation、本主题文档、Handoff | board performance | `test-rvv/segmentation/extract_polygonal_prism_data/log/board/repeated-production*/summary.md` |
| function evaluation | evaluation | 保存候选取舍、测试矩阵和未覆盖范围 | phase results | 本主题文档、筛选队列 | decision audit | `test-rvv/segmentation/extract_polygonal_prism_data/doc/extract_polygonal_prism_data-evaluation.zh.md` |
| topic-local doc suite | documentation role docs | 保存 testing、correctness、benchmark/evidence、optimization evidence 和 code map 细节 | README / evaluation | reviewer | reviewability support | `test-rvv/segmentation/extract_polygonal_prism_data/doc/{testing-overview,correctness-tests,benchmark-and-evidence,optimization-evidence,test-support-code-map}.zh.md` |
| optimization roadmap / matrix | phase docs | 保存后续候选和证据状态 | phase loop | Handoff / reviewer | recovery pointer | `test-rvv/segmentation/extract_polygonal_prism_data/doc/optimization-roadmap.zh.md`、`doc/phases/optimization-matrix.zh.md` |

## 生产接入后的 Closeout

| 文件 / 入口 | 状态 | 说明 |
| --- | --- | --- |
| `segmentation/include/pcl/segmentation/extract_polygonal_prism_data.h` | adopted | 新增 protected `segmentStd` 和 RVV 构建下的 `segmentRvv` 声明；public API 不变 |
| `segmentation/include/pcl/segmentation/impl/extract_polygonal_prism_data.hpp` | adopted | public `segment` 短路调用 `segmentRvv`，否则进入 `segmentStd` |
| `test-rvv/segmentation/extract_polygonal_prism_data` | adopted evidence assets | 保存 production direct tests、bench wrapper、phase docs、summary evidence 和恢复入口 |
| `doc-rvv/library-screening/segmentation/segmentation-function-evaluation-queue.zh.md` | updated | 队列状态更新为已采纳 |

当前无需回滚。Phase 050 已把合法 concave hull 多 polygon XOR 纳入 adopted production behavior，Phase 060 已完成常见 <=32-byte 点型扩展，Phase 070 已把规模阈值采纳为 32。若后续 reviewer 发现 `initCompute` / `deinitCompute` fallback 状态与 PCLBase 生命周期不一致，应先修复生产 helper 并重跑 `make run_test_compare`、`make run_board_test`、production board repeated summary 和 Evidence Doctor，再更新本文档。

## 后续方向

当前 adopted 范围内没有必须继续才能支撑生产接入的缺口。Phase 050 已证明 concave hull 多 polygon XOR 值得接入，Phase 060 已证明三种 <=32-byte 常见点型值得保留，并把 `PointXYZINormal` 降级为标量 fallback，Phase 070 已证明 32 点阈值值得采纳。剩余方向不建议在当前自动 loop 中继续推进：wide-stride 点型需要新的实现族和专项证据，`projectPoints` 属于更宽的 `SampleConsensusModelPlane` component ablation（组件消融），自定义点型或 `Scalar=double` 会扩大 layout / 数值边界。
