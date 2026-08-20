# 优化证据

## 已验证 family

| family | decision | evidence |
| --- | --- | --- |
| edge-interpolation-rvv | attempted / not production candidate | correctness 通过，但 board repeated 只有弱收益，doctor 还有退化 Error |
| cube-index-prepass | strong-positive / diagnostic complete | correctness 通过，board repeated 明显正向，doctor 无 Error |
| production-active-cell-prepass | adopted / generic production | `RVVXYZAoSFloatLayout<PointNT>` gate 已接入；QEMU Std/RVV 各 6 tests passed；`PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 5-run board repeated 均为 positive，Evidence Doctor 全部无 finding |
| active-z finite-collapse single-buffer | attempted / neutral / not adopted | correctness 通过且 asm 证明少一组 staging store，但 RVV-vs-RVV `mc_prod_xyz_64` 3-run median 只有 `1.007x`，Evidence Doctor `Errors=0, Warnings=1, Suggestions=1` |

## 当前结论

- production 已接入 generic xyz AoS gate：RVV 负责 active-cell prepass，三角点 emission 仍走原标量 `createSurface()`。
- 当前证据不支持把 edge interpolation RVV 接入 production。
- generic 代表点型 correctness 和 5-run board repeated 已补；不满足 traits gate 的 fallback correctness 也已补。
- Phase 060 已尝试 active-z finite-collapse single-buffer，收益在 RVV-vs-RVV 同边界 A/B 中属于 neutral，因此当前 production truth 保持 Phase 050 adopted generic prepass。
- 更复杂的 active-z table lookup / `vcompress` 需要 profile 或 component ablation 证明 tail 仍是瓶颈；真实 Hoppe / RBF 输入分布需要单独 phase 或子 topic。

## Generic repeated 证据

| point type | case | median | doctor |
| --- | --- | ---: | --- |
| `PointXYZ` | `mc_prod_xyz_64` | `3.873x` | `Errors=0, Warnings=0, Suggestions=0` |
| `PointXYZI` | `mc_prod_xyzi_64` | `3.618x` | `Errors=0, Warnings=0, Suggestions=0` |
| `PointXYZRGB` | `mc_prod_xyzrgb_64` | `3.601x` | `Errors=0, Warnings=0, Suggestions=0` |
| `PointXYZRGBA` | `mc_prod_xyzrgba_64` | `3.639x` | `Errors=0, Warnings=0, Suggestions=0` |

## Phase 060 RVV-vs-RVV 证据

| case | median | values | doctor | decision |
| --- | ---: | --- | --- | --- |
| `mc_prod_xyz_64` | `1.007x` | `1.002x, 1.007x, 1.012x` | `Errors=0, Warnings=1, Suggestions=1` | neutral / not adopted |
