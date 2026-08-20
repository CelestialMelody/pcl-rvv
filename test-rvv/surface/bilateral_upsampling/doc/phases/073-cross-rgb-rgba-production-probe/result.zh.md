# Phase 073 result: cross RGB/RGBA production probe

## 当前结论

本阶段把 production RVV gate 从 same-type RGB/RGBA 扩展到 RGB/RGBA exact family：
`PointXYZRGB -> PointXYZRGB`、`PointXYZRGBA -> PointXYZRGBA`、`PointXYZRGB -> PointXYZRGBA`
和 `PointXYZRGBA -> PointXYZRGB` 都可以进入 color-gather RVV helper。其它点型、其它 layout
和 `Scalar=double` 仍回退标量。

交叉 RGB/RGBA 的 production public 和 steady public 板卡结果均为正向。结合用户已说明“如果有收益，
同意先接入”，本阶段把交叉 RGB/RGBA 纳入 adopted production behavior。

## 实现回填

| action | 状态 | 证据 |
| --- | --- | --- |
| cross gate expansion | done | `kBilateralUpsamplingRVVCompatible` 取消 `std::is_same_v<PointInT, PointOutT>`，仍限制 `PointXYZRGB` / `PointXYZRGBA` 与 `RVVXYZAoSFloatLayout` |
| cross public correctness | done | 新增 `PointXYZRGB -> PointXYZRGBA` holes 和 `PointXYZRGBA -> PointXYZRGB` dense public-entry gtest |
| cross public bench labels | done | 新增 cross public / steady public 4 个 bench label |
| current manifest / doctor | done | `evidence_manifest.json` 覆盖 10 个当前 production public / steady public 对比，Evidence Doctor `Errors=0`、`Warnings=0`、`Suggestions=0` |

## 验证结果

| action | 结果 | 说明 |
| --- | --- | --- |
| QEMU correctness | pass | `make -C test-rvv/surface/bilateral_upsampling run_test_compare`，Std/RVV 均为 13/13 passed |
| asm refresh | pass | `dump_bench_rvv` 后可见 `vlse8.v`、`vzext.vf2`、`vmaxu.vv`、`vminu.vv`、`vluxei16.v`、`vfabs.v`、`vmflt.vf`、`vfredusum.vs` |
| QEMU bench smoke | pass | `make -C test-rvv/surface/bilateral_upsampling run_bench_rvv BENCH_ARGS='1 0'` 跑通，交叉 case 输出 checksum 和误差统计；QEMU timing 不作为性能结论 |
| board freshness | pass | `make -C test-rvv/surface/bilateral_upsampling board_smoke SSH_OPTS='-F /dev/null' RSYNC_SSH='ssh -F /dev/null'` 完成 correctness 与 Std/RVV bench compare |
| Evidence Doctor | pass | `doc/phases/073-cross-rgb-rgba-production-probe/evidence_manifest.json` -> `evidence-doctor.md`，`Errors=0`、`Warnings=0`、`Suggestions=0` |

## 当前板卡结果

| production public case | Std avg | RVV avg | speedup | decision |
| --- | ---: | ---: | ---: | --- |
| `PointXYZRGB 80x60 w3 dense` | 6.1123 ms | 4.8681 ms | 1.26x | positive |
| `PointXYZRGB 120x90 w4 holes` | 25.7178 ms | 21.2556 ms | 1.21x | positive |
| `PointXYZRGBA 180x120 w5 dense` | 75.3833 ms | 63.6616 ms | 1.18x | positive |
| `PointXYZRGB -> PointXYZRGBA 120x90 w4 holes` | 26.3132 ms | 21.3591 ms | 1.23x | positive |
| `PointXYZRGBA -> PointXYZRGB 120x90 w4 dense` | 26.0464 ms | 21.3000 ms | 1.22x | positive |

| steady public case | Std avg | RVV avg | speedup | decision |
| --- | ---: | ---: | ---: | --- |
| `PointXYZRGB 80x60 w3 dense` | 6.6024 ms | 5.0495 ms | 1.31x | positive |
| `PointXYZRGB 120x90 w4 holes` | 26.0561 ms | 21.5517 ms | 1.21x | positive |
| `PointXYZRGBA 180x120 w5 dense` | 77.4845 ms | 62.1629 ms | 1.25x | positive |
| `PointXYZRGB -> PointXYZRGBA 120x90 w4 holes` | 26.0141 ms | 21.5678 ms | 1.21x | positive |
| `PointXYZRGBA -> PointXYZRGB 120x90 w4 dense` | 26.1494 ms | 21.5923 ms | 1.21x | positive |

Phase 073 覆盖当前 production binary，因此 phase 072 的板卡数字降为历史 same-type freshness。当前同一
`analyze_bench_compare.log` 同时覆盖 same-type 与 cross RGB/RGBA，当前 truth 为 `public 1.26x / 1.21x / 1.18x / 1.23x / 1.22x`、`steady public 1.31x / 1.21x / 1.25x / 1.21x / 1.21x`。

## Evidence boundary

当前 adopted 结论覆盖 RGB/RGBA exact family、`Scalar=float`、organized RGBD grid 和
`RVVXYZAoSFloatLayout`。交叉输出只证明 `r/g/b/x/y/z` 语义与标量 reference 对齐；`PointXYZRGBA`
输出的 alpha 不作为本阶段 RVV 写回语义。其它点型、其它 layout、`Scalar=double`、真实 sensor
数据和更大规模仍未覆盖。

## 继续 / 停止决定

`continue_stop_decision`: adopted_cross_rgb_rgba_closeout_refreshed。

`stop_condition_hit`: none。

`next_phase_default`: 不继续自动扩到其它点型、layout 或 `Scalar=double`。这些方向需要新的点型 /
layout phase，先证明字段语义、输出语义和 production direct 性能；当前不建议在本 topic 内继续无界扩展。
