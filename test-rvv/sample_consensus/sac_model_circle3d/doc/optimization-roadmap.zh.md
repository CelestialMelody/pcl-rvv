# sac_model_circle3d Optimization Roadmap

## 当前边界

当前 topic 覆盖 `sac_model_circle3d.hpp` 的 3D 圆投影核。Phase 000 完成 count/select component ablation（组件消融）：count 负向，select 在 test-only（仅测试使用）边界正向。Phase 010 把 select 候选接入 production（生产源码）后，post-narrowing production-public（公开入口标量/RVV）10-run board 证据转为负向 / 不稳定，不能支撑采纳；当前生产补丁已回滚，topic 进入 rollback/no-production closeout。

当前默认状态：`selectWithinDistance` 生产 RVV 补丁已回滚；`countWithinDistance` 当前 projection family 不接入；`PointXYZI/RGB/RGBA` 扩展已拒绝；`getDistancesToModel` 暂缓到独立语义审计，不应混入当前未采纳的 select patch。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| projection count RVV candidate | retained-candidate rescreen；Phase 000 实测 | direct indexed `indices_`，`PointXYZ`，float xyz AoS | 期望用向量投影和 `vcpop` 减少逐点 Eigen 对象成本。 | 实测显示缺少 select 的压缩写回收益后，向量公式开销超过 public baseline。 | Phase 000 correctness、asm、5-run board、Evidence Doctor。 | rejected with evidence | 无；新 count family 需另写 plan。 |
| projection select RVV candidate | retained-candidate rescreen；Phase 000/010/020 实测 | `selectWithinDistance` public path | test-only 边界中 `vcompress` 同时写 inliers 和 error_sqr_dists 曾经正向。 | 接入 production 后 `PointXYZ` 10-run mean 0.9748、median 0.9991、5/10 退化；点类型扩展也负向。 | QEMU correctness、production asm、10-run board、Evidence Doctor。 | rollback/no-production | 生产补丁已回滚；当前 topic 不再保留同边界 production 方向。 |
| full-RVV getDistances sqrt/double store | circle2d / line / stick 的 `vfsqrt + vfwcvt + vse64` 经验 | `getDistancesToModel` dense output | 可能消除 dense double store tail。 | `getDistancesToModel` 公式符号需先审计；当前 select production 负向说明不能从相邻 topic 直接外推收益。 | 独立 same-chain correctness、board repeated、asm。 | deferred / separate-entry-audit | 仅在用户明确希望继续同 topic 新入口时另写 phase。 |
| count-specific formula / reduction candidate | Phase 000 负向反思 | `countWithinDistance` | 探索是否能避免 sqrt 或减少投影中间量。 | 会改变数值近似和阈值边界，维护成本高；当前生产探针已负向。 | 新 phase plan、数值一致性、asm、board、doctor。 | rejected for current loop / possible future research | 不建议作为当前连续推进方向。 |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| projection select RVV candidate | production-public board repeated 已负向，且补丁已回滚。 | 新的实现族或新入口必须另开 topic / phase plan。 |
| identity-index strided load | circle2d / line 的同边界 A/B 已经负向或不稳；circle3d 首阶段不把它作为主假设。 | Phase 000 gather 候选正向且 profile 显示 gather 是主要剩余成本。 |
| 合并 count/select projection production patch | Phase 000 显示 count 5/5 稳定退化，不能被 select 正向掩盖。 | 仅当 count-specific 候选在同边界 repeated board 转为正向后重新考虑；当前 topic 不再保留此方向。 |
| traits-gated 点类型扩展 | Phase 020 中 `PointXYZI` mean 0.9117、`PointXYZRGB` mean 0.8883、`PointXYZRGBA` mean 0.8828，三个 doctor 均有 Error。 | 需要新的点类型专用实现形态和独立 plan；不能复用当前 production helper。 |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| Phase 000 | select-only production probe | select candidate 在有 warm-up 的 5-run board 中 5/5 正向，且 asm 可归属到 `vcompress` 路径。 | 用户授权后补 production dispatch、fallback tests、production direct board repeated、Evidence Doctor 和 PI5 检查点。 | high，但当前停在用户授权。 |
| Phase 010 | select production probe refresh | post-narrowing `PointXYZ` 10-run 出现 5/10 退化，Evidence Doctor 有 Error。 | 用户决定回滚或明确授权另做受控试验。 | stop；当前不建议采纳。 |
| Phase 020 | select point-type expansion | production dispatch 的 traits-gated 泛型扩展在三种常见点型上均负向。 | 已完成三点型 board / doctor；保留 fallback 测试。 | closed / rejected. |
| Phase 030 | rollback/no-production closeout | production 补丁已回滚，当前 topic 不再保留同边界生产方向。 | 无；只需保留历史证据和 topic-local closeout。 | done. |
| Phase 020 | getDistances lambda sign audit | 源码中 `getDistancesToModel` 的 lambda 符号与 count/select 不同，不能直接复用 select 结论。 | 标量语义审计、same-chain reference、必要时独立 RVV plan。 | low；当前 select production patch 未采纳前不作为默认连续推进项。 |
