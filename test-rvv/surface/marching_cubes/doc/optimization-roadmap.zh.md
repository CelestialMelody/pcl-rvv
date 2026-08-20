# marching_cubes optimization roadmap

## 当前边界

当前 topic 已完成 helper-level 诊断、production boundary audit、narrow `PointNormal` production adoption 和 generic point type expansion。`surface/include/pcl/surface/impl/marching_cubes.hpp` 在 `__RVV10__` 下对满足 `pcl::rvv::RVVXYZAoSFloatLayout<PointNT>` 的点型采用 RVV active-cell prepass，再回到标量 `createSurface()` 输出三角形；不满足 gate 的 `PointNT` 回退标量。长期 production 文档主路径为 `doc-rvv/surface/marching_cubes-RVV.zh.md`。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| edge-interpolation-rvv | 当前 `createSurface` edge interpolation | active cell vertex emission | 批量计算 12 条 edge 的 xyz 插值 | Board repeated 为 `0.992x/1.024x/1.049x` 且 doctor 有退化 Error，说明小定长 work 被 staging 和输出成本稀释 | correctness、asm、board、Evidence Doctor | attempted / not production candidate | 暂停 |
| cube-index-prepass | voxel scan 8-neighbor compare | dense grid cell scan | 提前过滤 inactive / NaN cell | 8 邻域重叠和 active list staging 成本已在 helper 边界证实可接受，但仍未覆盖 public path | scan diagnostic、active ratio bench、board | strong-positive / diagnostic complete | Phase 020 |
| structure-parity-doc-suite | topic-local docs and code map | README / testing-overview / benchmark docs | 让恢复者和 reviewer 能按目录找证据 | 已补齐主读者路径；后续只需随 phase 刷新 | doc-suite files | done | Phase 030 |
| production-active-cell-prepass | `performReconstruction()` public path | historical `PointNormal` synthetic public entry | 跳过 inactive cell 的标量 `getNeighborList1D()` / `createSurface()` 调用 | 只证明 synthetic voxelized grid，不证明 Hoppe/RBF voxelization 真实分布 | production direct tests、asm、board、doctor | adopted / historical narrow anchor | Phase 050 泛型扩展已替代 exact gate |
| generic point type expansion | `performReconstruction()` public template | `PointNT` with xyz float AoS traits | 把 active-cell prepass 从 exact `PointNormal` 扩到更常见 xyz 点型 | 输出构造仍只写 xyz；真实 Hoppe/RBF 输入分布未覆盖 | traits audit、fallback tests、production direct bench、asm、board | adopted / generic production | done |
| active-z finite-collapse single-buffer | Phase 050 closeout reflection | RVV active-cell prepass inner tail | 减少 `finite_flags` 落内存和标量尾段 flag 读 | Phase 060 RVV-vs-RVV median 只有 `1.007x`，doctor near-threshold；收益被 `createSurface()` 输出主导 | QEMU correctness、asm、same-boundary RVV-vs-RVV board A/B、doctor | attempted / neutral / not adopted | 暂停 |
| active-z table lookup / vcompress | Phase 060 closeout reflection | RVV active-cell prepass inner tail | 进一步把 `edgeTable` active 判断或 active z 输出压缩到 RVV 侧 | 比 finite-collapse 更复杂；没有 profile 证明 tail 是瓶颈前不值得继续 | profile 或同边界 component ablation、QEMU correctness、asm、RVV-vs-RVV board A/B、doctor | deferred with resume condition | no default phase |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| Phase 000 | cube-index-prepass | Edge interpolation 只得到 neutral-to-weak-positive，且 sphere case 稳定低于 1 | correctness、asm、board、doctor | high |
| Phase 010 | structure-parity-doc-suite + production-boundary-audit | prepass 已强正向，下一步要把恢复入口和 production 边界补齐 | doc suite、boundary scan、mismatch audit | high |
| Phase 020 | PI1 production integration plan | doc suite 和 public boundary 已闭合，但生产 patch 需要用户检查点 | PI1 plan、fallback gate、point type scope | high |
| Phase 030 | production stabilization + point type expansion audit | 用户已确认保留 narrow production patch；Evidence Doctor 还有 low_run_count warning，exact gate 也需要 fallback 和 traits 扩展审计 | 5-run board、non-PointNormal fallback correctness、generic traits gate plan | high |
| Phase 050 | generic point type expansion audit | 代码阅读显示 `RVVXYZAoSFloatLayout` 可能足够表达输入前提，但输出语义和代表点型证据仍未闭合 | representative point-type correctness、board recovery、traits / output audit | high |
| Phase 050 | active-z table lookup / compression A/B | generic repeated 已把接入证据闭合，剩余可优化点集中在 active-z 预扫描内部的 table lookup 和标量尾段 | candidate implementation、QEMU correctness、asm、board A/B | medium |
| Phase 060 | full vector table lookup / `vcompress` | finite-collapse 减少一组 store 但 RVV-vs-RVV 只有 neutral，说明尾段收益很容易被输出成本吞掉；只有 profile 指向该尾段时再试更复杂 code shape | profile / component ablation、same-boundary RVV-vs-RVV A/B | low / resume-gated |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| edge interpolation production | helper 诊断没有稳定收益，且 production 已采用收益更强的 active-cell prepass | 只有在后续 profile 证明 active-cell tail 仍被 edge interpolation 主导时恢复 |
| active-z finite-collapse single-buffer | Phase 060 correctness 和 asm 都成立，但 `mc_prod_xyz_64` RVV-vs-RVV 3-run median 仅 `1.007x`，Evidence Doctor 有 low-run 和 near-threshold 提示；不足以改变 production truth | 只有在更高 run count、profile 或不同输入分布证明 active-z staging store 是稳定瓶颈时恢复 |
