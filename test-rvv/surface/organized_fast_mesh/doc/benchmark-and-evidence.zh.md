# Benchmark 与证据

本文件说明 bench 输入、计时边界、checksum 和证据等级。

## bench label

| label | 入口 | 边界 | 证明点 |
| --- | --- | --- | --- |
| `ofm_quad_valid_cache` | `generateMeshCurrentBuild` | finite cache + polygon append | 规则 quad 扫描是否受益 |
| `ofm_right_cut_valid_cache` | `generateMeshCurrentBuild` | finite cache + polygon append | fixed right-cut 路径是否受益 |
| `ofm_left_cut_valid_cache` | `generateMeshCurrentBuild` | finite cache + polygon append | fixed left-cut 路径是否受益 |
| `ofm_adaptive_cut_valid_cache` | `generateMeshCurrentBuild` | finite cache + adaptive diagonal choice | adaptive cut 主成本是否可向量化 |
| `ofm_public_quad` | `OrganizedFastMesh::reconstruct` | public overload | 生产公开入口 quad 路径是否受益 |
| `ofm_public_right_cut` | `OrganizedFastMesh::reconstruct` | public overload | 生产公开入口 right-cut 路径是否受益 |
| `ofm_public_left_cut` | `OrganizedFastMesh::reconstruct` | public overload | 生产公开入口 left-cut 路径是否受益 |
| `ofm_public_adaptive_cut` | `OrganizedFastMesh::reconstruct` | public overload | 生产公开入口 adaptive-cut 路径是否受益 |

## 证据边界

- QEMU 只用于构建、正确性和日志形状。
- 板卡 repeated summary 才能支撑性能结论。
- 当前 bench 不包含 shadow edge 检查；那是下一阶段候选。

## 当前 board 结果

### Diagnostic 历史结果

| case | std avg | rvv avg | speedup | 证据边界 |
| --- | ---: | ---: | ---: | --- |
| `ofm_quad_valid_cache` | 286.2682 ms | 264.0053 ms | 1.08x | 单次 board diagnostic |
| `ofm_right_cut_valid_cache` | 803.3517 ms | 689.3863 ms | 1.17x | 单次 board diagnostic |
| `ofm_left_cut_valid_cache` | 797.3111 ms | 682.2623 ms | 1.17x | 单次 board diagnostic |
| `ofm_adaptive_cut_valid_cache` | 849.6489 ms | 584.5341 ms | 1.45x | 单次 board diagnostic |

这组结果只证明 test-only candidate（测试专用候选）在诊断边界正向，不能替代
production public path 的最终性能结论。

### Production public 结果

当前生产直连结果位于 `log/board/analyze_bench_compare.log`：

| case | std avg | rvv avg | speedup | 证据边界 |
| --- | ---: | ---: | ---: | --- |
| `ofm_public_quad` | 327.7682 ms | 327.1801 ms | 1.00x | production public |
| `ofm_public_right_cut` | 577.6627 ms | 584.6402 ms | 0.99x | production public |
| `ofm_public_left_cut` | 575.8935 ms | 578.9270 ms | 0.99x | production public |
| `ofm_public_adaptive_cut` | 463.7781 ms | 523.0977 ms | 0.89x | production public |

Evidence Doctor（证据体检）结果位于 `log/board/evidence_doctor.md`：Errors=3，
Warnings=8，Suggestions=5。Errors 主要来自 public right/left/adaptive 的退化频率，
因此该 production 接入不支持采纳，生产补丁已回滚。

## 负向原因记录

当前负向不是 correctness 问题；QEMU 和板卡 correctness 都曾保持 checksum 一致。主要问题是
production public path 中可 RVV 化的计算比例太小：finite mask 和 adaptive z 差值可批量化，
但 cell 分支、shadow gate fallback、polygon 输出顺序和 `pcl::Vertices` 写回仍是标量主成本。

上一版 production helper 使用 `push_back` 写 polygon，而原标量实现使用预分配 `resize` + `idx`
原地写回。Phase 020 已把 production helper 和 test-only candidate 调整为同构写回；QEMU
correctness 通过，QEMU bench smoke 的 `ofm_public_adaptive_cut` 为 `0.66x`，只作为日志形状
和风险提示，不作为硬件性能结论。板卡复测因 SSH 超时未完成。
