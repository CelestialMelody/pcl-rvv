# Phase 000 Result：current state and grid sampling diagnostic

## 执行摘要

本阶段建立了 `surface/src/on_nurbs/triangulation.cpp` 的接入前 RVV 诊断 topic，并实现测试专用 `param_grid_rvv_store` candidate。production 源码未修改。当前证据不支持进入 production integration loop。

## 已完成动作

| 动作 | 状态 | 证据 |
| --- | --- | --- |
| 建立 topic scaffold | done | `Makefile`、`board.mk`、`include/`、`src/`、`doc/`。 |
| 实现 RVV 参数网格写入 | done | `include/triangulation.h::createParamGridCandidate`。 |
| QEMU correctness | done | `make run_test_compare`，Std / RVV 均 2 tests passed。 |
| asm attribution | done | `make dump_bench_rvv`，RVV 指令归属到参数网格 helper 内联热点。 |
| board smoke | done | `param_grid_512_smoke`、`surface_eval_256_smoke`。 |
| Evidence Doctor manifest | done | `make doctor_board_smoke_manifests` 可复现两个 smoke manifest / doctor。 |
| board recovery | done | 用户确认板卡恢复；`make check_board_ssh` 通过，`ping 192.168.55.2` 0% packet loss，路由经 `enp6s0`。 |
| board correctness | done | `make run_board_test && make fetch_board_logs`，2 tests passed，见 `log/board/run_test.log`。 |
| 5-run repeated board | done | `param_grid_512_repeated_20260820_141209`、`surface_eval_256_repeated_20260820_141331`。 |
| RISC-V OpenNURBS symbol audit | blocked | `nm -D .../libpcl_surface.so | c++filt | rg 'ON_NurbsSurface::Evaluate|Triangulation::convertSurface2PolygonMesh|ON_3dPoint'` 无命中，真实 on_nurbs direct bench 仍缺符号链。 |

## 数值结果

| case | role | iterations / warmup | Std avg | RVV avg | Std/RVV | checksum | Evidence Doctor |
| --- | --- | ---: | ---: | ---: | ---: | --- | --- |
| `tri_param_grid_512` | diagnostic smoke | 20 / 3 | `7.9730 ms` | `7.9861 ms` | `0.998x` | equal | `Errors=1, Warnings=1, Suggestions=0` |
| `tri_surface_eval_256` | diagnostic smoke | 8 / 2 | `58.5395 ms` | `58.2759 ms` | `1.005x` | equal | `Errors=0, Warnings=1, Suggestions=1` |
| `tri_param_grid_512` | diagnostic repeated board | 20 / 3, 5-run | median values | median values | median `0.978x`, min `0.952x`, max `1.028x` | equal | `Errors=1, Warnings=0, Suggestions=0` |
| `tri_surface_eval_256` | diagnostic repeated board | 8 / 2, 5-run | median values | median values | median `0.999x`, min `0.985x`, max `1.036x` | equal | `Errors=1, Warnings=0, Suggestions=0` |

`tri_param_grid_512` repeated values 为 `1.028x, 0.975x, 0.952x, 1.020x, 0.978x`，3/5 低于 1。`tri_surface_eval_256` repeated values 为 `0.986x, 0.999x, 1.036x, 1.000x, 0.985x`，3/5 低于 1。两个 repeated Evidence Doctor 都给出 `ba_degradation_frequency` Error，不能支撑正向性能结论。

## diagnostic-to-production mismatch audit

| 项 | 当前事实 |
| --- | --- |
| evidence role | `diagnostic` / `pre-production diagnostic`。 |
| A/B boundary | baseline 和 candidate 都是 `test_helper`，wrapper 为 `bench_triangulation`。 |
| 当前决策问题 | 参数网格写入 RVV 是否值得进入 production probe。 |
| 是否可外推 production | 不可直接外推。当前没有真实 `Triangulation::convertSurface2PolygonMesh` / `convertSurface2Vertices` production direct 证据。 |
| comparison-boundary 风险 | 当前使用测试专用 Evaluate-like sink；它不是 `ON_NurbsSurface::Evaluate`。 |
| weak / negative / neutral 时 bounded production probe 条件 | 只有真实 OpenNURBS direct bench 可构建、full-path repeated board 至少 weak-positive、checksum 和 asm boundary 闭合时才考虑。当前不满足。 |
| clean adoption 条件 | 需要同一 production boundary 内的 RVV-vs-RVV 或 public Std/RVV 证据，并经过 PI5 用户确认。当前没有 production patch。 |

## EvidenceDecision

`param_grid_rvv_store`：diagnostic no-production for current candidate。理由是 5-run repeated board 中局部参数网格和测试专用 full-path 都出现 3/5 退化，Evidence Doctor 均为 `Errors=1`；同时真实 OpenNURBS direct 证据被当前 RISC-V 安装库符号缺失阻塞。

下一步不应直接改 production。若用户希望继续真实 on_nurbs 路径，需要先修复 / 重建 RISC-V `libpcl_surface` 与 OpenNURBS 符号链；否则当前 candidate 在本阶段收敛为 no-production。

## 2026-08-20 恢复审计补充

用户补充说明“由于没有支持 on_nurbs，与该依赖有关的可以忽略”。按该提示覆盖后，本阶段不再把真实 OpenNURBS / on_nurbs direct probe 当作默认恢复动作。

剩余不依赖该符号链的候选只有 `createIndices` 输出构造消融。当前源码输出是 `std::vector<pcl::Vertices>`，每个三角形包含一个小 `std::vector` 的 push / 分配；它不是连续数值数组，也没有清晰可维护的 RVV store（RVV 连续或跨步写入）边界。做 `reserve` 或预分配只能是非 RVV 标量消融，不能支撑本 topic 的 RVV production decision（生产接入决策）。

`continue_stop_decision`：暂停当前 topic 的 RVV 优化推进。`stop_condition_hit` 为：roadmap 和 optimization matrix 在用户补充边界内已经没有授权、未阻塞且值得推进的 RVV candidate；继续会转向已忽略的 on_nurbs 依赖链路，或转向非 RVV 标量消融。`next_phase_default`：无。若以后重新授权 on_nurbs / OpenNURBS 依赖链路，或明确要求非 RVV 分配消融，再新建 phase。

## Recovery Attempts

| date | command / probe | result | interpretation |
| --- | --- | --- | --- |
| 2026-08-20 | `make check_board_ssh` | `192.168.55.2` port 22 timeout | 配置地址当前不可达。 |
| 2026-08-20 | `ping -c 2 -W 2 192.168.55.2` | 100% packet loss | 目标没有 ICMP 回包。 |
| 2026-08-20 | `make check_board_ssh REMOTE_IP=172.16.89.126` | port 22 timeout | `test-rvv/config.mk` 中旧注释地址也不可达。 |
| 2026-08-20 | `make check_board_ssh REMOTE_IP=milkv-jupiter` | 解析到 `172.16.89.126`，port 22 timeout | SSH alias 没有提供新的可用路径。 |
| 2026-08-20 | `ip route get 192.168.55.2` / `ip route get 172.16.89.126` | 均经 `wg0`，源地址 `172.16.89.127` | 本机路由存在，但 peer / 目标不可达。 |
| 2026-08-20 | 第三轮 `make check_board_ssh` / `ping` / `REMOTE_IP=milkv-jupiter` | 同样 timeout / 100% packet loss | 同一外部 board/VPN 可达性阻塞已连续复现，当前 phase 无法生成 required 5-run repeated board evidence。 |
| 2026-08-20 | `nm -D /home/zoomin/codes/riscv/pcl-rvv/lib/libpcl_surface.so 2>/dev/null \| c++filt \| rg 'ON_NurbsSurface::Evaluate\|pcl::on_nurbs::Triangulation::convertSurface2PolygonMesh\|ON_3dPoint'` | 无输出 | 当前安装库仍不能支撑真实 OpenNURBS / on_nurbs direct probe。 |
| 2026-08-20 | 用户确认板卡恢复后 `make check_board_ssh` / `ping -c 2 -W 2 192.168.55.2` | SSH pass，ping 0% packet loss | 板卡恢复，继续执行 repeated board。 |

## Repeated Evidence Artifacts

| run label | summary | manifest | doctor | raw support |
| --- | --- | --- | --- | --- |
| `board-triangulation-param-grid-512-repeated-20260820` | `test-rvv/surface/triangulation/log/board/param_grid_512_repeated_20260820_141209/summary.md` | `test-rvv/surface/triangulation/log/board/param_grid_512_repeated_20260820_141209/evidence_manifest.json` | `test-rvv/surface/triangulation/log/board/param_grid_512_repeated_20260820_141209/evidence_doctor.md` | `param_grid_512_repeated_20260820_141209-raw` |
| `board-triangulation-surface-eval-256-repeated-20260820` | `test-rvv/surface/triangulation/log/board/surface_eval_256_repeated_20260820_141331/summary.md` | `test-rvv/surface/triangulation/log/board/surface_eval_256_repeated_20260820_141331/evidence_manifest.json` | `test-rvv/surface/triangulation/log/board/surface_eval_256_repeated_20260820_141331/evidence_doctor.md` | `surface_eval_256_repeated_20260820_141331-raw` |
