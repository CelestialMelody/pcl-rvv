# bfgs 测试支撑代码地图

## 本文职责

本文记录 `bfgs` topic 的测试支撑代码地图。Phase 020 已创建 `include/`、`include/impl/`、`src/`、`script/` 和 board support，这些文件只服务 test-rvv 诊断，不改变 production。

## 总调用图

```text
src/test_bfgs.cpp
  -> include/bfgs.h
    -> include/impl/bfgs_fixtures.hpp
    -> include/impl/bfgs_references.hpp
    -> include/impl/bfgs_candidates.hpp
  -> registration/include/pcl/registration/bfgs.h

src/bench_bfgs.cpp
  -> include/bfgs.h
  -> direction-update / move-to-slope / gicp-shaped-vector6 case registry
  -> log/qemu/run_bench_direction_update_rvv.log
  -> script/generate_bfgs_qemu_evidence_manifest.py
  -> log/qemu/evidence_doctor.md

board.mk + Makefile board targets
  -> run_board_bench_bfgs_direction_update_repeated
  -> log/board/direction_update_repeated/run-*/run_bench_{std,rvv}.log
  -> script/generate_bfgs_board_repeated_summary.py
  -> log/board/direction_update_repeated/summary.md
  -> log/board/direction_update_repeated/evidence_doctor.md
```

## 稳定聚合入口

`include/bfgs.h` 是稳定聚合入口。test 和 bench 只 include 这个文件，避免直接依赖多个内部 helper。

## Fixtures 与输入构造

`include/impl/bfgs_fixtures.hpp` 保存：

- 固定 `Vector6d` 样本，匹配 GICP caller。
- 固定二次函数 functor，用于 public API smoke。
- 特殊边界样本：`dxdg == 0`、空输入、固定 `Vector6d` 初值和目标点。

## 标量 Reference

`include/impl/bfgs_references.hpp` 保存 test-only reference（测试专用参考链路），只复刻 BFGS 状态更新语义，不复制 production 全部 line-search 状态机。

## Candidate / Diagnostic Helper

`include/impl/bfgs_candidates.hpp` 保存 test-only diagnostic helper。手写 RVV helper 标明：

- 只服务测试资产。
- 不改变 production dispatch。
- 不能替代 production direct 证据。

## Bench Harness 与 Case Registry

`src/bench_bfgs.cpp` 维护 `direction-update`、`move-to-slope`、`gicp-shaped-vector6` 和 `all` case-filter。case label 能让 Evidence Doctor 判断 A/B boundary（对比边界）和 checksum 角色。

## Scripts 与 Evidence Output

`script/generate_bfgs_qemu_evidence_manifest.py` 解析 topic-local QEMU smoke 日志和 filtered asm 路径，再生成通用 Evidence Doctor manifest。

`script/generate_bfgs_board_repeated_summary.py` 解析 board repeated `run-*` 目录，生成 `test-rvv/registration/bfgs/log/board/direction_update_repeated/summary.md` 和 `test-rvv/registration/bfgs/log/board/direction_update_repeated/evidence_manifest.json`。全局 `test-rvv/script/evidence_doctor.py` 仍负责通用证据体检。

## Production 与 Test Support 边界

production 源码当前不改。test support 可以 include `registration/include/pcl/registration/bfgs.h`，但不能通过宏覆盖或 shadow header（测试专用头覆盖）改变 production 行为。

## 拆分审计

| area | current shape scan | config / quality bar | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| test/bench source layout | `src/test_bfgs.cpp`、`src/bench_bfgs.cpp` | 配置要求 topic-local `src/` | `adopted` | Phase 010 已创建并通过 QEMU build/test；Phase 020 board negative | 无默认生产动作 |
| aggregator and internal helpers | `include/bfgs.h`、`include/impl/*.hpp` | 配置要求聚合入口 `include/` 和内部目录 `include/impl` | `adopted` | helper 拆成 fixtures / references / candidates | 无 |
| script and bench registry | `script/generate_bfgs_qemu_evidence_manifest.py`、`script/generate_bfgs_board_repeated_summary.py`、bench case-filter | topic-specific manifest wrapper 放本地 script | `adopted` | QEMU manifest / doctor 已生成；board summary / doctor negative | 无默认生产动作 |
| board support | `board.mk`、`collect_board_bfgs_direction_update_repeated` | 复用 shared board fragment | `adopted / negative` | Milkv-Jupiter 5-run repeated，doctor Errors=2 | 停止默认 caller / production 队列 |
| legacy compatibility | 无旧路径 | 不保留 alias | `not_applicable with evidence` | 新 topic 无旧入口 | 无 |
