# PI2 result: shape-bin indexed production probe

> PI3 status: 本阶段的 production patch 已在 `PI3-shape-bin-production-rollback-closeout` 中按用户确认回滚。下文保留 PI2 当时的采证事实和 PI5 暂停语境，用作 historical production probe（历史生产探针）证据。

## 当前结论

本阶段完成 PI2-PI5 的 production integration loop（生产接入闭环）采证，但证据不支持直接采纳当前 production patch（生产补丁）。

实际结果是：真实 `createBinDistanceShape` production-detail（生产细节 helper）在 Milkv-Jupiter 单次 board compare 为 1.07x，checksum 一致；但两个 production-public（生产公开入口）case 接入后没有收益，`public_shot352_fixed_lrf` 为 0.98x，`public_shot1344_fixed_lrf` 为 0.99x，且 Evidence Doctor 对两个 public case 都给出 `ba_degradation_frequency` Error。PI2 当时按 workflow 进入 `pending_user_confirmation_rollback`；PI3 已按用户确认回滚 patch，不创建正式 `doc-rvv/features/shot-RVV.zh.md`。

## 实际执行范围

| item | planned | actual |
| --- | --- | --- |
| production scope | 只接 `createBinDistanceShape` 的 indexed normal gather | 已按范围完成；未触碰 interpolation、color LAB/RGB、LRF、OMP 或 public API。 |
| point type / layout | `PointNT` normal traits + AoS layout gate | 使用 `pcl::traits::has_normal`、single-float normal field、POD standard-layout、`sizeof(PointNT)==sizeof(POD)` 和 field offset alignment gate。 |
| fallback | 非 RVV、小规模、layout 不满足、过大 32-bit byte offset 回退标量 | 已实现；小规模 direct test 覆盖 fallback 结果。 |
| side effect | 保留 NaN normal count 和 `PCL_WARN` 文本 | direct tests 中 NaN count 和 warning 输出保持。 |
| evidence | correctness、asm、board、Evidence Doctor、registry | 已完成。 |

## Production diff 摘要

`features/include/pcl/features/impl/shot.hpp` 新增：

- `pcl::detail::SHOTRVVNormalAoSLayout`：阶段本地 normal AoS layout gate（布局准入）。
- `pcl::detail::shotCreateBinDistanceShapeRVV`：`__RVV10__` 下的 indexed gather RVV helper，使用 `pcl::rvv_load::indexed_load3_fields_f32m2`，RVV 路径计算 finite mask、f64 dot/clamp/bin 和 `vcpop` NaN count。
- `pcl::detail::shotCreateBinDistanceShapeStd`：原标量循环抽出的 fallback helper。
- `createBinDistanceShape` 现在先尝试 RVV helper，失败时调用 Std helper；`PCL_WARN` 仍由入口统一触发。

公开 API 未变。

## Correctness / fallback 证据

| command | result | evidence boundary |
| --- | --- | --- |
| `make -C test-rvv/features/shot run_test_shape_bin` | Std/RVV 各 6/6 pass | shape-bin component、production-detail、`PointNormal` layout 和小规模 fallback。 |
| `make -C test-rvv/features/shot run_test_compare` | Std/RVV 各 15/15 pass | public SHOT352/SHOT1344、所有既有 component diagnostic 和 production-detail tests。 |
| `make -C test-rvv/features/shot run_board_test REMOTE_TEST_ARGS='--gtest_filter=ShotShapeBinProductionDetail.*'` | board RVV 3/3 pass | 目标硬件上真实 helper direct correctness；不是性能结论。 |

新增 production-detail tests：

- `CreateBinDistanceShapeMatchesReferenceForIndexedNormalCloud`
- `CreateBinDistanceShapeMatchesReferenceForPointNormalLayout`
- `SmallNeighborhoodKeepsScalarResult`

## ASM attribution（反汇编归因）

`make -C test-rvv/features/shot dump_bench_rvv` 生成 `build/asm/riscv/bench_shot_rvv.full.asm` 和 `build/asm/riscv/bench_shot_rvv.asm`。`full.asm` 中能定位到：

- `pcl::SHOTEstimationBase<pcl::PointXYZ, pcl::Normal, pcl::SHOT352, pcl::ReferenceFrame>::createBinDistanceShape(...)`
- `pcl::SHOTEstimationBase<pcl::PointXYZRGBA, pcl::Normal, pcl::SHOT1344, pcl::ReferenceFrame>::createBinDistanceShape(...)`

附近可见 `vluxei32.v`、`vfwcvt.f.f.v` 和 `vcpop.m`，说明生产 helper 符号内命中 indexed normal gather、f32-to-f64 widen 和 NaN count 路径。asm 文件是生成产物，不默认提交。

## Board evidence（板卡证据）

| run label | evidence role | case | Std ms | RVV ms | speedup | checksum | Doctor |
| --- | --- | --- | ---: | ---: | ---: | --- | --- |
| `board-shot-production-shape-bin-direct-pi2` | production-detail | `production_shape_bin_direct` | 4.48743 | 4.17598 | 1.07x | match | Errors=0, Warnings=1 |
| `board-shot-public-shot352-production-pi2` | production-public | `public_shot352_fixed_lrf` | 0.122331 | 0.125424 | 0.98x | match | Errors=1, Warnings=1 |
| `board-shot-public-shot1344-production-pi2` | production-public | `public_shot1344_fixed_lrf` | 0.268388 | 0.269739 | 0.99x | match | Errors=1, Warnings=1 |

证据路径：

- `log/board/board-shot-production-shape-bin-direct-pi2/analyze_bench_compare.log`
- `log/board/board-shot-production-shape-bin-direct-pi2/evidence_manifest.json`
- `log/board/board-shot-production-shape-bin-direct-pi2/evidence_doctor.md`
- `log/board/board-shot-public-shot352-production-pi2/analyze_bench_compare.log`
- `log/board/board-shot-public-shot352-production-pi2/evidence_manifest.json`
- `log/board/board-shot-public-shot352-production-pi2/evidence_doctor.md`
- `log/board/board-shot-public-shot1344-production-pi2/analyze_bench_compare.log`
- `log/board/board-shot-public-shot1344-production-pi2/evidence_manifest.json`
- `log/board/board-shot-public-shot1344-production-pi2/evidence_doctor.md`

## Evidence Doctor 处理

| finding | 处理 |
| --- | --- |
| production-detail `low_run_count` Warning | 只把 1.07x 写成 weak-positive initial signal（弱正向初筛信号），不写强 production performance。 |
| public SHOT352 `ba_degradation_frequency` Error | 这是 production-public 退化；阻断采纳。低 run count 可能放大波动，但当前方向已经不支持“接入后有收益”。 |
| public SHOT1344 `ba_degradation_frequency` Error | 同上；color descriptor 共享 shape path 后仍没有端到端收益。 |

`make -C test-rvv/features/shot evidence_status` 显示 registry fresh。

## Diagnostic-to-production mismatch audit 回填

Phase 040 的 1.65x-1.86x indexed component positive 只能证明 shape-bin 局部 helper 有潜力；PI2 生产证据显示该收益在 public entry 中被 search、interpolation、descriptor normalization、output copy 和函数调用 / dispatch 成本稀释。当前 public Std/RVV negative 不能支持 clean adoption（干净采纳）。

当前不是 RVV-family-selection 问题：此前没有 SHOT production RVV family。即便如此，production public negative 已经足以把本 patch 降级；PI3 已按用户确认回滚。

## Optimization matrix 更新

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback | board evidence | asm | Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| shape-bin indexed production probe | indexed normal cloud | `PointNT` normal traits + AoS, f32 normal -> f64 bin distance | QEMU Std/RVV 15/15；board direct 3/3 | detail 1.07x, public 0.98x / 0.99x | production helper has `vluxei32.v` / `vcpop.m` | public Errors=1 each | attempted / rolled back in PI3 | 不创建 `doc-rvv`。 |

## Continue / stop decision

`stop_condition_hit`: PI5 production evidence decision requires user confirmation（生产证据决策需要用户确认）。

没有继续默认优化方向：

- interpolation geometry staging、interpolation bin-selection 和 color RGB/LUT staging 已有负向或不稳定证据；
- shape-bin production public evidence 没有收益；
- normalization component 虽然正向，但 public entry 历史约 1x，且本轮已经证明局部收益可被端到端成本吞掉；若要继续 normalization production probe，需要新的 PI1 授权和独立计划，不应在当前 shape-bin PI2 patch 上继续叠加。

`next_phase_default`: superseded by PI3 rollback closeout。PI3 已移除 `shot.hpp` production patch，并把 topic 收口为 no-production / diagnostic closeout。
