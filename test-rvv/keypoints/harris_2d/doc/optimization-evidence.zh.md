# harris_2d Optimization Evidence

| optimization / candidate | production path | test-support path | evidence | decision | boundary |
| --- | --- | --- | --- | --- | --- |
| `response-map-rvv` | `responseRVV()` adopted through dense RVV dispatch | `computeResponsesCandidate()` | QEMU 7 tests pass、asm gate pass、production direct board positive | superseded by production direct adoption | diagnostic evidence no longer final decision source |
| `direct-intensity-stride-store` | `responseRVV()` writes interior responses with `vsse32` to `PointOutT::intensity` | `bench_harris_2d --public-entry` | board median 1.07x / 1.09x / 1.18x，Evidence Doctor Errors=0 Warnings=0 | adopted | `PointXYZI -> PointXYZI`、float、organized dense、NMS disabled |
| `computeSecondMomentMatrix-dimension-fix` | `computeSecondMomentMatrix()` reads current `input_` width / height each call | `PublicComputeDoesNotReusePreviousImageSize` | RED in Std before fix，GREEN after fix | adopted | correctness bug fix, not RVV-specific |
| `direct-IntensityT-rvv` | not implemented | not implemented | none | deferred | needs traits / accessor / layout phase |
| `nms-rvv` | not implemented | not implemented | source risk audit | rejected for current scope | sort、occupancy map、critical output remain scalar |

## 当前采用的优化方式

`responseRVV()` 将 interior pixels（内部像素）按 VL chunk（可变向量长度分块）处理：从 `derivatives_rows_` / `derivatives_cols_` 连续加载窗口贡献，累加 `covar_xx/covar_xy/covar_yy`，按 method 计算 response，并通过 AoS（结构数组）stride 直接写回 `PointOutT::intensity`。边界像素仍调用 `computeSecondMomentMatrix()`，避免复杂边界 mask 扩大代码。

## 暂缓和拒绝

`direct-IntensityT-rvv` 暂缓，因为当前生产证据只覆盖 `PointXYZI` 的 public compute。把 `IntensityT` accessor 和任意点类型字段 offset 纳入 RVV 需要读 generic point type strategy（泛型点类型策略）并补新的 fallback、asm 和 board evidence。

`nms-rvv` 当前拒绝，因为它涉及排序、occupancy map 和 OpenMP critical 输出顺序。当前 phase 已经在 NMS disabled 的 public response map 上取得弱正向收益；继续做 NMS 需要 profile 证明它是主成本。
