# Benchmark 与证据

## bench 输入

- `mc_edge_sphere_64`
- `mc_edge_wave_72`
- `mc_edge_sparse_sphere_80`
- `mc_prepass_sphere_64`
- `mc_prepass_wave_72`
- `mc_prepass_sparse_sphere_80`
- `mc_prod_sphere_64`
- `mc_prod_wave_72`
- `mc_prod_sparse_sphere_80`
- `mc_prod_xyz_64`
- `mc_prod_xyzi_64`
- `mc_prod_xyzrgb_64`
- `mc_prod_xyzrgba_64`

## 证据边界

- bench 只测 helper-level 的 grid scan + surface emit。
- `prepass` 结果是强正向，但仍是 production-shaped diagnostic。
- `edge` 结果是 attempted / neutral-to-weak-positive。
- `production-direct` 结果是 adopted production evidence；`production_direct_repeated` 保留历史 `PointNormal` anchor，`generic_*_repeated` 是当前 traits-gated generic gate 的代表点型证据。

## 当前 repeated board

| family | summary | doctor |
| --- | --- | --- |
| edge interpolation | `log/board/edge_interpolation_repeated/summary.md` | `Errors=1, Warnings=4, Suggestions=2` |
| cube-index prepass | `log/board/prepass_repeated/summary.md` | `Errors=0, Warnings=3, Suggestions=0` |
| production direct historical `PointNormal` | `log/board/production_direct_repeated/summary.md` | `Errors=0, Warnings=1, Suggestions=0` |
| generic `PointXYZ` | `log/board/generic_xyz_repeated/summary.md` | `Errors=0, Warnings=0, Suggestions=0` |
| generic `PointXYZI` | `log/board/generic_xyzi_repeated/summary.md` | `Errors=0, Warnings=0, Suggestions=0` |
| generic `PointXYZRGB` | `log/board/generic_xyzrgb_repeated/summary.md` | `Errors=0, Warnings=0, Suggestions=0` |
| generic `PointXYZRGBA` | `log/board/generic_xyzrgba_repeated/summary.md` | `Errors=0, Warnings=0, Suggestions=0` |

## Generic repeated board

| point type | case | median | values | checksum |
| --- | --- | ---: | --- | --- |
| `PointXYZ` | `mc_prod_xyz_64` | `3.873x` | `3.820x, 3.873x, 3.892x, 3.876x, 3.870x` | match |
| `PointXYZI` | `mc_prod_xyzi_64` | `3.618x` | `3.618x, 3.600x, 3.618x, 3.618x, 3.633x` | match |
| `PointXYZRGB` | `mc_prod_xyzrgb_64` | `3.601x` | `3.597x, 3.566x, 3.623x, 3.601x, 3.626x` | match |
| `PointXYZRGBA` | `mc_prod_xyzrgba_64` | `3.639x` | `3.627x, 3.636x, 3.714x, 3.639x, 3.695x` | match |
