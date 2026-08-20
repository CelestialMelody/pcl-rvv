# triangulation 函数级评估

## 范围和目标源码

目标源码是 `surface/src/on_nurbs/triangulation.cpp` 和公开头 `surface/include/pcl/surface/on_nurbs/triangulation.h`。本评估来自 `doc-rvv/library-screening/surface/surface-function-evaluation-queue.zh.md` 中 `on_nurbs/triangulation.cpp` 的建议进入函数级评估项。

## 函数族作用速览

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | 与主流程关系 | RVV 判断 |
| --- | --- | --- | --- | --- |
| `createVertices` | 按 knot 范围生成规则 `(u, v, z0)` 参数点。 | 输入起点、宽高、分段数；输出 `PointCloud<PointXYZ>`。 | `convertSurface2PolygonMesh` 和 `convertSurface2Vertices` 的前置网格。 | 首阶段 RVV 诊断候选。 |
| `createIndices` | 每个 grid cell 输出两个三角形索引。 | 输入起始 vertex index 和分段数；输出 `std::vector<pcl::Vertices>`。 | mesh polygon 构造。 | 标量 reserve / 输出构造消融；不作为首个 RVV 内核。 |
| `convertSurface2PolygonMesh` | 生成参数网格、三角形索引，并对每个参数点调用 `ON_NurbsSurface::Evaluate`。 | 输入 surface 和 resolution；输出 `PolygonMesh`。 | 未裁剪 surface 公开入口。 | full-path 诊断必须覆盖。 |
| `convertSurface2Vertices` | 与 polygon mesh 路径相同，但输出 cloud + vertices。 | 输入 surface 和 resolution；输出调用方容器。 | 上游 `test_on_nurbs.cpp` 在 fitting loop 中反复调用。 | full-path 诊断必须覆盖。 |
| `convertTrimmedSurface2PolygonMesh` | 先判断点是否在 trimming curve 内，再投影边界点并 Evaluate。 | 输入 surface、curve、mesh，可选 start/end。 | 裁剪 surface 入口。 | 暂缓到后续 phase，不能把未裁剪结论外推。 |
| `convertCurve2PointCloud` | 沿 curve element 采样，或先 curve Evaluate 再 surface Evaluate。 | 输入 curve / surface；输出 `PointXYZRGB`。 | 曲线可视化辅助入口。 | 暂缓。 |

## 初步结论

当前最有边界感的候选是 `param_grid_rvv_store`：在 test support 中用 RVV strided store（跨步写入）批量生成 `PointXYZ` 参数网格，再把后续求值保持为同一标量调用。当前 RISC-V 安装库没有导出 on_nurbs / OpenNURBS 符号，因此首个可执行诊断使用测试专用 Evaluate-like sink（曲面求值替身）观察参数网格写入是否有局部价值；它不能替代真实 `ON_NurbsSurface::Evaluate` direct 证据。

本阶段不直接改 production。当前结果已经回填：QEMU correctness 和 board correctness 通过，RVV 指令归属闭合；板卡 repeated 中 `tri_param_grid_512` median `0.978x`，`tri_surface_eval_256` median `0.999x`，两者均 3/5 退化且 Evidence Doctor `Errors=1`。该证据不支持 production 接入。

## Traceability Map（可追踪性地图）

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `Triangulation::convertSurface2PolygonMesh` | production public entry | 生成 mesh cloud 和 polygons。 | on_nurbs 示例和测试。 | `createVertices`、`createIndices`、`ON_NurbsSurface::Evaluate`。 | production boundary（生产边界），本阶段不修改。 | `surface/src/on_nurbs/triangulation.cpp` |
| `Triangulation::convertSurface2Vertices` | production public entry | 生成 mesh cloud 和 vertex list。 | `test/surface/test_on_nurbs.cpp` fitting loop。 | `createVertices`、`createIndices`、`ON_NurbsSurface::Evaluate`。 | production boundary，本阶段不修改。 | `surface/src/on_nurbs/triangulation.cpp` |
| `param_grid_rvv_store` | candidate helper | RVV 写入规则参数网格。 | `src/test_triangulation.cpp`、`src/bench_triangulation.cpp`。 | test-only full-path helper。 | diagnostic correctness / asm / bench。 | `test-rvv/surface/triangulation/include/triangulation.h` |
| phase 000 plan | documentation section | 记录本阶段范围、矩阵和停止条件。 | 短 prompt 恢复。 | phase result、Handoff。 | recovery pointer（恢复入口）。 | `test-rvv/surface/triangulation/doc/phases/000-current-state-and-grid-sampling-diagnostic/plan.zh.md` |

## 测试和 bench 计划

| 测试 / bench | 层级 | 作用 |
| --- | --- | --- |
| `run_test_compare` | correctness（正确性） | 对比标量 reference 和 RVV candidate 的参数点、polygon 顺序和 full surface checksum。 |
| `run_bench_compare` | board-only benchmark（板卡性能测试） | 在目标硬件上比较 Std / RVV build 的 test helper；QEMU 默认禁止作为性能结论。 |
| `dump_bench_rvv` | asm attribution（反汇编归属） | 确认 RVV store 指令能归属到参数网格 helper 或内联热点。 |
| Evidence Doctor | evidence validation（证据体检） | 性能结论前暴露 metadata、checksum、A/B 边界和波动风险。 |

## 生产接入判断

当前判断为 `diagnostic/no-production for current candidate`。`doc-rvv/surface/triangulation-RVV.zh.md` 当前不适用，因为还没有 adopted production behavior、用户确认保留的 production patch 或 PI5 生产证据闭环。

## 当前证据链

| evidence | command / file | result | boundary |
| --- | --- | --- | --- |
| correctness | `make run_test_compare` | Std / RVV 均 2 tests passed。 | QEMU correctness；不作性能结论。 |
| asm attribution | `make dump_bench_rvv` | 看到 `vsetvli`、`vid.v`、`vfcvt.f.xu.v`、`vfmacc.vf`、`vsse32.v`。 | `createParamGridCandidate` test helper。 |
| board smoke param-grid | `log/board/param_grid_512_smoke/evidence_doctor.md` | `Errors=1, Warnings=1, Suggestions=0`；Std/RVV `0.998x`。 | diagnostic smoke，单次 run。 |
| board smoke surface-eval | `log/board/surface_eval_256_smoke/evidence_doctor.md` | `Errors=0, Warnings=1, Suggestions=1`；Std/RVV `1.005x`。 | diagnostic smoke，单次 run。 |
| board correctness | `make run_board_test && make fetch_board_logs` | 2 tests passed。 | board correctness；不作性能结论。 |
| repeated board param-grid | `log/board/param_grid_512_repeated_20260820_141209/evidence_doctor.md` | `Errors=1, Warnings=0, Suggestions=0`；median `0.978x`，3/5 退化。 | diagnostic repeated。 |
| repeated board surface-eval | `log/board/surface_eval_256_repeated_20260820_141331/evidence_doctor.md` | `Errors=1, Warnings=0, Suggestions=0`；median `0.999x`，3/5 退化。 | diagnostic repeated。 |
