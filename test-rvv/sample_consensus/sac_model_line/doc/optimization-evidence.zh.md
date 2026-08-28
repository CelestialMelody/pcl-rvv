# sac_model_line 优化证据

## Candidate 状态

| candidate family | 状态 | 证据 | 结论边界 |
| --- | --- | --- | --- |
| `count-indexed-gather-f32m2` | adopted production behavior | QEMU correctness pass；production asm 命中 RVV indexed load / FMA / mask popcount；Phase 060 接入后 public board median `4.8068x`；production doctor 0/0/0。 | 只覆盖 `PointXYZ` 风格 direct indexed count 和当前 production gate。 |
| `select-vcompress-scratch-scalar-store` | historical adopted baseline, replaced | Phase 050 接入后 public board median `3.1014x`；production doctor 0/0/0。 | 已被 Phase 060 compressed double store 替换，只作为历史 production baseline。 |
| `select-vcompress-vse64-error-store` | adopted production behavior | QEMU correctness pass；production asm 命中 RVV indexed load / FMA / mask / `vcompress.vm` / inlier `vse32` / `vfwcvt` / `vse64`；Phase 060 接入后 public board median `3.2460x`；production doctor 0/0/0。 | 只覆盖 `PointXYZ` 风格 direct indexed select 和当前 production gate。 |
| `getDistances-sqr-rvv-scalar-sqrt-store` | rejected for current diagnostic boundary | QEMU correctness pass；asm 命中 `vluxseg3ei32.v`、`vfmacc.vv` 和 `vse32.v`，没有 `vfsqrt`；board repeated median `0.9340x`；doctor Errors=1 / Warnings=0 / Suggestions=0。 | 只拒绝 RVV 平方距离后回到标量 sqrt / dense store 的当前测试 helper 形状。 |
| `getDistances-vfsqrt-scratch-scalar-store` | historical adopted baseline, replaced | Phase 040 QEMU correctness pass；production asm 命中 RVV indexed load / FMA / `vfsqrt.v` / scratch `vse32`；接入后 public board median `3.4036x`；production doctor 0/0/0。 | 已被 Phase 050 direct double store 替换，只作为历史 production baseline。 |
| `getDistances-vfsqrt-vse64-store` | adopted production behavior | QEMU correctness pass；production asm 命中 RVV indexed load / FMA / `vfsqrt.v` / `vfwcvt` / `vse64`；Phase 060 接入后 public board median `4.0460x`，Phase 050 getDistances-specific baseline median `4.2237x`；production doctor 0/0/0。 | 只覆盖 `PointXYZ` 风格 direct indexed getDistances 和当前 production gate；Phase 060 未修改 getDistances，批次波动不作为退化结论。 |
| `identity-index-strided-load` | rejected with evidence | Phase 070 strict RVV-vs-RVV A/B 中 identity 输入只有 count/getDistances 弱正向，select median `0.9967x` 且 4/5 退化；shuffled 控制组 getDistances median `0.9959x` 且 3/5 退化。identity doctor 为 Errors=1 / Warnings=1 / Suggestions=2；shuffled doctor 为 Errors=3 / Warnings=0 / Suggestions=2。 | 只拒绝当前同边界 identity-strided load family；production 已回退到 Phase 060 gather-only load family。 |
| `point-type-expansion` | turn_stop_deferred with stop_condition_hit | 当前 helper 只验证 `PointXYZ`。 | `PointXYZI`、RGB/RGBA、normal 和自定义点型需在 production scope 或独立 diagnostic scope 中授权后再做。 |

## ILP / LMUL 取舍

当前 count、select 和 getDistances 候选使用 `f32m2` indexed gather。LMUL m2 与已有 PCL RVV 点加载 helper 兼容，能在一个 VL chunk 内保留 xyz、叉乘中间值、squared distance、mask、select 压缩写回暂存和 getDistances vfsqrt 暂存。阶段证据没有比较 m1/m4，也没有做 unroll（循环展开）或 ILP（指令级并行）消融；这些不是当前 adopted production boundary 的未完成项，但属于后续生产实现形态可复核点。

## 数值预算

标量公开入口使用 Eigen float vector 计算 squared distance，再与 double threshold squared 比较，`getDistancesToModel` 还会写 double distance。RVV candidate 使用 float 中间量并保留严格 `< threshold^2` 判断；Phase 030 在 RVV 内执行 `vfsqrt` 后再扩成 double 写回。Phase 050 把 `getDistancesToModelRVV` 的 dense double 写回改成 `vfwcvt + vse64`，Phase 060 又把 `selectWithinDistanceRVV` 的 compressed error double store 改成 `vfwcvt + vse64`，去掉对应 scratch 和标量 lane loop。`src/test_sac_model_line.cpp` 覆盖阈值附近样本、非单位 direction 归一化、select 输出清空、inlier 顺序、`error_sqr_dists_` 和 dense distance output 误差预算。Phase 060 已补齐 production direct 证据；若后续扩大边界，仍需新增阈值极近、空 indices 和非 `PointXYZ` 点型等 dedicated case（专门用例）。

## Production 接入状态

Phase 040 已补齐 dispatch / fallback / point type gate、production direct test、production asm attribution 和 board production bench。Phase 050 在同一 production boundary 内替换 `getDistancesToModelRVV` 写回形态并重跑接入后板卡证据；Phase 060 又替换 `selectWithinDistanceRVV` 的压缩平方误差写回形态并重跑接入后板卡证据。当前证据支持当前窄范围接入，并已按本轮采纳条件写成 adopted production behavior（已采纳生产行为）。Phase 020 的 scalar-sqrt 退化只拒绝该测试 helper 形状，不能推出 `getDistancesToModel` 整体不适合 RVV。Phase 070 的 identity-index strided load 属于 RVV-family-selection（RVV 实现族选择）问题；同边界 A/B 后不满足采纳条件，当前 production 保持 gather-only load family。
