# Benchmark And Evidence

## Bench cases

路径：`src/bench_range_image_border_extractor.cpp`

| case | size | 目的 |
| --- | --- | --- |
| `score_update_320x240` | 320x240 | 常规 organized score image |
| `score_update_641x481_tail` | 641x481 | 覆盖 RVV tail lanes（尾段 lane） |
| `score_update_threshold_sign_mix` | 257x193 | 覆盖较低阈值、正负符号混合和 tail |
| `score_pipeline_641x481_four_images` | four 641x481 images | 覆盖 production 中 left/right/top/bottom 四张 score image 的 batch update |
| `range_image_score_generation_160x120` | 160x120 RangeImage | 计时真实 `extractBorderScoreImages()` score generation |
| `range_image_generation_plus_update_160x120` | 160x120 RangeImage | 计时 score generation 加四方向 score-update |
| `range_image_local_surface_160x120` | 160x120 RangeImage | 单独计时 `extractLocalSurfaceStructure()` |
| `range_image_border_scores_after_surface_160x120` | 160x120 RangeImage | local surface 预生成后计时 `extractBorderScoreImages()` |

Std build 中同名 score-update case 调用 `updateScoresStd`；RVV build 中同名 score-update case 调用 `updateScoresRVV`。RangeImage case 会链接 production `range_image_border_extractor.cpp`，并只在 score-update 后段按 build 选择 test-only Std/RVV helper。输出包含 `Dataset:`、`Iterations:`、`Warmup Iterations:`、`name: X ms / iter` 和 `name checksum: Y`，兼容共享 `analyze_bench_compare.py`。

## Board evidence

Phase 000 使用过的命令形态：

```bash
make board_repeated \
  REPEATED_BOARD_TAG=phase000_score_update \
  REPEATED_BOARD_REMOTE_TAG=phase000_score_update \
  REPEATED_BOARD_RUN_LABEL=range_image_border_extractor_phase000_score_update_repeated \
  REPEATED_BOARD_CASE_NAME=score_update_641x481_tail \
  RIBE_REPEATED_BENCH_ARGS="--case-filter score_update_641x481_tail --repeat 32 --iterations 10 --warmup 2"
make evidence_doctor_repeated
```

本次 repeated case 是 `score_update_641x481_tail`，参数为 `--repeat 32 --iterations 10 --warmup 2`，采集 5 次。summary 位于 `log/board/repeated_phase000_score_update/summary.md`，manifest 位于 `log/board/repeated_phase000_score_update/evidence_manifest.json`，Doctor 报告位于 `log/board/repeated_phase000_score_update/evidence_doctor.md`。

| runs | median speedup | min | max | B/A < 1 | checksum |
| ---: | ---: | ---: | ---: | ---: | --- |
| 5 | `2.560x` | `2.520x` | `2.590x` | 0/5 | `8.74573e+08` both sides |

Evidence Doctor 结果：`Errors=0, Warnings=0, Suggestions=1`。剩余 suggestion 是缺少 taskset / governor / freq / temperature 环境字段；它不阻塞当前 diagnostic positive，但阻止升级为 strict production performance。

Phase 010 clean repeated case 是 `score_pipeline_641x481_four_images`，run label 是 `range_image_border_extractor_phase010_four_score_pipeline_repeated_clean`，参数为 `--repeat 8 --iterations 10 --warmup 2`，采集 5 次。summary 位于 `log/board/repeated_phase010_four_score_pipeline_clean/summary.md`，manifest 位于 `log/board/repeated_phase010_four_score_pipeline_clean/evidence_manifest.json`，Doctor 报告位于 `log/board/repeated_phase010_four_score_pipeline_clean/evidence_doctor.md`。当前 `make board_repeated` 默认已指向 Phase 010 four-score-image case；本次 clean run 使用 `_clean` tag 是为了替代一批远端 tag 错配的历史 run。

| runs | median speedup | min | max | B/A < 1 | checksum |
| ---: | ---: | ---: | ---: | ---: | --- |
| 5 | `2.210x` | `2.160x` | `2.230x` | 0/5 | `8.70961e+08` both sides |

Phase 020 RangeImage generation repeated case 使用 `range_image_score_generation_160x120,range_image_generation_plus_update_160x120`，run label 是 `range_image_border_extractor_phase020_range_image_generation_score_update_repeated`，参数为 `--repeat 1 --iterations 10 --warmup 2`，采集 5 次。summary 位于 `log/board/repeated_phase020_range_image_generation_score_update/summary.md`，manifest 位于 `log/board/repeated_phase020_range_image_generation_score_update/evidence_manifest.json`，Doctor 报告位于 `log/board/repeated_phase020_range_image_generation_score_update/evidence_doctor.md`。

| case | runs | median speedup | values | checksum | decision |
| --- | ---: | ---: | --- | --- | --- |
| `range_image_score_generation_160x120` | 5 | `1.000x` | `1.000x, 0.990x, 1.000x, 1.000x, 1.000x` | `24039.1` both sides | neutral |
| `range_image_generation_plus_update_160x120` | 5 | `1.000x` | `1.010x, 0.990x, 1.000x, 1.000x, 1.000x` | `26940.6` both sides | neutral |

Phase 030 score-generation component split repeated case 使用 `range_image_local_surface_160x120,range_image_border_scores_after_surface_160x120`，run label 是 `range_image_border_extractor_phase030_score_generation_component_split_repeated`，参数为 `--repeat 1 --iterations 10 --warmup 2`，采集 5 次。summary 位于 `log/board/repeated_phase030_score_generation_component_split/summary.md`，manifest 位于 `log/board/repeated_phase030_score_generation_component_split/evidence_manifest.json`，Doctor 报告位于 `log/board/repeated_phase030_score_generation_component_split/evidence_doctor.md`。

| case | runs | median speedup | values | checksum | decision |
| --- | ---: | ---: | --- | --- | --- |
| `range_image_local_surface_160x120` | 5 | `1.000x` | `0.990x, 1.000x, 1.010x, 1.000x, 1.000x` | `-26817.5` both sides | neutral |
| `range_image_border_scores_after_surface_160x120` | 5 | `1.000x` | `1.000x, 1.000x, 1.010x, 0.990x, 1.000x` | `24039.1` both sides | neutral |

Phase 020 和 Phase 030 Doctor 均为 `Errors=0, Warnings=2, Suggestions=4`。Warnings 是 1/5 B/A 低于 1；Suggestions 是环境 metadata 缺失和 median 贴近 1.0。处理方式：两个 production-shaped diagnostic 都降级为 neutral，不作为 production adoption 证据。

## Evidence output policy

`build/`、`log/qemu/`、`log/board/` 和 `log/evidence_registry.json` 默认 local-only。若用户要求提交 evidence logs，应先运行脱敏检查，并把 evidence commit 与 topic 源码 / 文档 commit 分开。
