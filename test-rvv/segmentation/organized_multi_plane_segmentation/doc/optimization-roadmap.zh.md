# organized_multi_plane_segmentation 优化路线图

## 当前默认恢复动作

`next_phase_default`: `no-production diagnostic closeout`。当前 topic 没有授权范围内、未阻塞且高优先级的下一步 production 候选。若未来要重开，需要先提供新的 profile、输入形态或实现族证据，证明 Phase 010 的 per-region gather / 临时 cloud 成本可以被避开。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `plane_d_dot_rvv` | retained-candidate rescreen；当前源码 `plane_d` loop | ordered dense `PointXYZ + Normal` | 用 RVV FMA 覆盖每点点积预处理 | 板卡 5/5 退化 | Phase 000 board / doctor | rejected | none |
| `boundary_gather_rvv` | 当前源码 boundary copy loop | boundary indices -> `PointXYZ` cloud | 边界点多时减少复制成本 | component 近阈值且长尾；production-shaped gather-only 退化 | Phase 000 + Phase 010 board / doctor | rejected for production-shaped path | none |
| `viewpoint_projection_rvv` | 当前源码 `projectToPlaneFromViewpoint` | ordered boundary cloud | 覆盖投影中的 dot/div/FMA 公式 | 局部 positive 不能穿透 region output 边界 | Phase 010 `region_projected` | rejected for current production-shaped path | none |
| alternate no-temporary-cloud region assembly | Phase 010 反思 | production boundary output | 避免 per-region `PointCloud` 临时对象和 gather 写回 | 需要改 production 数据结构或 `PlanarRegion` 构造边界，维护风险高 | profile + PI1 design + production direct tests | deferred, requires new evidence and user authorization | future only |
| organized connected component comparator staging | retained-candidate companion idea | CCL comparator / label remap | 若 comparator predicate 占比高，可能另开 companion topic | 当前 topic 未证明 CCL 内热点 | public profile showing comparator/label remap hotspot | deferred, separate topic | future only |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| `plane_d_dot_rvv` | Phase 000 板卡 median 0.81x，5/5 退化 | 新输入规模或 point/normal layout profile 证明当前 bench 不是代表性边界 |
| `region_boundary_projection_rvv` | Phase 010 `region_projected` median 0.89x，5/5 退化 | 出现避免 per-region 临时 cloud / gather 写回的新实现族，并先完成 PI1 范围审计 |
| `region_boundary_gather_only_rvv` | Phase 010 `region_gather_only` median 0.80x，5/5 退化 | 真实 profile 证明 current synthetic boundary row source 不代表 production |
| `refine_label_growth_rvv` | 双向 pass 会改写 labels、label_indices 和 inlier_indices，状态依赖强 | public-shaped profile 证明 refine comparator 或 label update 是热点 |
| CCL label pass | 属于 `organized_connected_component_segmentation` companion 主题，union-find / comparator 虚调用主导 | 当前 topic 或真实 organized segmentation profile 证明 comparator staging 明显可见 |
| region covariance / `eigen33` | per-region 操作，调用次数由 label 数决定，不是本阶段逐点主扫描 | profile 证明 large region fitting 成为主热点 |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| Phase 000 | 只让 `projection` 升级到 production-shaped diagnostic | component board positive，且可与 boundary gather 组合验证 | Phase 010 board / doctor | completed |
| Phase 010 | no-temporary-cloud / direct contour fill 实现族 | `region_projected` 与 `region_gather_only` 都退化，说明临时输出组织可能主导 | 真实 public profile、PI1 设计、production direct correctness / board | low; requires new evidence |
