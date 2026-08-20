# Phase 072 result: production same-type gate alignment

## 当前结论

本阶段将 production RVV gate 收窄到 same-type RGB/RGBA：只让 `PointXYZRGB -> PointXYZRGB` 与 `PointXYZRGBA -> PointXYZRGBA` 进入 adopted color-gather RVV helper。交叉 `PointXYZRGB -> PointXYZRGBA` / `PointXYZRGBA -> PointXYZRGB` 组合没有 production direct correctness、bench manifest 或板卡证据，因此继续回退标量路径。

这个修正不改变 adopted family 的主体。当前生产路径仍是 color-gather RVV helper 优先，失败后回退旧 RVV helper，再回退标量 helper；变化是源码启用范围现在与已验证证据范围一致。

## 实现回填

| action | 状态 | 证据 |
| --- | --- | --- |
| same-type gate | done | `kBilateralUpsamplingRVVCompatible` 增加 `std::is_same_v<PointInT, PointOutT>` |
| production 注释 | done | `performProcessing` 注释改为 verified same-type RGB/RGBA layouts |
| 文档范围同步 | done | `doc-rvv`、evaluation、testing、benchmark/evidence、optimization evidence、roadmap、phase matrix、queue 和 Handoff |

## 验证结果

| action | 结果 | 说明 |
| --- | --- | --- |
| QEMU correctness | pass | `make -C test-rvv/surface/bilateral_upsampling run_test_compare`，Std/RVV 均为 11/11 passed。 |
| asm refresh | pass | `dump_bench_rvv` 后可见 `vlse8.v`、`vzext.vf2`、`vmaxu.vv`、`vminu.vv`、`vluxei16.v`、`vfredusum.vs`，并可见 finite mask 的 `vfabs.v` / `vmflt.vf`。 |
| QEMU bench smoke | pass | `make -C test-rvv/surface/bilateral_upsampling run_bench_rvv BENCH_ARGS='1 0'` 跑通，所有 case 输出 checksum 和误差统计；QEMU timing 不作为性能结论。 |
| board freshness | pass | `make -C test-rvv/surface/bilateral_upsampling board_smoke SSH_OPTS='-F /dev/null' RSYNC_SSH='ssh -F /dev/null'` 完成 correctness 与 Std/RVV bench compare。 |
| Evidence Doctor | pass | `doc/phases/072-production-same-type-gate-alignment/evidence_manifest.json` -> `evidence-doctor.md`，`Errors=0`、`Warnings=0`、`Suggestions=0`。 |

## 当前板卡结果

| production public case | Std avg | RVV avg | speedup | decision |
| --- | ---: | ---: | ---: | --- |
| `PointXYZRGB 80x60 w3 dense` | 6.7632 ms | 4.7442 ms | 1.43x | positive |
| `PointXYZRGB 120x90 w4 holes` | 25.7794 ms | 21.5760 ms | 1.19x | positive |
| `PointXYZRGBA 180x120 w5 dense` | 77.5731 ms | 64.1031 ms | 1.21x | positive |

| steady public case | Std avg | RVV avg | speedup | decision |
| --- | ---: | ---: | ---: | --- |
| `PointXYZRGB 80x60 w3 dense` | 6.5152 ms | 5.0417 ms | 1.29x | positive |
| `PointXYZRGB 120x90 w4 holes` | 26.2818 ms | 21.7013 ms | 1.21x | positive |
| `PointXYZRGBA 180x120 w5 dense` | 80.2225 ms | 62.5394 ms | 1.28x | positive |

Phase 070 的 near-threshold RGBA public case 为 `1.02x`，phase 072 当前二进制刷新后为 `1.21x`。因此 near-threshold 风险降为历史风险；当前仍是 single board smoke，不是 repeated suite。

## Evidence boundary

当前 adopted 结论只覆盖 same-type `PointXYZRGB -> PointXYZRGB` 与 `PointXYZRGBA -> PointXYZRGBA`、`Scalar=float`、organized RGBD grid 和 `RVVXYZAoSFloatLayout`。交叉 RGB/RGBA 输出、其它点型、其它 layout、`Scalar=double`、真实 sensor 数据和更大规模都没有被本阶段扩展。

本轮默认 SSH config 曾让 `board_smoke` 超时；板卡端口可达后，使用 `SSH_OPTS='-F /dev/null' RSYNC_SSH='ssh -F /dev/null'` 完成部署和运行。该环境细节只影响复现命令，不改变性能证据边界。

## 继续 / 停止决定

`continue_stop_decision`: adopted_closeout_refreshed。

`stop_condition_hit`: none。

`next_phase_default`: 不继续沿 helper-only / mask-chunk 旧线自动复跑；若用户要求提交，进入 commit / staging 审计；若要扩大交叉点型、其它点型、layout 或 `Scalar`，另开 point-type expansion phase。
