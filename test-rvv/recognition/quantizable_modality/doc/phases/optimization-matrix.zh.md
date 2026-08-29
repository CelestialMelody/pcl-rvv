# Optimization Matrix

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `shared-spread-rvv-2pass` | organized quantized byte map | byte map / not applicable point type / contiguous storage | 公共 `QuantizedMap::spreadQuantizedMap()`，默认 `spreading_size == 8` | `run_test_compare` 3/3；forced scalar/RVV 对拍；非默认 spread fallback | `bench_qm --case-filter shared_spread_320x240,shared_spread_641x481_tail` | 5-run positive，median `4.670x` / `4.770x`，checksum 一致，`B/A < 1 = 0/5` | `check_qm_rvv_asm` 命中 `vle8/vor/vse8` | `Errors=0 / Warnings=0 / Suggestions=4` | adopted | closeout |
| `variable-spread-rvv` | organized quantized byte map | byte map | 非默认 spreading size | fallback test only | not covered | not covered | not covered | not covered | deferred / separate phase | 只有 caller 证明非默认 spread 是热点后再启动 |
| `caller-end-to-end-rvv` | modality public entry | caller-specific point type / layout | `ColorModality`、`ColorGradientModality`、`SurfaceNormalModality` public entry | not covered | not covered | not covered | not covered | not covered | separate-topic | 另开 caller public-entry topic |
