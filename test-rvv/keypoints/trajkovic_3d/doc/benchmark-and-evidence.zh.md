# Trajkovic 3D Benchmark 与证据

## Bench 入口

```bash
make -C test-rvv/keypoints/trajkovic_3d run_bench_compare
make -C test-rvv/keypoints/trajkovic_3d run_bench_compare BENCH_ARGS="--mode public --case-filter all --iterations 20 --warmup 3 --width 320 --height 240"
make -C test-rvv/keypoints/trajkovic_3d board_repeated_production record_evidence_state_production
make -C test-rvv/keypoints/trajkovic_3d check_evidence_freshness
```

QEMU（仿真器）bench logs 可用于检查输出格式和 checksum，但不能作为性能结论。性能结论只来自 board（板卡）或目标硬件 repeated benchmark（重复性能测试）。

## Case 字典

| case | mode | 入口 | 数据 | 计时边界 | 证据角色 |
| --- | --- | --- | --- | --- | --- |
| `four_corners_320x240` | diagnostic | test helper | finite 320x240 synthetic | response map only | production-shaped diagnostic |
| `four_corners_invalid_320x240` | diagnostic | test helper | invalid point / normal | response map only | production-shaped diagnostic |
| `four_corners_641x481_tail` | diagnostic | test helper | tail-size synthetic | response map only | production-shaped diagnostic |
| `public_four_corners_320x240` | public | `Detector::compute()` | dense 320x240 with precomputed normals | response + NMS + output | production public |
| `public_eight_corners_320x240` | public | `Detector::compute()` | dense 320x240 with precomputed normals | response + NMS + output | production public |
| `public_four_corners_invalid_320x240` | public | `Detector::compute()` | non-dense control | scalar fallback public compute | fallback control |
| `public_eight_corners_invalid_320x240` | public | `Detector::compute()` | non-dense control | scalar fallback public compute | fallback control |
| `public_four_corners_641x481_tail` | public | `Detector::compute()` | non-dense tail control | scalar fallback public compute | fallback control |
| `public_eight_corners_641x481_tail` | public | `Detector::compute()` | non-dense tail control | scalar fallback public compute | fallback control |

## 当前 Production Evidence

| summary | run label | result |
| --- | --- | --- |
| `log/board/repeated_phase020_eight_corners_production_public/summary.md` | `trajkovic_3d_phase020_eight_corners_production_public_repeated` | `public_four_corners_320x240` median 1.790x，min 1.762x，max 1.820x，B/A < 1 为 0/5；`public_eight_corners_320x240` median 1.677x，min 1.613x，max 1.705x，B/A < 1 为 0/5。 |

配套路径：

- manifest：`log/board/repeated_phase020_eight_corners_production_public/evidence_manifest.json`
- doctor：`log/board/repeated_phase020_eight_corners_production_public/evidence_doctor.md`
- registry：`log/evidence_registry.json`

Evidence Doctor 结果为 Errors=0、Warnings=0、Suggestions=4。四个 suggestion 分别是缺少环境 metadata 和 binary hash，分别覆盖 public_four_corners_320x240 与 public_eight_corners_320x240；当前 checksum 一致且 5-run 全部 positive，因此不阻塞窄范围采纳。

## 历史 Diagnostic Evidence

Phase 000 response-only summary 位于 `log/board/repeated_phase000_four_corners_response/summary.md`。它证明 response helper 有 2.317x 到 3.596x 的局部收益，但最终 production 文档不把该数字写成公开入口收益。

## Freshness 和提交边界

`make check_evidence_freshness` 检查 Phase 000、Phase 010 和 Phase 020 的 summary / manifest / doctor 是否与 registry 一致，并要求这些路径被 phase result、evaluation 或 `doc-rvv` 引用。`log/**` 默认不提交；如需提交证据，优先提交被本文和长期文档引用的 summary / manifest / doctor，raw run logs 继续 local-only。
