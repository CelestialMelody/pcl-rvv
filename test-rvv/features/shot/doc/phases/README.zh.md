# SHOT phase 索引

## 当前默认恢复入口

当前默认恢复入口是 `PI3-shape-bin-production-rollback-closeout`。用户已确认回滚 PI2 shape-bin indexed production patch（生产补丁），`features/include/pcl/features/impl/shot.hpp` 当前恢复为原标量 production path（生产路径）。PI2 证据保留为历史生产探针：production-detail shape-bin helper 约 1.07x，但 public SHOT352 / SHOT1344 接入后为 0.98x / 0.99x，Evidence Doctor 对两个 public case 都给出退化 Error。当前状态是 `rollback/no-production`，不创建 `doc-rvv/features/shot-RVV.zh.md`。

## 阶段列表

| phase | 状态 | plan | result | 默认下一步 |
| --- | --- | --- | --- | --- |
| `000-current-state-and-diagnostic-plan` | done | `000-current-state-and-diagnostic-plan/plan.zh.md` | `000-current-state-and-diagnostic-plan/result.zh.md` | 已闭合为诊断脚手架；不支持 production 结论。 |
| `010-normalize-component-diagnostic` | done | `010-normalize-component-diagnostic/plan.zh.md` | `010-normalize-component-diagnostic/result.zh.md` | normalization component 3 次板卡 run 稳定 1.50x-1.59x；public entry 仍不稳定。 |
| `PI1-normalize-production-probe-plan` | authorization_required | not_created | not_applicable | 需要用户明确授权修改 production 后才能进入。 |
| `020-shape-bin-component-diagnostic` | done | `020-shape-bin-component-diagnostic/plan.zh.md` | `020-shape-bin-component-diagnostic/result.zh.md` | shape-bin SoA component 3 次板卡 run 稳定 2.38x-2.47x；不能直接外推到 production AoS / indices。 |
| `030-shape-bin-aos-layout-diagnostic` | done | `030-shape-bin-aos-layout-diagnostic/plan.zh.md` | `030-shape-bin-aos-layout-diagnostic/result.zh.md` | `pcl::Normal` 连续 AoS shape-bin 3 次板卡 run 稳定 1.76x-1.90x；仍未覆盖任意 `indices` gather。 |
| `040-shape-bin-indexed-gather-diagnostic` | done | `040-shape-bin-indexed-gather-diagnostic/plan.zh.md` | `040-shape-bin-indexed-gather-diagnostic/result.zh.md` | indexed gather component 3 次板卡 run 稳定 1.65x-1.86x；仍不能替代 production direct。 |
| `PI1-shape-bin-indexed-production-probe-plan` | superseded_by_PI2 | `PI1-shape-bin-indexed-production-probe-plan/plan.zh.md` | not_applicable | 已冻结 shape-bin indexed production probe 的范围、fallback、production direct tests 和暂停条件；PI2 已按此范围执行，PI3 已确认回滚。 |
| `050-interpolation-geometry-staging-diagnostic` | done | `050-interpolation-geometry-staging-diagnostic/plan.zh.md` | `050-interpolation-geometry-staging-diagnostic/result.zh.md` | 修正 valid mask 重复 scalar sqrt 后仍为 0.97x；该 staging arrays 形态不建议进入 production probe。 |
| `060-color-lab-distance-component-diagnostic` | done | `060-color-lab-distance-component-diagnostic/plan.zh.md` | `060-color-lab-distance-component-diagnostic/result.zh.md` | normalized LAB distance arithmetic 3 次板卡 run 为 1.10x-1.23x；只支持下一阶段 color production-shaped diagnostic，不支持直接 production patch。 |
| `070-color-rgb-lut-indexed-diagnostic` | done | `070-color-rgb-lut-indexed-diagnostic/plan.zh.md` | `070-color-rgb-lut-indexed-diagnostic/result.zh.md` | indexed RGB/LUT scalar staging + RVV LAB distance 为 1.02x，doctor near-threshold；不建议该 staging shape 进入 production probe。 |
| `080-interpolation-bin-selection-scalar-tail-diagnostic` | done / attempted_unstable | `080-interpolation-bin-selection-scalar-tail-diagnostic/plan.zh.md` | `080-interpolation-bin-selection-scalar-tail-diagnostic/result.zh.md` | scalar-tail staging 3 次 targeted board run 为 0.84x、1.12x、1.17x；不建议作为 production probe。 |
| `090-structure-parity-doc-suite-diagnostic` | done | `090-structure-parity-doc-suite-diagnostic/plan.zh.md` | `090-structure-parity-doc-suite-diagnostic/result.zh.md` | topic-local doc suite 已补齐；registry / target alias 仍是下一结构缺口。 |
| `100-evidence-registry-target-alias-diagnostic` | done | `100-evidence-registry-target-alias-diagnostic/plan.zh.md` | `100-evidence-registry-target-alias-diagnostic/result.zh.md` | registry / target alias 已接入；继续 production 需要用户授权 PI2。 |
| `PI2-shape-bin-indexed-production-probe` | done / attempted_negative | `PI2-shape-bin-indexed-production-probe/plan.zh.md` | `PI2-shape-bin-indexed-production-probe/result.zh.md` | production-detail 为 1.07x weak-positive，但 public SHOT352 / SHOT1344 为 0.98x / 0.99x 且 Doctor public Errors=1；PI3 已确认回滚 production patch。 |
| `PI3-shape-bin-production-rollback-closeout` | done / rollback_no_production | `PI3-shape-bin-production-rollback-closeout/plan.zh.md` | `PI3-shape-bin-production-rollback-closeout/result.zh.md` | `shot.hpp` production patch 已回滚；correctness / evidence freshness 通过；当前 topic 无默认继续优化方向。 |
