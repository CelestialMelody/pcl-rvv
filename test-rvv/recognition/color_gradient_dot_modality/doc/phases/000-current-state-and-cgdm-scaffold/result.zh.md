# Phase 000: current-state and CGDM scaffold result

## 当前结论

本阶段完成 `cgdm-gradient-dominant-rvv` production-shaped diagnostic（生产形态诊断）。测试资产先复刻
`ColorGradientDOTModality<PointXYZRGB>::processInputData()` 中的 RGB 差分、最大通道选择、`sqrt` / `atan2`
和 dominant map（主方向图）生成；随后在 test-only candidate 中把 RGB 字段跨步加载、平方幅值、通道选择、
`vfsqrt` 和 `atan2_RVV_f32m2` 接入 RVV（RISC-V Vector，RISC-V 向量扩展）。

诊断 helper 在板卡上稳定 positive，但本阶段的 `process_input_*` 只是 public-entry cross-check（公开入口交叉检查）：
当时 production 源码尚未接入 RVV，所以它只证明 production 尚未分流，不参与采纳判断。

## 执行记录

| step | status | evidence |
| --- | --- | --- |
| RED | done | `run_test_rvv` 先失败于 candidate path-hit，随后修正 oracle（正确性参考）与 production 的 `std::tan(1.0f)*4` 和 `GradientXY::x/y` 语义。 |
| GREEN | done | `make -C test-rvv/recognition/color_gradient_dot_modality run_test_compare` 通过 Std/RVV 3 个 gtest。 |
| QEMU smoke | done | `run_bench_rvv BENCH_ARGS="--case-filter dominant_map_320x240 --iterations 1 --warmup-iterations 1"` 通过；QEMU 时间不作为性能结论。 |
| asm | done | `check_cgdm_rvv_asm` 通过，bench asm 可见 RVV load / convert / `vfsqrt` / `atan2_RVV` 相关指令。 |
| board repeated | done | `log/board/repeated_phase000_dominant_map/summary.md`。 |
| Evidence Doctor | done | `log/board/repeated_phase000_dominant_map/evidence_doctor.md`：Errors=0，Warnings=2，Suggestions=10。Warnings 来自未接 production 的 `process_input_*` neutral cross-check，不影响 helper diagnostic。 |

## 诊断板卡结果

| case | evidence role | runs | median speedup | p10 - p90 | B/A < 1 | checksum |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| `dominant_map_320x240` | production-shaped diagnostic | 5 | `3.080x` | `3.052x` - `3.144x` | 0/5 | `8278385795811028032` |
| `dominant_map_641x481_tail` | production-shaped diagnostic | 5 | `3.990x` | `3.962x` - `4.000x` | 0/5 | `888857921314290191` |

## diagnostic-to-production mismatch audit

| item | decision |
| --- | --- |
| evidence role | `dominant_map_*` 是 test helper diagnostic，只证明候选算法和数据布局值得接 production probe。 |
| A/B boundary | Std/RVV 两侧共享 `bench_cgdm.cpp` 的 helper wrapper；不等于 production public entry。 |
| comparison-boundary risk | Phase 000 的 `process_input_*` 为 neutral，因为当时 production 还没有 RVV 分流；这不是候选无效，而是 production 尚未接入。 |
| bounded production probe condition | helper correctness、asm 和 5-run board 都通过，且用户本轮授权“接入后板卡有收益即可采纳”，因此进入 Phase 010 production integration。 |

## 下一步

Phase 010 已创建，目标是把 `computeMaxColorGradientsRVV()` 接入 production public entry，并用 production direct
（真实生产路径）板卡结果决定是否采纳。
