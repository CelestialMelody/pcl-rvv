# ISS 3D Testing Overview

## 测试层级

| 层级 | 入口 | 证明内容 | 不证明内容 |
| --- | --- | --- | --- |
| QEMU correctness（QEMU 正确性验证） | `run_test_compare` | Std/RVV binary 的 gtest 都通过，RVV 路径可运行且数值接近。 | 不证明目标硬件性能。 |
| asm attribution（反汇编归属） | `check_iss_3d_rvv_asm` | bench binary 中存在 ISS 3D test-only RVV indexed load 和 vector reduction。 | 不证明收益大小；production probe 已移除，production asm gate 不再是当前 target。 |
| historical production probe | `board_repeated_production` guarded by `ISS3D_ALLOW_HISTORICAL_PRODUCTION_PROBE=1` | 仅在重新引入 production probe patch 后才允许复跑历史形态。 | 当前源码没有 production RVV path，不能直接运行后写成 production evidence。 |
| diagnostic board bench（诊断板卡性能测试） | `board_repeated` | test-only scatter helper 的局部收益。 | 不证明 public `compute()` 端到端收益。 |
| production public board bench | `board_repeated_production` | 真实 `ISSKeypoint3D::compute()` 边界的 Std/RVV 对比。 | 当前 synthetic case checksum 为 0，不能证明非空 keypoint 输出语义。 |

## Target 粒度审计

| target 类别 | 当前 target | decision | 说明 |
| --- | --- | --- | --- |
| correctness aggregate | `run_test_compare` | adopted | 同时跑 Std 和 RVV gtest。 |
| correctness aliases | `run_test_std`、`run_test_rvv` | adopted | 由共享 harness 提供，便于单侧复现。 |
| bench diagnostic aliases | `run_board_bench_compare` with default `BENCH_ARGS` | adopted | 覆盖 `scatter_indexed_256`、`scatter_indexed_tail_73`、`scatter_contiguous_256`。 |
| QEMU smoke aliases | `run_qemu_smoke` | adopted | 仅别名到 correctness，不跑 QEMU bench compare。 |
| board smoke aliases | `run_board_bench_compare fetch_board_logs` | adopted | 单次板卡 smoke 只证明可运行和方向，不作最终性能结论。 |
| board repeated aliases | `board_repeated`、`board_repeated_production` | adopted | 生成 repeated summary、manifest、doctor 和 registry 记录。 |
| doctor / registry aliases | `record_evidence_state_repeated`、`record_evidence_state_production`、`check_evidence_freshness` | adopted | 当前 production 记录不默认引用 `doc-rvv`，因为未采纳。 |
| historical probe guarded aliases | `board_repeated_production` guard | adopted | 当前 no-production 源码下防止误跑 scalar-vs-scalar production evidence。旧 f32 checksum mismatch 没有保留可运行 guarded target，只在 phase result 中作为历史负向记录。 |

## 覆盖矩阵

| 入口 / case | point type | row source | layout | RVV gate | status |
| --- | --- | --- | --- | --- | --- |
| diagnostic scatter | `pcl::PointXYZ` | synthetic indexed neighbor list | AoS xyz float | `neighbor_count >= 16` | positive diagnostic evidence |
| diagnostic tail | `pcl::PointXYZ` | synthetic indexed neighbor list | AoS xyz float | tail VL + `neighbor_count=73` | correctness and board covered |
| production public | `pcl::PointXYZ` | `searchForNeighbors` returned indices | AoS xyz float | historical probe only | neutral, removed / no-production |
| non-RVV build | template scalar | production scalar loop | original layout | no `__RVV10__` | covered by Std gtest build |
| non-xyz / other AoS point types | generic template | production scalar fallback expected | not fully tested | traits gate | deferred / unvalidated |

## Artifact Tracking

README、evaluation、phase result 和 benchmark/evidence 文档引用的 summary evidence 都位于 `test-rvv/keypoints/iss_3d/log/board/repeated_*`。这些是 summary-only（只提交摘要）候选；`build/`、`log/qemu/` 和每轮 raw board log 默认 local-only。
