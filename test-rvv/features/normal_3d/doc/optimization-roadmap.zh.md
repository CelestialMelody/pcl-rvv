# normal_3d Optimization Roadmap

## 当前默认恢复动作

Phase 000 已闭合。当前默认状态为 `turn_stop_deferred with stop_condition_hit`：不建议继续修改
`features/include/pcl/features/impl/normal_3d.hpp`。恢复条件是新的真实 workload / PCD board evidence、
profile 指向 `normal_3d.hpp` local flip/output loop，common covariance helper 改动后的回归需求，
或用户明确授权 bounded production probe。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| reuse common dense indexed covariance RVV | current source shape | dense indexed neighborhoods in normal estimation | Avoids duplicate normal_3d RVV helper; reuses existing common covariance kernel. | Search and Eigen solver dilute public runtime. | done: correctness, asm attribution, 5-run board component/public bench, Evidence Doctor. | attempted / reused-positive-component | closed in Phase 000 |
| normal_3d-local flip/output vectorization | queue screening | output fill / flip loop | Could help only if local output loop becomes measurable. | Single-point branchy logic; Phase 000 public median only `1.02x`, no local bottleneck evidence. | profile or dedicated ablation showing flip/output loop is hot, then production-direct tests and fallback matrix. | rejected with current evidence | no next phase unless resume condition is met |
| point type expansion | workflow scope rule | PointXYZRGB / PointNormal-like / custom xyz-like layouts | Broadens coverage if common RVV gate applies safely. | This is a common covariance coverage question, not a `normal_3d.hpp` local patch question. | point-type correctness, asm, board, Evidence Doctor under the owning common/helper topic. | not_applicable for this closeout | reopen only with common helper change or explicit user scope |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| Phase 000 | real PCD / workload public-path audit | synthetic public path weak-positive may not represent external benchmark data. | available PCD dataset, public benchmark compare, repeated board, Doctor. | low; only if user or workload requires normal_3d public-path proof |
| Phase 000 | search / solve profile split | component covariance positive but public path diluted. | profile or timer split proving search, solve, flip/output proportions. | low; not needed for current no-production decision |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| OMP normal_3d RVV integration | OMP/RVV nesting and scheduling confound performance attribution. | Scalar normal topic has stable production evidence and OMP-specific benchmark budget. |
