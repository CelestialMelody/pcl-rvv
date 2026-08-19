# organized_fast_mesh 函数级评估

## 范围和目标源码

目标源码是 `surface/include/pcl/surface/impl/organized_fast_mesh.hpp`。
它负责 organized point cloud 的网格重建：按二维行列顺序扫描邻点，生成 quad 或 triangle mesh。

## 函数 / 函数族作用速览

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | 与主流程关系 | RVV 判断 |
| --- | --- | --- | --- | --- |
| `performReconstruction` | 入口层组织输出 | input cloud -> `PolygonMesh` / polygons | 公开入口 | 值得评估 |
| `reconstructPolygons` | 根据 triangulation type 分流 | 入口状态 -> 具体 mesh builder | 分发层 | 需要保留标量 gate |
| `makeQuadMesh` | 生成 quad mesh | organized grid -> polygons | 规则扫描 | 值得评估 |
| `makeRightCutMesh` / `makeLeftCutMesh` | 固定 triangle 切分 | organized grid -> polygons | 规则扫描 | 值得评估 |
| `makeAdaptiveCutMesh` | 按 z 差值选切分方向 | organized grid -> polygons | 规则扫描 + 少量浮点 | 值得评估 |
| `isValidTriangle` / `isValidQuad` | finite 检查 | point indices -> bool | 热点候选 | RVV 优先 |
| `isShadowed*` | 阴影 / 视点相关检查 | point data -> bool | 复杂边界 | 暂缓 |

## 函数级结论

当前 topic 值得做的不是“把整个类都向量化”，而是把规则网格扫描里的
finite mask 和 adaptive cut 的对角线比较单独拿出来评估。shadow 检查涉及
视点、距离阈值和角度阈值，先保留标量。

## 标量流程

公开入口先检查 organized 属性，再进入 `reconstructPolygons`。随后根据 triangulation type
进入 quad / left-cut / right-cut / adaptive-cut builder。每个 builder 都是两层循环：
外层按 row step 走行，内层按 column step 走 cell。
每个 cell 先判断有效点是否 finite，再按切分方式追加 1 或 2 个三角形，或者 1 个 quad。
adaptive cut 额外比较两条对角线对应点的 z 差值。

## RVV 诊断设计

第一阶段只做 production-shaped diagnostic：

- RVV 负责批量计算 finite flags。
- RVV 负责批量计算 adaptive cut 的 z 差值偏好。
- 标量继续负责 polygon append、shape gating 和 shadow 检查。

## 生产接入判断

当前不建议保留 production 源码中的 RVV 接入。原因不是 correctness 失败，而是
production public path（生产公开入口）性能证据不支持采纳：

- `PointXYZ` / `float` / organized / `triangle_pixel_size=1` / `storeShadowedFaces(true)` 下，
  test-only candidate 曾在 diagnostic boundary（诊断边界）显示正向，但那不是最终 production evidence。
- production public path 首轮接入后，board summary 显示 quad `0.78x`、right-cut `0.93x`、
  left-cut `0.95x`、adaptive-cut `1.21x`，说明固定切分路径会被 RVV helper 额外成本拖慢。
- 收窄为 adaptive-cut-only 后，board summary 仍显示 adaptive-cut `0.89x`，quad 约 `1.00x`、
  right/left 约 `0.99x`；Evidence Doctor 为 Errors=3，主要是 public path 退化频率。
- 负向归因是受证据约束的实现形态判断：RVV 片段只批量计算 finite mask（有限值掩码）和
  adaptive diagonal preference（自适应对角线偏好），而 polygon append（多边形追加）、
  分支和顺序输出仍是标量主成本。上一版 production helper 还使用 `push_back` 写回，
  与原标量路径的预分配 `resize` + `idx` 原地写回不同，导致真实公开入口的输出容器成本被放大。
- Phase 020 已把 production helper 和 test-only candidate 改为同构的预分配写回，并通过
  QEMU correctness；QEMU bench smoke 仍显示 RVV 慢于标量，只能作为日志形状和负向风险提示。
  板卡复测当前因 SSH 超时未完成。

结论：当前 topic 不建议继续扩大 production 接入范围，生产源码中的 RVV 接入已按用户确认回滚。
本 topic 以 no-production 结论收口，仅保留 topic-local test / bench / evidence 资产作为复查依据。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `surface/include/pcl/surface/impl/organized_fast_mesh.hpp` | production | 公开入口与标量实现 | PCL surface API | `PolygonMesh` / `Vertices` | source of truth | repo source |
| `test-rvv/surface/organized_fast_mesh/include/organized_fast_mesh.h` | test support | 稳定聚合入口 | test / bench | internal helpers | test entry | topic-local |
| `test-rvv/surface/organized_fast_mesh/include/impl/ofm_reference.hpp` | test support | 标量参考 | test / bench | candidate compare | correctness baseline | topic-local |
| `test-rvv/surface/organized_fast_mesh/include/impl/ofm_candidates.hpp` | test support | test-only RVV candidate | test / bench | diagnostic compare | diagnostic candidate | topic-local |
| `test-rvv/surface/organized_fast_mesh/src/test_organized_fast_mesh.cpp` | test | correctness 对拍 | Make target | QEMU / board smoke | correctness | topic-local |
| `test-rvv/surface/organized_fast_mesh/src/bench_organized_fast_mesh.cpp` | bench | std/RVV bench compare | Make target | QEMU / board benchmark | diagnostic bench | topic-local |
| `test-rvv/surface/organized_fast_mesh/script/analyze_organized_fast_mesh_manifest.py` | script | 生成 evidence manifest | board logs | Evidence Doctor / registry | evidence metadata | topic-local |
