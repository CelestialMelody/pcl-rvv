# Phase 012 Plan：point type / layout expansion

## 阶段意图和边界

本阶段继续 Phase 011 的整理后重启路线，优先扩展
`row_source_policy=correspondence-pair` 下的 Phase 008 direct index stream baseline。
本阶段只覆盖 test-rvv 诊断资产，不修改
`registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp`，
不启动 production integration loop。

`validated_scope`：

- row_source_policy：`correspondence-pair`
- index_pattern：`strided`
- candidate_family：direct index stream
- point type / layout：`PointXYZI`、`PointXYZRGB` / `float` / x,y,z AoS
- correctness：candidate 与 same-chain scalar reference 对拍
- QEMU：smoke only（只证明构建、日志形状和 checksum，不证明性能）

`unvalidated_scope`：

- source-indexed / dual-indexed direct gather 的 `PointXYZI`、`PointXYZRGB`
- mixed source/target 点型组合
- `Scalar=double`
- production public dispatch、fallback 和 production asm
- 真实 workload correspondence index distribution

## 当前状态

Phase 008 证明 `PointXYZ` / `float` / standard `Correspondence` AoS 的 direct index
stream 在板端相对 staged/direct gather 为 positive。Phase 009 拒绝 segment-load，
Phase 010 拒绝 locality-aware production candidate。Phase 011 已固定四类
row-source policy，并把 `contiguous/local-window/strided` 限定为 correspondence-pair
内部 `index_pattern`。

## 优化矩阵

| candidate family | row_source_policy | index_pattern | point type / Scalar / layout | correctness | QEMU smoke | board | asm | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| direct index stream | correspondence-pair | strided | `PointXYZI` / `float` / xyz AoS + intensity | planned | planned | bounded repeated planned | bench asm smoke | planned | planned |
| direct index stream | correspondence-pair | strided | `PointXYZRGB` / `float` / xyz AoS + rgb | planned | planned | bounded repeated planned | bench asm smoke | planned | planned |

## 实现和测试动作

| action | 产物 | 验证 | 完成判据 |
| --- | --- | --- | --- |
| A1 fixtures | `include/impl/tedq_adapters.hpp` | compile | `PointXYZRGB` deterministic corpus 保留颜色字段但只变换 x/y/z。 |
| A2 correctness | `src/test_tedq.cpp` | `make run_test_compare` | Std/RVV 都通过；RVV 构建命中 direct index stream 和 gather stats。 |
| A3 bench smoke | `src/bench_tedq.cpp`、QEMU manifest script | `make run_qemu_smoke_evidence_doctor BENCH_ARGS="--iterations 2 --warmup-iterations 1 --case-filter correspondence-point-type-layout"` | QEMU manifest 能区分 `PointXYZI` / `PointXYZRGB` 和 `correspondence-pair`。 |
| A4 board hook | Make / board target 或明确暂缓理由 | bounded 5-run repeated | 若板卡配置可用，同轮采集 summary / manifest / doctor；否则写入恢复条件。 |
| A5 docs | README、testing overview、benchmark/evidence、correctness tests、roadmap、matrix | docs scan + `make evidence_status` | Phase 012 结果能从当前 topic 恢复。 |

## Evidence Doctor 和 registry

QEMU smoke 只作为 log-shape evidence（日志形状证据）。若 bench banner 或 case label 变化，
必须刷新 QEMU manifest / doctor 和 evidence registry。板卡 repeated 使用 5-run budget；
bucket 口径沿用当前 topic：positive、weak_positive、neutral、negative、unstable。

## 继续 / 停止条件

如果 `PointXYZI` / `PointXYZRGB` correctness 和 QEMU smoke 通过，但板卡不可用，则本阶段
停在 `diagnostic_partial / board_deferred`，下一默认动作是
`run_board_bench_correspondence_point_type_layout_repeated`。如果板卡也通过且 Evidence Doctor
无阻断错误，则 matrix 标记为 `diagnostic_point_type_layout_positive` 或对应 bucket；仍不进入
production，除非用户明确授权 PI1。
