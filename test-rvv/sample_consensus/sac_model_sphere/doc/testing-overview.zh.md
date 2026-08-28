# sac_model_sphere 测试总览

## 本文职责

本文只说明 test、bench、QEMU、board 和 Evidence Doctor 的入口分类与证据边界。具体 gtest 语义见 `doc/correctness-tests.zh.md`，候选取舍见 `doc/optimization-evidence.zh.md`。

## 运行入口分类

| 类别 | 入口 | 证明范围 | 不能证明 |
| --- | --- | --- | --- |
| correctness aggregate（正确性汇总入口） | `make -C test-rvv/sample_consensus/sac_model_sphere run_test_compare` | Std/RVV 两个构建中的 6 个 gtest 都通过。 | 不证明真实性能，也不证明未测试点型的性能。 |
| correctness alias（正确性别名） | `run_sphere_public_tests` / `run_board_sphere_public_tests` | 只跑本 topic 五个核心 gtest filter，包含 production direct select（真实生产入口 select）和 RGB/RGBA layout 对拍。 | 不覆盖其它 sample_consensus 模型。 |
| bench diagnostic | `bench_sac_model_sphere`，默认参数 `65536 200` | 输出 public entry 和测试专用 candidate 的计时行。 | QEMU bench timing 不作为性能结论。 |
| QEMU smoke（QEMU 小型验证） | `run_test_compare`、`dump_bench_rvv` | 正确性、构建、日志形状和 RVV 指令存在性。 | 不证明目标硬件性能。 |
| board smoke（板卡小型验证） | `SSH_AUTH_SOCK=<ssh-agent-socket> make -C ... board_smoke` | 板卡可达、test 通过、bench 输出可解析。 | 单次 run 不证明稳定性能。 |
| board repeated（板卡重复采集） | `collect_production_repeated_board_evidence`、`collect_production_vcompress_repeated_board_evidence`、`collect_point_type_repeated_board_evidence`、RGB/RGBA point-type repeated targets | production target 生成接入后 5-run manifest 和 Evidence Doctor。 | raw logs 默认不提交；环境字段仍有 taskset/governor/freq/temperature 缺失。 |
| doctor / registry | production / vcompress / point-type record 与 status targets | 检查 production repeated board 数值分布、证据角色和 registry freshness。 | target 不替代重新跑板卡；只复核对应已归档 repeated logs。 |
| historical probe guarded aliases | none | 当前 topic 没有历史 guarded probe。 | 不适用。 |

## 输入数据总览

测试和 bench 使用 synthetic sac_model_sphere direct indexed shell cloud（合成球壳点云，`indices_` 直接索引）。
当前 correctness 覆盖 `PointXYZ`、`PointXYZI`、`PointXYZRGB` 和 `PointXYZRGBA`；production board repeated 覆盖这些内建代表点型、65536 点、float xyz AoS layout（结构数组布局）。

## 覆盖矩阵

| 路径 | correctness | QEMU | asm | board repeated | Evidence Doctor | production 状态 |
| --- | --- | --- | --- | --- | --- | --- |
| public `countWithinDistance` | covered | covered | `countWithinDistanceRVV` 符号级 covered，`rvv_instr_count=19` | `PointXYZ` Phase 045 median `3.5154x`；`PointXYZI` Phase 050 median `1.9977x`；RGB/RGBA Phase 060 median 约 `2.01x-2.06x` | no Error / Warning | 已有 production RVV。 |
| public `selectWithinDistance` | covered | covered | `selectWithinDistanceRVV` 符号级 covered，`rvv_instr_count=27`，可见 `vcompress.vm` | `PointXYZ` Phase 045 median `2.0989x`；`PointXYZI` Phase 050 median `1.5901x`；RGB `1.6225x` clean；RGBA `1.5528x` with warning | RGBA select row 有 stability / long-tail Warnings；Errors 均属 getDistances candidate | Phase 045/046 `vcompress` production patch 已采纳。 |
| public `getDistancesToModel` | covered | covered | `rvv_instr_count=0` | `PointXYZ` median `1.0004x` | public getDistances 接近阈值或有退化 warning/error | 当前仍是标量；该异常不阻塞 select 采纳。 |
| diagnostic select candidate | covered | covered | historical partial inline evidence | Phase 020 manifest median `1.4161x` | no candidate Error | 已被 production direct select 取代为历史诊断证据。 |
| diagnostic getDistances candidate | covered | covered | partial inline evidence | negative | 5/5 退化 Error | 当前候选 rejected。 |

## 当前可提交证据和默认排除项

可提交候选是 Phase 000 / Phase 020 / Phase 045 / Phase 050 / Phase 060 的 manifest、Evidence Doctor Markdown / JSON、`log/evidence_registry.json`
和 topic-local 文档；`log/board/**` raw logs、`build/**`、远端路径和私有配置默认排除。
