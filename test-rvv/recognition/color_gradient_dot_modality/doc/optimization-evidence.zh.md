# color_gradient_dot_modality 优化证据索引

本文件做什么：
这里把当前 topic 里发生过的 candidate family、取舍和证据路径放在一起，方便 reviewer 直接看“为什么采纳
A、为什么暂缓 B”。优化搜索空间本身仍放在 `optimization-roadmap.zh.md`。

## 当前结论

| candidate family | 代码路径 | 状态 | 证据 |
| --- | --- | --- | --- |
| `cgdm-gradient-dominant-rvv` | `recognition/include/pcl/recognition/color_gradient_dot_modality.h` | adopted production behavior | `run_test_compare`、`check_cgdm_rvv_asm`、`board_repeated` 生产 direct。 |
| `cgdm-gradient-only-rvv` | 仅 helper 级 dominant-map 预处理 | superseded | 生产 direct 已覆盖完整 public entry，不需要单独保留为当前路线。 |
| `cgdm-invariant-map-rvv` | `computeInvariantQuantizedMap()` | deferred | 状态恢复语义更复杂，当前没有 profile 证明它是热点。 |

## 差异说明

| 维度 | adopted 路径 | deferred 路径 |
| --- | --- | --- |
| dispatch | `processInputData()` 在 RVV 构建下走 `computeMaxColorGradientsRVV()` | `computeInvariantQuantizedMap()` 仍保持标量状态恢复逻辑。 |
| fallback | 非 RVV 构建自然回到标量 helper | 该路径不改 production 行为。 |
| board evidence | 生产 direct 5-run 正向 | 尚无独立 board evidence。 |
| asm | `check_cgdm_rvv_asm` 通过 | 未单独取证。 |
| 下一步 | 当前 topic closeout | 只有 template creation profile 指向它时才重开。 |

## 不能外推的边界

- 只验证了 organized `PointXYZRGB`。
- 只证明当前 public RVV path 比当前 public scalar path 快。
- 没有新的 RVV family-selection 问题，因此不需要 RVV-vs-RVV A/B。
