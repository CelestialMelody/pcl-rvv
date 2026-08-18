# Phase 015 Plan：production integration for all row sources

## 阶段意图和授权边界

用户已授权把四类 row-source policy（行来源策略）对应的 RVV 优化接入 production，
再在 production 端测试收益，并在 PI5 后由用户判断保留还是取消接入。

本阶段允许修改：

- `registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp`
- 当前 topic 的 test-rvv 测试、bench、Makefile、script、phase / evaluation / roadmap 文档和 summary evidence。

本阶段不修改 public API（公开接口），不创建 commit。PI5 后必须停在用户判断点。

## PI1 接入范围

| row source policy | production 入口 | RVV family | 点型 / Scalar / layout gate | fallback |
| --- | --- | --- | --- | --- |
| `ordered-cloud-pair` | `estimateRigidTransformation(cloud_src, cloud_tgt, Matrix4&)` | strided xyz load + C1/C2 f64 reduction | `PointSource` / `PointTarget` 分别满足 `RVVXYZAoSFloatLayout`，`Scalar=float`，dense，规模达到阈值 | 非 RVV 构建、`Scalar!=float`、layout 不满足、非 dense、小规模或数量不匹配保持标量 |
| `source-indexed-cloud-pair` | `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, Matrix4&)` | source indexed gather + target strided load | 同上，并要求 source cloud byte offset 可由 32-bit 表达，indices 非负且在 source 范围内 | 任一 gate 失败保持标量 iterator |
| `dual-indexed-cloud-pair` | `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, indices_tgt, Matrix4&)` | source / target 双侧 indexed gather | 同上，并要求两侧 indices 非负且在各自 cloud 范围内 | 任一 gate 失败保持标量 iterator |
| `correspondence-pair` | `estimateRigidTransformation(cloud_src, cloud_tgt, correspondences, Matrix4&)` | direct correspondence index stream + 双侧 indexed gather | 同上，并要求 `pcl::index_t` 为 32-bit、`pcl::Correspondence` standard-layout、query/match 非负且在范围内 | 任一 gate 失败保持标量 iterator |

不接入 Phase 009 的 `vlseg3e32` segment-load，也不接入 Phase 010 的 locality-aware dispatch。

## 实现动作

| action | 产物 | 验证 |
| --- | --- | --- |
| PI2 production patch | production header 新增 TEDQ detail RVV helper、四个 public overload 的短路 dispatch、内部标量 helper | 本地 diff 审查；非 RVV 构建可编译 |
| PI3 production direct tests | `src/test_tedq.cpp` 增加 production detail path-hit / fallback tests，确认四类入口在 RVV 构建中可命中生产 helper，非覆盖条件 fallback | `make run_test_compare` |
| PI4 production evidence rerun | production-public / row-source public board repeated；必要时新增 summary script 或复用现有 board summary | QEMU correctness、QEMU smoke、asm、board repeated、Evidence Doctor、registry |
| PI5 evidence decision | 本 result、matrix、roadmap、evaluation 同步 | 停在 `pending_user_confirmation_adopt_production` 或 `pending_user_confirmation_rollback` |

## Board 预算和决策桶

- board：`Milkv-Jupiter`
- repeated runs：5
- iterations：20
- warm-up iterations：5
- performance 只采用 board / target hardware；QEMU timing 不进入收益判断。
- 若 production direct 不是至少 `weak_positive`，或 Evidence Doctor 出现未修正 Error，PI5 默认建议取消接入。

## 停止条件

- correctness / fallback / path-hit 任一失败且不能修复。
- production asm 不能归因到 TEDQ production helper。
- board repeated 或 Evidence Doctor 支持取消接入。
- 需要扩大到未授权 public API、其它模块或公共 wrapper API。
- PI5 完成后必须停止，等待用户判断保留或取消。
