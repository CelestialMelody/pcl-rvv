# color_gradient_dot_modality Optimization Matrix

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `cgdm-gradient-dominant-rvv` | organized RGB image -> bin map | `PointXYZRGB` input，float magnitude / angle，`uint8_t` output map | test helper `computeDominantMapCandidate`; production `processInputData()` -> `computeMaxColorGradientsRVV()` | `run_test_compare` Std/RVV 3 tests passed | helper `dominant_map_*` median `3.080x` / `3.990x`; production `process_input_*` median `2.600x` / `3.510x` | production direct 5-run positive，0/5 degradation | `check_cgdm_rvv_asm` passed | production direct Errors=0，Warnings=0，Suggestions=4 | adopted production behavior | none for current gradient path |
| `cgdm-invariant-map-rvv` | region / mask driven bin neighborhood | `PointXYZRGB` input + `MaskMap` / `RegionXY` | future `computeInvariantQuantizedMap()` helper | not run | not run | not run | not run | not run | deferred | reopen only if template creation profile points to invariant map |

当前矩阵没有新的 unblocked 算法候选；Phase 020 只处理 closeout 和 commit readiness。
