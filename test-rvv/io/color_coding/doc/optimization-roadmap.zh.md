# color_coding Optimization Roadmap

## 当前边界

当前 topic 只处理 `io/include/pcl/compression/color_coding.h` 的 color component（颜色组件）诊断。production 长期文档暂不适用；完整 octree compression public entry（公开压缩入口）需要后续生产形态证据。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| scalar same-chain reference | current production source | encode average, encode diff, decode, default color | 建立正确性基线 | reference 可能漏掉 signed char / bit reduction 细节 | gtest same-chain against hand-derived expected values | adopted | phase 000 completed |
| RVV indexed encode average | phase 010 board summary | indexed leaf color gather | leaf 31/257/1024/4096 均正向 | Doctor warning 集中在 group_outlier / long tail，不能 group-wide 外推 | production-shaped diagnostic, trace / asm, board repeated, Doctor | positive component diagnostic, separate reporting required | phase 020 |
| RVV indexed encode points | phase 010 board summary | indexed leaf color gather + scalar diff push | 减少平均颜色求和成本，整体 encode points 仍有 1.13x-1.25x median | diff stream `push_back` 仍是标量，production caller 可能被其它成本主导 | production-shaped diagnostic, board repeated, Doctor | positive component diagnostic | phase 020 |
| RVV contiguous decode/default | phase 010 board summary | contiguous output range | decode 稳定到 1.08x-1.12x，default 约 1.23x-1.25x | decode 收益较弱，必须与 default 分开批准 | production-shaped diagnostic, board repeated, Doctor | weak-positive / positive component diagnostic | phase 020 |
| production-shaped color coder probe | phase 020 board summary | `pcl::PointXYZRGBA` / color-coder call shape | encode average / encode points / default 保持正向；decode large case 暴露风险 | public compression entry 仍可能由 octree traversal / entropy coder 主导；decode large case Error | production-shaped diagnostic and component-to-production mismatch audit | split: encode/default partial-production-candidate, decode deferred | phase 030 PI1 plan |
| PI1 encode/default production integration plan | phase 020 partial candidate | `encodeAverageOfPoints`, `encodePoints` average pass, `setDefaultColor` | 把正向生产形态候选冻结为生产补丁前范围 | 需要用户确认才能修改 production；需要 fallback / traits / size gate | production direct correctness, fallback tests, asm, board, Doctor | planned / user checkpoint | phase 030 |
| doc-suite parity essentials | doc-suite quality bar | README / testing / benchmark / code map role docs | 降低 reviewer 恢复成本，避免证据散落 | 只改 topic-local docs，不应扩大 production 范围 | role inventory, artifact tracking, path-scoped status | adopted in phase 040 | completed |
| decode staged-store implementation-shape audit | phase 020 decode Error | `decodePoints` production-shaped helper | 隔离 direct `vsse32` AoS store 是否是主要风险 | scratch resize + scalar AoS writeback 可能更慢；仍不是 production direct | staged labels, repeated board, Doctor | rejected with evidence in phase 050 | completed |
| PI2 production public probe | phase 060 production patch | exact `pcl::PointXYZRGBA` public `ColorCoding` methods | 验证真实 production dispatch 是否值得保留 | pre-production positive 可能被 public method 边界吞掉 | production direct tests, fallback tests, asm, board repeated, Doctor | completed / PI5 checkpoint | phase 070 partial rollback |
| PI5 partial rollback default-only | phase 070 user-authorized rollback | exact `pcl::PointXYZRGBA` public `setDefaultColor` | 验证只保留 default RVV 是否仍有收益 | helper 太短，收益容易被测量波动反转 | production direct tests, default-only board repeated, Doctor, registry | completed / no adoption recommended | phase 080 full rollback |
| full rollback no-adoption closeout | phase 080 user-requested recommendation | all `ColorCoding` public methods | 移除剩余 production RVV 分流并整理 no-adoption 状态 | 若继续需要新 profile / 新实现族 | run_test_compare, production-filter smoke, freshness, diff check | completed / stop condition hit | none |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| phase 000 | leaf-size sweep + longer timing boundary | Board summary positive for encode/default but decode has Evidence Doctor Error and average leaf timing is too small | per-size board repeated summary, Evidence Doctor, asm attribution | high |
| phase 010 | production-shaped color coder precheck | Component evidence has no Doctor Error; encode/default/decode all have usable positive signal inside diagnostic boundary | real PCL point type smoke, production-shaped bench, repeated board, Evidence Doctor, mismatch audit | high |
| phase 010 | doc-suite essentials | Topic now has multi-phase board / Doctor evidence but still lacks testing / benchmark / code-map standalone role docs | doc-suite role inventory and artifact tracking | high |
| phase 020 | split encode/default from decode | `ps_decode_points_leaf4096` has Doctor Error while encode/default remain positive | PI1 plan excluding decode; optional decode implementation-shape audit | high |
| phase 040 | doc-suite parity closeout | README、testing overview、correctness、benchmark/evidence、optimization evidence 和 code map 已拆出 | artifact tracking, freshness check, Handoff update | completed |
| phase 050 | reject staged-store decode shape | staged-store decode triggers two Doctor Errors and does not improve direct decode instability | no further staged-store probe unless new profile/root-cause evidence appears | completed |
| phase 060 | production public evidence splits candidates | `encodeAverageOfPoints` production public 负向，`encodePoints` 弱正，`setDefaultColor` 当时同批 positive | 已由 phase 070 部分回滚和 default-only 复跑取代；phase 060 保留为 historical evidence | superseded |
| phase 070 | default-only evidence collapses to neutral / unstable | 部分回滚后 `prod_set_default_color_4096` median 1.0035x、min 0.9945x，并触发 Doctor Error | phase 080 已完整回滚；除非有新 profile / 新实现族，否则不继续当前 topic | superseded by phase 080 |
| phase 080 | full rollback completes no-adoption closeout | production 源码已无 RVV dispatch，Std/RVV correctness 仍通过 | 当前没有建议继续推进的 RVV candidate；未来只在新 profile / 新实现族证据下重开 | completed |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| full octree compression public entry | 本阶段只隔离 color coder，不含 point coder、octree traversal、entropy coder | color coder component positive and production-shaped probe plan approved |
| production patch | component evidence 尚未建立 | PI1 plan 能冻结 fallback / dispatch / point type / evidence boundary |
| decode production candidate | phase 010 only proves weak-positive / positive component diagnostic; no production caller evidence | production-shaped decode precheck remains positive and no Doctor Error |
| direct decode production candidate | phase 050 direct `ps_decode_points_leaf4096` is only weak / unstable Warning with min 0.9116x | new decode family or profile explains the instability and passes production-shaped board without Doctor Error |
| staged-store decode candidate | phase 050 `ps_decode_points_staged_leaf257` and `ps_decode_points_staged_leaf4096` trigger Doctor Errors | do not resume without new root-cause evidence that changes the staged-store cost model |
| full octree compression public entry | 仍可能被 traversal、point coder 或 entropy coder 主导 | production-shaped color coder probe shows value and PI1 can freeze public-entry evidence plan |
| production encode average RVV | phase 060 production public evidence negative；`leaf257` / `leaf4096` both trigger Doctor Error | only resume with a new implementation family or profile that changes the public-boundary cost model |
| production encode points average-pass RVV | phase 060 production public evidence weak / near-threshold with warning | only resume with lighter implementation or stronger repeated evidence |
| production default color RVV | phase 070 default-only production public evidence neutral / unstable；median 1.0035x, min 0.9945x, Doctor Error 2/5 below 1 | only resume with new profile, larger real workload evidence, or a different implementation family that changes the public-boundary cost model |

## roadmap_default_recovery_queue

| priority | action | status | resume condition |
| --- | --- | --- | --- |
| 1 | no next phase | turn_stop_deferred with stop_condition_hit | Phase 080 已完整回滚；当前没有建议继续推进的 RVV candidate。 |
| 2 | decode implementation-shape audit | rejected with evidence | phase 050 已尝试 staged-store shape；除非有新的 profile 或实现族，不重复该路线。 |
| 3 | full octree compression public entry | blocked / not recommended now | 当前 color coder 没有 adopted production behavior；除非用户明确要求完整入口 profile，否则不继续。 |
