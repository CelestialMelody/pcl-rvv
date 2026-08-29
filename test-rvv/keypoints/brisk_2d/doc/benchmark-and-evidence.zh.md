# BRISK 2D Benchmark 与证据

## Bench case 字典

| case label | 入口 / 计时边界 | 证明点 | 不能证明什么 |
| --- | --- | --- | --- |
| `brisk_halfsample_640x480` | 构造一个 `HALFSAMPLE` 派生 `brisk::Layer` | 2x2 平均 helper 在常规尺寸上的 production direct（真实生产路径）收益 | 不覆盖 AGAST/OAST。 |
| `brisk_halfsample_641x481_tail` | 构造 odd-size source 的 `HALFSAMPLE` 派生层 | tail-derived 宽高和 vector tail（向量尾段）边界 | 不代表所有奇偶尺寸。 |
| `brisk_twothirdsample_640x480` | 构造一个 `TWOTHIRDSAMPLE` 派生层 | 3x3 -> 2x2 weighted downsample 的 RVV 收益 | 不覆盖后续 keypoint detection。 |
| `brisk_construct_pyramid_640x480` | `ScaleSpace(4)::constructPyramid(image, width, height)` | downsample helper 链在生产 `ScaleSpace` 构造路径中的组合收益 | 不覆盖完整 `BriskKeypoint2D::compute()`。 |
| `brisk_public_compute_320x240` | `BriskKeypoint2D<PointXYZRGBA>::compute()` on synthetic organized cloud | 判断 adopted downsample helper 对完整公开入口的端到端影响 | synthetic 输入不代表真实图像分布；AGAST/OAST detector 仍是标量。 |

所有 case 使用 `--iterations 100 --warmup-iterations 10`。speedup（加速比）按 `Std ms / RVV ms` 计算。

## 当前 repeated board 结果

证据路径：

- summary：`test-rvv/keypoints/brisk_2d/doc/phases/000-current-state-and-downsample-diagnostic/repeated-evidence-summary.md`
- manifest：`test-rvv/keypoints/brisk_2d/doc/phases/000-current-state-and-downsample-diagnostic/repeated-evidence-manifest.json`
- doctor：`test-rvv/keypoints/brisk_2d/doc/phases/000-current-state-and-downsample-diagnostic/repeated-evidence-doctor.md`
- registry：`test-rvv/keypoints/brisk_2d/log/evidence_registry.json`

| case | median speedup | min | max | B/A < 1 | 当前桶 |
| --- | ---: | ---: | ---: | ---: | --- |
| `brisk_halfsample_640x480` | 1.048x | 1.017x | 1.088x | 0/5 | near-threshold weak-positive |
| `brisk_halfsample_641x481_tail` | 1.041x | 1.000x | 1.051x | 0/5 | near-threshold weak-positive |
| `brisk_twothirdsample_640x480` | 1.115x | 1.105x | 1.140x | 0/5 | weak-positive |
| `brisk_construct_pyramid_640x480` | 1.175x | 1.125x | 1.200x | 0/5 | weak-positive |

Phase 000 Evidence Doctor 结果为 `Errors=0, Warnings=0, Suggestions=2`。两个 suggestion 都是 halfsample 近阈值；
处理方式是：不把 halfsample 单项写成强性能结论，但考虑实现小、fallback 简单、组合 `constructPyramid`
稳定正向，并且用户本轮授权“板卡有收益即可采纳”，因此允许 adopted。

## Phase 010 public-entry repeated board 结果

证据路径：

- summary：`test-rvv/keypoints/brisk_2d/doc/phases/010-public-compute-end-to-end/repeated-evidence-summary.md`
- manifest：`test-rvv/keypoints/brisk_2d/doc/phases/010-public-compute-end-to-end/repeated-evidence-manifest.json`
- doctor：`test-rvv/keypoints/brisk_2d/doc/phases/010-public-compute-end-to-end/repeated-evidence-doctor.md`
- registry：`test-rvv/keypoints/brisk_2d/log/evidence_registry.json`

| case | median speedup | min | max | B/A < 1 | 当前桶 |
| --- | ---: | ---: | ---: | ---: | --- |
| `brisk_halfsample_640x480` | 1.047x | 1.032x | 1.054x | 0/5 | near-threshold weak-positive |
| `brisk_halfsample_641x481_tail` | 1.039x | 1.035x | 1.053x | 0/5 | near-threshold weak-positive |
| `brisk_twothirdsample_640x480` | 1.108x | 1.099x | 1.112x | 0/5 | weak-positive |
| `brisk_construct_pyramid_640x480` | 1.136x | 1.122x | 1.140x | 0/5 | weak-positive |
| `brisk_public_compute_320x240` | 1.017x | 1.014x | 1.023x | 0/5 | neutral / near-threshold |

Evidence Doctor 结果为 `Errors=0, Warnings=0, Suggestions=3`。第三个 suggestion 是 public compute 近阈值；
处理方式是：公开入口正确性和 checksum 闭合，但端到端收益接近中性，说明 AGAST/OAST detector 和 refinement
等标量阶段稀释了 downsample helper 的收益。

## 输出策略

`log/board/repeated-phase000-downsample/run-*` 是 raw board log（原始板卡日志），包含远端路径和构建环境信息，
默认不提交。`log/board/repeated-phase010-public-compute/run-*` 同样是 raw board log，默认不提交。长期文档只引用
run label、summary、manifest、doctor 和 registry。
