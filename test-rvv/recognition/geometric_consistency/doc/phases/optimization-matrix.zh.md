# Optimization Matrix

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `pairwise-consistency-rvv` | correspondence-pair | `PointXYZ / float / AoS` production gate | fixed consensus set + candidate j predicate | scalar reference vs RVV candidate | `bench_gc` batch predicate + upstream correctness | phase 000 board positive + phase 010 diagnostic positive | `vluxei32` / `vfsqrt` / `vfsub` / `vfabs` | diagnostic board doctor clean | adopted | keep as current narrow production helper |
| `cluster-growth-rvv` | correspondence-pair | `PointXYZ / float / AoS` production gate | sorted correspondences + taken_corresps growth loop | planned cluster-growth reference vs candidate | future production-direct growth bench | phase 010 diagnostic positive | not yet production-boarded | not yet production-boarded | deferred | new production-direct growth phase |
| `sort+RANSAC boundary audit` | correspondence-pair | production generic template | outer sort / rejector / output order | not yet covered | not yet covered | not run | not run | not run | deferred | only after growth phase closes |
