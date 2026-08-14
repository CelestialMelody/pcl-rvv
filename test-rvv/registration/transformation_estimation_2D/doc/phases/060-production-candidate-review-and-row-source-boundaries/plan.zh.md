# Phase 060 Plan: production-candidate-review-and-row-source-boundaries

## 阶段意图和边界

本阶段承接最新真实板卡证据，先审阅当前 ordered-cloud-pair production patch，再决定是否进入采用/提交；同时整理 indexed / correspondence row source 的下一候选边界。

本阶段不自动回滚 production patch，不把 row-source 的弱收益升级为 production，不扩大到泛型点型或 `Scalar=double`。如果需要删除、覆盖或回滚 production patch，必须先由用户明确授权。

## 当前输入

| 输入 | 当前事实 |
| --- | --- |
| production patch | `transformation_estimation_2D.hpp` 保留 exact `PointXYZ -> PointXYZ`、`Scalar=float`、dense finite ordered-cloud-pair gate |
| correctness | Std / RVV 各 16/16 |
| production-public board | `Milkv-Jupiter`：4K `4.222x`、64K `5.310x`、256K `4.947x`，Doctor 0/0/0 |
| row-source board | 9 cases，Doctor 1/2/6；64K 有退化/长尾 |

## 计划动作

| id | 动作 | 完成判据 |
| --- | --- | --- |
| A1 | production diff / dispatch / fallback review | 写清命中条件、回退条件、未覆盖 overload 和用户可见 diff；不执行回滚 |
| A2 | production evidence provenance review | 校对 board manifest 的 device、run count、iterations、warmup、binary hash、timer boundary 与当前源码 |
| A3 | row-source 64K stability analysis | 对 source-indexed、dual-indexed、correspondence 的 materialize、缓存局部性、长尾和退化频率给出最小解释；不先验删除异常 |
| A4 | next candidate decision | ordered candidate 进入 review-pending；row-source 只有形成新的 gather/staging 方案后才创建下一 PI1 |
| A5 | validation refresh | `evidence_status`、脚本 `py_compile`、YAML parse、`git diff --check` 全部通过 |

## 继续 / 停止条件

继续条件：

- 当前 patch 的 gate / fallback 仍能由源码和 direct correctness 解释；
- 当前 board manifest 与本次编译/部署边界一致；
- row-source 的下一候选能明确到数据布局、访存或 staging 机制。

合法停止条件：

- 用户需要先审阅 production diff；
- 扩大到 indexed / correspondence production 需要新的 PI1；
- 继续修改会触及其它 topic 或超出当前 production scope。

以下情况不能作为停止理由：

- 单纯因为已有正向 board 结果；
- 单纯因为 row-source 64K 需要解释而删除异常数据。

## 预期下一阶段

若用户确认采用当前窄范围 patch：进入 production closeout / commit review。

若继续 row-source：先建立 `row-source-layout-stability-diagnostic`，聚焦 materialize-to-ordered 与真实 gather/staging 的差异；即使新候选正向，也必须重新走 PI1-PI5。
