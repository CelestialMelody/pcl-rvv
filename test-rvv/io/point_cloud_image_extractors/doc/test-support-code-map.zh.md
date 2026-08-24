# Test Support Code Map

## 本文职责

本文帮助 reviewer 从文档定位到 test support（测试支撑）代码、bench wrapper、script 和 evidence output。
它不承担性能结论；性能结论见 `doc/benchmark-and-evidence.zh.md`。

## 总调用图

```text
src/test_pcie.cpp / src/bench_pcie.cpp
  -> include/pcie.h
    -> include/impl/pcie_support.hpp
      -> real PCL RGB extractor for scalar RGB reference
      -> real PCL normal extractor for scalar normal reference
      -> real PCL label extractor for scalar label mono reference
      -> test-only scalar scaling reference
      -> test-only RVV candidates
      -> test-only PI2 gate policy helpers
      -> production direct hook checks for current production header
  -> script/generate_pcie_evidence_manifest.py
    -> ../../script/evidence_doctor.py
```

## 稳定入口

| 路径 | 角色 | 说明 |
| --- | --- | --- |
| `include/pcie.h` | aggregator header（聚合头） | 测试和 bench 统一包含入口。 |
| `include/impl/pcie_support.hpp` | internal helper（内部 helper） | fixture、reference、candidate 和 post-pass。 |
| `src/test_pcie.cpp` | correctness executable | gtest 入口。 |
| `src/bench_pcie.cpp` | bench executable | case-filter、计时和 checksum 输出。 |
| `script/generate_pcie_evidence_manifest.py` | topic-local manifest wrapper | 把 topic bench log 转成 Evidence Doctor manifest。 |

## Helper 字典

| helper | 层级 | 调用者 | 作用 | 不能证明 |
| --- | --- | --- | --- | --- |
| `makeRgbCloud` | fixture | tests / bench | 构造 organized RGB/RGBA 点云。 | 真实 PCD 输入分布。 |
| `makeScalingCloud` | fixture | tests / bench | 构造 organized `PointXYZI` 点云。 | 任意 float 字段。 |
| `makeNormalCloud` | fixture | tests / bench | 构造 organized `PointNormal` 点云。 | 泛型 normal traits 和真实法线分布。 |
| `makeLabelCloud` | fixture | tests / bench | 构造 organized `PointXYZL` 点云，并覆盖 16 位截断边界。 | random / Glasbey label 分布。 |
| `extractRgbScalar` | diagnostic reference | tests | 调真实 PCL RGB extractor。 | RVV dispatch。 |
| `extractNormalScalar` | diagnostic reference | tests | 调真实 PCL normal extractor。 | RVV dispatch 和泛型 normal 点型。 |
| `extractLabelMono16Scalar` | diagnostic reference | tests | 调真实 PCL label extractor 的 `COLORS_MONO` 分支。 | RGB random / Glasbey 分支。 |
| `extractScalingScalar` | diagnostic reference | tests / Std bench | 复刻 intensity scaling 标量语义。 | 任意字段 extractor。 |
| `extractRgbRvv` | candidate | RVV build | v0 RGB/RGBA 跨步读取和逐 lane 写回。 | production dispatch。 |
| `extractRgbSegmentStoreRvv` | candidate | RVV build | v1 RGB/RGBA 跨步读取、收窄和 `vsseg3e8` 写回。 | production dispatch。 |
| `extractScalingRvv` | candidate | RVV build | v0 intensity scaling。 | full-range v0 性能正向。 |
| `extractScalingFullRangeReductionRvv` | candidate | RVV build | v1 full-range min/max 规约。 | NaN/Inf production 语义扩展。 |
| `extractNormalRvv` | candidate | RVV build | normal_x/y/z 跨步读取、float scale 后写 `rgb8`。 | production dispatch；Phase 060 已显示当前形态性能负向。 |
| `extractLabelMono16Rvv` | candidate | RVV build | label 跨步读取、低 16 位截断和 `mono16` 写回。 | production dispatch；RGB random / Glasbey 分支。 |
| `paintNaNsWithBlackScalar` | post-pass reference | tests | 保持 NaN 像素清零语义。 | 所有 encoding 的 production post-pass。 |
| `rgbProductionProbeGate` | test-only policy gate | tests | 表达 Phase 040 对 RGB production probe 的 exact `PointXYZRGB` / `PointXYZRGBA` 准入范围。 | 真实 production dispatch 或 fallback 命中。 |
| `scalingProductionProbeGate` | test-only policy gate | tests | 表达 Phase 040 对 exact `PointXYZI`、`intensity` 字段和 full-range scaling 的准入范围。 | 任意字段 scaling、真实 production dispatch 或 fallback 命中。 |
| `expectRgbProductionMatchesScalar` | production direct assertion | tests | 调真实 RGB extractor，对比标量参考并检查 RVV / scalar hook。 | 未冻结 RGB 点型的收益。 |
| `expectIntensityProductionMatchesScalar` | production direct assertion | tests | 调真实 intensity extractor，对比标量参考并检查 RVV / scalar hook。 | 其它字段和其它 intensity 点型。 |
| `expectLabelMono16ProductionMatchesScalar` | production direct assertion | tests | 调真实 label extractor 的 `COLORS_MONO`，对比标量参考并检查 RVV hook。 | generic label-like 点型和 RGB label modes 性能。 |

## Bench Harness 与 Case Registry

`src/bench_pcie.cpp` 负责：

- 解析 `--iterations`、`--warmup-iterations`、`--case-filter`。
- 构造 `640x480` synthetic organized clouds。
- 输出 label、平均耗时、Total Time、checksum 和输出规模。

case metadata 在 `script/generate_pcie_evidence_manifest.py` 的 `CASE_METADATA` 中维护，用来补充
candidate label、point type、checksum policy 和 asm boundary。

## Scripts 与 Evidence Output

| 输出 | 生成方式 | 用途 |
| --- | --- | --- |
| `log/board/repeated_phase020/summary.md` | `make collect_board_repeated` + shared repeated analyzer | repeated speedup summary。 |
| `log/board/repeated_phase020/evidence_manifest.json` | topic-local manifest wrapper | Evidence Doctor 输入。 |
| `log/board/repeated_phase020/evidence_doctor.md` | shared Evidence Doctor | 异常信号和结论降级依据。 |
| `log/board/repeated_phase060/summary.md` | `make collect_board_repeated` + shared repeated analyzer | normal v0 repeated speedup summary。 |
| `log/board/repeated_phase060/evidence_manifest.json` | topic-local manifest wrapper | Phase 060 Evidence Doctor 输入。 |
| `log/board/repeated_phase060/evidence_doctor.md` | shared Evidence Doctor | normal v0 退化频率和长尾解释依据。 |
| `log/board/repeated_phase070/summary.md` | `make collect_board_repeated` + shared repeated analyzer | label mono16 repeated speedup summary。 |
| `log/board/repeated_phase070/evidence_manifest.json` | topic-local manifest wrapper | Phase 070 Evidence Doctor 输入。 |
| `log/board/repeated_phase070/evidence_doctor.md` | shared Evidence Doctor | label mono16 正向诊断复核依据。 |
| `log/board/repeated_pi4/summary.md` | `make collect_board_repeated` + shared repeated analyzer | Phase 080 production-public repeated speedup summary。 |
| `log/board/repeated_pi4/evidence_manifest.json` | topic-local manifest wrapper | Phase 080 Evidence Doctor 输入。 |
| `log/board/repeated_pi4/evidence_doctor.md` | shared Evidence Doctor | production-public 正向复核依据。 |
| `log/board/repeated_phase090/summary.md` | `make collect_board_repeated` + shared repeated analyzer | Phase 090 label production-public repeated speedup summary。 |
| `log/board/repeated_phase090/evidence_manifest.json` | topic-local manifest wrapper | Phase 090 Evidence Doctor 输入。 |
| `log/board/repeated_phase090/evidence_doctor.md` | shared Evidence Doctor | label mono16 weak-positive 采纳依据。 |
| `build/asm/riscv/bench_pcie_rvv.asm` | `make dump_bench_rvv` | RVV 指令归属辅助证据。 |

## 拆分审计

当前 topic 已使用配置默认结构：`src/`、`include/`、`include/impl/` 和 topic-local `script/`。
`pcie_support.hpp` 同时承载 fixture、reference、candidate、policy gate 和 assertion-adjacent helper，
后续若进入 production integration loop（生产接入闭环）并继续增长，可按职责拆成 `fixtures`、
`references`、`candidates`、`policy_gates` 和 `bench_cases` 内部头。当前阶段不拆分，因为 helper
总量仍可在单文件内审查。
