# Harris 2D optimization roadmap

当前 topic 覆盖 `keypoints/include/pcl/keypoints/impl/harris_2d.hpp` 的 organized response map。roadmap（优化路线图）只保存跨 phase 的候选搜索空间和恢复条件；阶段事实归属到 `doc/phases/`，最终生产行为写入 `doc-rvv/keypoints/harris_2d-RVV.zh.md`。

## 默认恢复队列

| priority | phase | scope | status | resume condition |
| ---: | --- | --- | --- | --- |
| 1 | `020-direct-intensity-stride-store` | public `PointXYZI -> PointXYZI` response map | completed / adopted | 默认恢复到 review；同范围无未阻塞性能动作 |
| 2 | `030-point-type-accessor-expansion` | broader `IntensityT` / point type traits | deferred | 用户明确要求扩大 production 范围，且允许读取 generic point type strategy 并补完整证据 |
| 3 | `040-nms-profile-or-rvv` | NMS enabled path | rejected for current scope | profile 证明 NMS 是主成本且输出顺序 oracle 可定义 |

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `response-map-rvv` | keypoints queue + current source shape | organized full-image response map | derivative、second-moment 和 formula 批量化 | 小窗口规约收益有限 | QEMU correctness、asm、board repeated、Evidence Doctor | adopted through production direct | none |
| `direct-intensity-stride-store` | phase020 reflection | public response output | 减少中间 response buffer 和末尾拷贝 | AoS stride 写回需要字段 offset 正确 | public direct tests、asm、board repeated | adopted | none |
| `computeSecondMomentMatrix-dimension-fix` | board correctness diagnosis | scalar and RVV public entry | 修复同进程多尺寸 correctness | 改变历史 bench baseline，需要重跑 board | RED/GREEN test、board rerun | adopted | none |
| `direct-IntensityT-rvv` | production integration follow-up | public template entry | 减少泛型 accessor / staging 成本 | `IntensityT` 和点类型 layout 复杂 | traits / accessor audit、fallback tests、board | deferred | 030 |
| `nms-rvv` | queue risk item | NMS threshold / occupancy map | 可能减少后处理时间 | sort、状态写入、输出顺序和 OpenMP critical 风险高 | profile、correctness oracle、order stability tests | rejected for current scope | 040 only if profile justifies |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 020 | environment / binary metadata capture | Evidence Doctor 留下 6 个 non-blocking suggestions | board runner 记录 taskset、governor、freq、temperature、binary hash | low unless release-grade report required |
| 020 | point-type accessor expansion | production patch 目前只证明 `PointXYZI` | generic point type strategy、fallback、QEMU、asm、board | medium if user wants wider template coverage |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| `direct-IntensityT-rvv` | 当前采纳范围已经有收益；扩大泛型点类型会引入新的 traits / layout 风险 | 用户要求扩大，或真实 workload 使用非 `PointXYZI` |
| `nms-rvv` | NMS 包含 sort、occupancy map 和 critical push，当前无 profile 证明它主导成本 | profile 指向 NMS 主导且能定义稳定输出顺序 oracle |

## Stop decision

当前授权范围内没有值得继续推进的同范围优化方向。已采纳实现的收益来自 public response map；进一步优化都需要扩大点类型、`IntensityT` 或 NMS scope，因此默认暂停在 production adopted / ready for review。
