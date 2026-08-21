# image_depth benchmark and evidence

## 本文职责

本文记录 bench（性能测试）输出合同、case-filter 字典、板卡 repeated summary、Evidence Doctor（证据体检）和 evidence registry（证据登记表）。它不承担 production adoption（生产采纳）结论；结论归属到 evaluation 和 phase result。

## Bench 输出格式

`src/bench_image_depth.cpp` 输出每个 case 的 label、平均耗时、total time、checksum、pixels 和 line_step。Std build 关闭 `__RVV10__`，RVV build 打开 `__RVV10__`；两侧通过同一个 bench wrapper 和 checksum policy 对比。非 `prod_*` label 是 production-shaped diagnostic（生产形态诊断）；`prod_*` label 调用真实 `DepthImage` public entry，作为 production-public（真实公开入口）证据。`prod_depth_downsample_640x480_to_320x240` 在当前 production patch 中是 fallback coverage（回退覆盖）case：它证明深度下采样保持标量路径，不证明 RVV 收益。

## case-filter 字典

| label | 路径 | 输入 | 当前证据 | 不能证明 |
| --- | --- | --- | --- | --- |
| `depth_full_640x480` | contiguous depth meters | `640x480 -> 640x480` tight row | median 1.38x，min 1.26x，max 1.40x | production direct |
| `depth_full_padded_640x480` | contiguous depth meters with padding | `640x480 -> 640x480` padded row | median 1.21x，min 1.17x，max 1.24x | 任意 padding / production dispatch |
| `depth_downsample_640x480_to_320x240` | downsample depth meters | `640x480 -> 320x240` | median 1.17x，min 1.03x，max 1.24x | 非整数 downsample |
| `disparity_full_640x480` | contiguous disparity | `640x480 -> 640x480` | median 1.93x，min 1.64x，max 2.04x | production direct；有 long-tail warning |
| `disparity_downsample_640x480_to_320x240` | downsample disparity | `640x480 -> 320x240` | median 1.38x，min 1.13x，max 1.66x | production direct；有 long-tail warning |
| `prod_depth_full_640x480` | production-public depth meters | `640x480 -> 640x480` tight row | median 1.42x，min 1.31x，max 1.43x | 其它尺寸 / raw path |
| `prod_depth_full_padded_640x480` | production-public depth meters with padding | `640x480 -> 640x480` padded row | median 1.20x，min 1.14x，max 1.25x | 任意非 float 对齐 padding |
| `prod_depth_downsample_640x480_to_320x240` | production fallback coverage depth meters | `640x480 -> 320x240` | context median 0.99x，min 0.98x，max 1.08x | RVV speedup；当前生产入口已回退标量 |
| `prod_disparity_full_640x480` | production-public contiguous disparity | `640x480 -> 640x480` | median 1.80x，min 1.46x，max 1.82x | 有 long-tail warning |
| `prod_disparity_downsample_640x480_to_320x240` | production-public downsample disparity | `640x480 -> 320x240` | median 1.34x，min 1.13x，max 1.39x | 有 long-tail warning |

## Evidence paths

| evidence | path | role |
| --- | --- | --- |
| contiguous repeated summary | `log/board/repeated_contiguous/summary.md` | board production-shaped diagnostic |
| contiguous manifest | `log/board/repeated_contiguous/evidence_manifest.json` | Evidence Doctor input |
| contiguous doctor | `log/board/repeated_contiguous/evidence_doctor.md` | Errors=0，Warnings=1 |
| downsample repeated summary | `log/board/repeated_downsample/summary.md` | board production-shaped diagnostic |
| downsample manifest | `log/board/repeated_downsample/evidence_manifest.json` | Evidence Doctor input |
| downsample doctor | `log/board/repeated_downsample/evidence_doctor.md` | Errors=0，Warnings=2 |
| production-public repeated summary | `log/board/repeated_production_public/summary.md` | board production-public |
| production-public manifest | `log/board/repeated_production_public/evidence_manifest.json` | Evidence Doctor input |
| production-public doctor | `log/board/repeated_production_public/evidence_doctor.md` | Errors=0，Warnings=2 |
| registry | `log/evidence_registry.json` | freshness guard |

## Evidence Doctor 处理

当前 production-public doctor 没有 Error。Warnings 包含：

- `prod_disparity_downsample_640x480_to_320x240` 的 `long_tail_or_variance`：保留 min/median/max，不剔除 run。
- `prod_disparity_full_640x480` 的 `long_tail_or_variance`：min 仍为 1.46x，决策桶保持 positive。

`prod_depth_downsample_640x480_to_320x240` 的 context values 为 0.98x、0.98x、1.08x、1.00x、0.99x。它在 manifest 中标为 `production-fallback-coverage`，只记录当前生产补丁的深度下采样标量 fallback 状态，不参与 RVV speedup 异常频率判定，也不能写成 production RVV 采纳依据。

历史 production-shaped doctor 也没有 Error。Warnings 均为长尾或方差偏大。所有 summary 都保留 min/median/max；如果用户希望进一步降低 disparity 长尾风险，建议扩大 production-public runs。

## ASM Attribution

`make dump_bench_rvv` 生成 `build/asm/riscv/bench_image_depth_rvv.asm`。phase 050 后 bench binary 链接 `io/src/image_depth.cpp`，关键指令可归属到 production helper / public entry 内联边界，可见：

- contiguous path：`vle16`、`vfmul` / `vfrdiv`、masked `vse32`。
- downsample disparity path：`vlse16`、`vfrdiv`、masked `vse32`。depth downsample 在当前 production patch 中回退到标量，不作为生产 RVV asm 归属项。

`build/asm/riscv/bench_image_depth_rvv.full.asm` 中可按 `fillDepthImage*RVV` / `fillDisparityImage*RVV` helper 或内联边界定位这些指令。

## 提交边界

共享 `test-rvv/.gitignore` 只保留通用生成物规则，`log/**` 默认 ignored，不为 `image_depth` 添加 topic-specific allowlist（主题专用放行规则）。如果用户明确要求提交少量 summary、manifest、doctor 或 registry，使用 `git add -f <specific files>` 精确选择，并单独做 evidence commit。raw board logs、QEMU logs、build output、远端路径和本机配置默认排除；若用户明确要求提交 raw 或 sanitized logs（脱敏日志），需要另跑日志脱敏和提交拆分流程。
