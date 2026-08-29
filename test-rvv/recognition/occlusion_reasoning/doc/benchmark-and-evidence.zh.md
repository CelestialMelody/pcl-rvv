# Bench 与证据

当前 bench 分两条 production direct（真实生产路径）边界：

- Phase 020：public inline `filter(scene, model, f, threshold)`，计时包含 `copyPointCloud`，
  不包含 scene/model setup。这是 item 6 的当前性能 truth。
- Phase 010：`ZBuffering::filter(model, indices, thres)`，计时不包含 `copyPointCloud`。
  这是 item 7 的已采纳基线。

Phase 000 的 raw depth diagnostic（原始深度诊断）仍保留作历史基线，但不再作为当前
production performance truth（生产性能事实）。

## Bench 输出合同

输出必须包含：

- `Dataset:`：输入规模、depth 分辨率和 case 名。
- `Iterations:` 与 `Warmup Iterations:`。
- Phase 020：`public_inline_filter_visible_points: <ms> / iter`。
- Phase 010：`production_filter_indices_projection_mask_compress: <ms> / iter`。
- checksum（校验和）、kept count（保留数量）和 wrapper mode（包装边界）。

## 板卡复跑

默认运行 5 次，每次 200 iterations、5 warmup。decision bucket（决策桶）：

- `positive`：median speedup >= 1.20 且 0/5 退化。
- `weak_positive`：median speedup 在 `[1.05, 1.20)` 且方向稳定。
- `neutral`：median 在 `[0.95, 1.05)`。
- `negative`：median < 0.95。
- `unstable`：预算内跨桶摇摆或 Evidence Doctor 暴露未解决 Error。

## 当前 Production 证据

| phase | run label | case | timer boundary | decision bucket | median | min / max | B/A < 1 | checksum |
| --- | --- | --- | --- | --- | ---: | ---: | ---: | --- |
| 020 | `occlusion_reasoning_phase020_inline_filter_production_direct_repeated` | `public_inline_filter_visible_points` | `production_public_inline_filter_includes_copyPointCloud_excludes_scene_model_setup` | positive | `1.250x` | `1.240x` / `1.260x` | `0/5` | stable |
| 010 | `occlusion_reasoning_phase010_production_direct_repeated` | `production_filter_indices_projection_mask_compress` | `production_public_filter_indices_after_computeDepthMap_setup_excludes_copyPointCloud` | positive | `1.270x` | `1.260x` / `1.270x` | `0/5` | stable |

## 证据路径

Phase 020 public inline:

- repeated summary：`test-rvv/recognition/occlusion_reasoning/log/board/repeated_phase020_inline_filter_production_direct/summary.md`
- manifest：`test-rvv/recognition/occlusion_reasoning/log/board/repeated_phase020_inline_filter_production_direct/evidence_manifest.json`
- Evidence Doctor：`test-rvv/recognition/occlusion_reasoning/log/board/repeated_phase020_inline_filter_production_direct/evidence_doctor.md`
- registry：`test-rvv/recognition/occlusion_reasoning/log/evidence_registry.json`

Phase 010 ZBuffering:

- repeated summary：`test-rvv/recognition/occlusion_reasoning/log/board/repeated_phase010_production_direct/summary.md`
- manifest：`test-rvv/recognition/occlusion_reasoning/log/board/repeated_phase010_production_direct/evidence_manifest.json`
- Evidence Doctor：`test-rvv/recognition/occlusion_reasoning/log/board/repeated_phase010_production_direct/evidence_doctor.md`

## 历史基线

Phase 000 的历史诊断基线只用于解释 production direct 前的候选来源：

- `test-rvv/recognition/occlusion_reasoning/log/board/repeated_phase000_filter_diagnostic/summary.md`
- `test-rvv/recognition/occlusion_reasoning/log/board/repeated_phase000_projection_mask_compress/summary.md`

## Evidence Doctor

Phase 020 与 Phase 010 的 Evidence Doctor 结果均为 `Errors=0`、`Warnings=0`、
`Suggestions=1`。唯一建议是补齐 `taskset`、`governor`、`freq`、`temperature`
等环境字段；它不阻塞当前结论，但后续 board 记录应补上。

raw board logs（原始板卡日志）默认不提交。
