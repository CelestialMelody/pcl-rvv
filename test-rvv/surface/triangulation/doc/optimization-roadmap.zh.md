# triangulation Optimization Roadmap

## 当前边界

当前 topic 来自 surface 函数评估队列的 `on_nurbs/triangulation.cpp` 建议项。首阶段只做未裁剪 NURBS surface 的规则参数网格写入和完整 surface 采样诊断，不修改 production 源码。

2026-08-20 恢复审计时，用户补充“由于没有支持 on_nurbs，与该依赖有关的可以忽略”。因此本 roadmap 不再把真实 OpenNURBS / on_nurbs direct probe（真实依赖链路直连探针）作为当前默认恢复动作；只保留不依赖该符号链、且能形成 RVV（RISC-V Vector，可变长度向量扩展）证据闭环的候选。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `param_grid_rvv_store` | 当前源码 `createVertices` 规则二维网格 | `convertSurface2PolygonMesh`、`convertSurface2Vertices` 的参数点写入 | 减少规则乘加与 AoS stride 写入成本 | `Evaluate` 可能主导总耗时 | correctness、asm、board full-path bench、Evidence Doctor | attempted / no-production current candidate | `000-current-state-and-grid-sampling-diagnostic` |
| `index_reserve_scalar` | 当前源码 `createIndices` 每三角形小 vector push | polygons 输出构造 | 减少分配开销，作为 RVV 成本隔离 | 非 RVV 候选，不能支撑 production RVV | correctness、bench 消融 | rejected as RVV next phase | 不作为当前 RVV phase 推进。 |
| `evaluate_batch_deferred` | `Evaluate` 可能主导 | OpenNURBS surface evaluation | 如果能批处理可能更接近主成本 | 外部库语义和维护边界高风险 | profile、OpenNURBS 语义审计、独立授权 | turn_stop_deferred by prompt override | 依赖 on_nurbs/OpenNURBS，当前忽略。 |
| `trimmed_surface_followup` | `convertTrimmedSurface2PolygonMesh` 也在目标文件 | trim + inverseMapping + surface Evaluate | 可能覆盖更复杂公开入口 | inverse mapping 稀释且语义风险更高 | 先完成未裁剪路径，再做 fixture 和 bench | turn_stop_deferred by prompt override | 依赖 on_nurbs/OpenNURBS，当前忽略。 |
| `curve_sampling_followup` | `convertCurve2PointCloud` 两个 overload | curve element sampling | 规则一维采样可能有局部机会 | curve Evaluate 主导，输出 RGB 写入简单 | 单独 correctness / bench | turn_stop_deferred by prompt override | 依赖 on_nurbs/OpenNURBS，当前忽略。 |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| `000` | board repeated recovery | 板卡恢复后已完成 `tri_param_grid_512` / `tri_surface_eval_256` 5-run；两个 repeated 都有 3/5 退化和 Evidence Doctor Error。 | 无需继续同边界复跑；维持 no-production。 | done |
| `000` | real OpenNURBS direct probe | 当前 RISC-V 安装库缺 on_nurbs / OpenNURBS 符号，测试只能用 Evaluate-like sink；用户已说明依赖相关方向可忽略。 | 当前不作为恢复动作。只有用户以后重新授权 on_nurbs 依赖链路时再建新 phase。 | turn_stop_deferred |
| `000` | non-dependency RVV follow-up audit | 恢复扫描确认剩余不依赖 OpenNURBS 的方向只剩 `createIndices` 输出构造。该路径是 `std::vector<pcl::Vertices>` 的小对象分配 / push，不是连续数值热点，也没有可维护的 RVV store 边界。 | 不建议另建 RVV phase；若以后要做，仅作为标量分配消融，不作为 RVV 收益候选。 | rejected |

## 当前默认恢复队列

| action | status | reason |
| --- | --- | --- |
| 继续 `param_grid_rvv_store` 同边界复跑 | rejected with evidence | 5-run board repeated 已出现 3/5 退化，Evidence Doctor 为 `Errors=1`；复跑预算已足够支持 negative / unstable diagnostic（负向 / 不稳定诊断）结论。 |
| 真实 OpenNURBS / on_nurbs direct probe | turn_stop_deferred with prompt override | 用户已说明依赖相关方向可忽略，当前不消耗本 topic 预算。 |
| `createIndices` / `index_reserve_scalar` 消融 | rejected as RVV next phase | 只可能减少小 vector 分配，不能形成 RVV 证据链，也不能支持 production RVV 决策。 |
| trimmed surface / curve sampling follow-up | turn_stop_deferred with prompt override | 依赖 on_nurbs / OpenNURBS 语义和符号链，当前忽略。 |
| topic-local 结构 / 文档对齐 | adopted / no action | 当前已有 `src/`、`include/`、phase index、README、testing overview、correctness、benchmark/evidence、optimization evidence、code map 和 evaluation；helper 规模未超过拆分阈值。 |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| `evaluate_batch_deferred` | 当前阶段不能改 OpenNURBS 内部语义，也没有 profile 证明它是唯一瓶颈。 | full-path bench 显示 `Evaluate` 主导且参数网格 RVV 被稀释；用户授权深入 OpenNURBS / production。 |
| `param_grid_rvv_store` | repeated board 显示参数网格局部 median `0.978x`，full-path median `0.999x`，两者均 3/5 退化。 | 只有真实 OpenNURBS direct bench 显示参数网格写入仍是可见主成本，且用户授权重新开 production-shaped probe。 |
| `trimmed_surface_followup` | 入口混合 curve inverse mapping，首阶段容易把多个成本混成一个结论。 | 未裁剪路径 evidence chain 闭合后再做 trim fixture。 |
| `curve_sampling_followup` | 不属于 surface 网格主路径。 | surface 主路径 closeout 后仍有当前 topic 授权且未阻塞。 |
| `index_reserve_scalar` | 非 RVV 候选；当前源码输出为 `std::vector<pcl::Vertices>` 小对象构造，不适合用 RVV 连续 store 直接替换。 | 只有用户明确要求做非 RVV 标量分配消融时恢复。 |
