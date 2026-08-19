# Optimization Evidence

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| finite-cache scan | ordered-cloud-pair | PointXYZ / float / organized | `generateMeshCurrentBuild` | `run_test_compare` pass | board `run_board_bench_compare` | positive single-run | vector loads in RVV candidate | Errors=0, low_run_count warnings | partial-production-candidate | 用户确认后进入 PI1 |
| adaptive diagonal preference | ordered-cloud-pair | PointXYZ / float / organized | `generateMeshCurrentBuild` | `run_test_compare` pass | board `run_board_bench_compare` | positive single-run, adaptive 1.45x | vector loads/sub/fabs/store in RVV candidate | Errors=0, low_run_count warnings | partial-production-candidate | 用户确认后进入 PI1 |
| production-output-shape | ordered-cloud-pair | PointXYZ / float / organized | `OrganizedFastMesh::reconstruct` public overload | `run_test_compare` pass | board `board_smoke` | public adaptive 0.89x, quad/right/left near 1.0x | public RVV asm dump exists | Errors=3, Warnings=8, Suggestions=5 | rejected / production patch reverted | topic no-production closeout |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 020-production-output-shape-rerun | output-shape parity | 标量预分配写回比 `push_back` 更接近 production hotspot 行为 | board repeated summary, doctor, asm attribution | medium |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| production-output-shape | Phase 020 仍未在 QEMU smoke 上转正，且已有 production public board 负向证据 | 若未来重新开启 topic，先获取新的 board profile；当前 topic 已 no-production 收口 |
