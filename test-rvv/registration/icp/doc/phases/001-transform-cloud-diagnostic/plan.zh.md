# Phase 001：transformCloud 诊断候选

## 目标

建立 `IterativeClosestPoint::transformCloud` 的 test-only RVV candidate（测试专用 RVV 候选），覆盖
`PointXYZ` 和 `PointNormal` 全云顺序扫描，并用 QEMU correctness、QEMU bench smoke 和反汇编初步判断
是否值得上板。

## 计划动作

| action | 完成判据 | 状态 |
| --- | --- | --- |
| scaffold | `test-rvv/registration/icp` 具备 `src/`、`include/`、`include/impl`、README、evaluation 和 roadmap。 | planned |
| scalar reference | `transformCloudStd` 按动态 offset、finite check 和 in-place 语义复刻 production。 | planned |
| RVV candidate | `transformCloudCandidate` 在 `__RVV10__`、规模和 offset gate 满足时覆盖 PointXYZ / PointNormal。 | planned |
| correctness tests | gtest 覆盖 PointXYZ、PointNormal、normal 非有限值、in-place、小规模和 offset fallback。 | planned |
| bench smoke | bench 输出 `Dataset:`、`Iterations:`、case avg、`Total Time` 和 checksum。QEMU 只作日志形状。 | planned |
| asm attribution | `dump_bench_rvv` 至少能在 bench RVV binary 中看到候选相关 RVV 指令；production 符号归属暂不要求。 | planned |
| Evidence Doctor | 若生成 bench/asm 证据，按人工 doctor 写 Errors / Warnings / Suggestions，或运行全局脚本。 | planned |
| docs closeout | 更新 phase result、optimization matrix、topic doc 和 Handoff。 | planned |

## 证据边界

QEMU timing（QEMU 计时）不作为性能结论。没有 repeated board（重复板卡测试）前，EvidenceDecision 不能强于
`diagnostic`。production 源码不在本 phase 修改范围内。

## 停止条件

- RVV / 标量 correctness 不一致，先停在诊断修复。
- 交叉工具链、QEMU 或依赖缺失导致不能构建，则输出 blocked handoff。
- QEMU correctness 和 asm 通过但板卡不可用时，停止在 `diagnostic`，默认下一步是板卡 repeated bench。
