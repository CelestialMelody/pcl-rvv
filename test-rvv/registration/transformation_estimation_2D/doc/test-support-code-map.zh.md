# 测试支撑代码地图

## 本文职责

本文说明 `transformation_estimation_2D` 的 test support（测试支撑代码）如何按 `artifact_layout` 和 `test_support` 配置组织，帮助 reviewer 从文档定位到源码、target 和本地证据。

## 总调用图

```text
src/test_te2d.cpp
  -> include/te2d.h
    -> include/impl/te2d_candidates.hpp
      -> estimatePublic2D()
      -> estimateFused2DStd()
      -> estimateFused2DCandidate()
      -> estimateFused2DSourceIndexedCandidate()
      -> estimateFused2DDualIndexedCandidate()
      -> estimateFused2DCorrespondenceCandidate()

src/bench_te2d.cpp
  -> include/te2d.h
    -> ordered-cloud-pair public case
    -> ordered-cloud-pair fused candidate case
    -> row-source materialize-to-ordered candidate cases
```

## 稳定聚合入口

| 文件 | 作用 | 边界 |
| --- | --- | --- |
| `include/te2d.h` | 测试和 bench 的唯一稳定 include 入口。 | 不暴露给 production，不承载候选实现正文。 |

## Fixtures 与输入构造

| helper | 作用 | 证据角色 |
| --- | --- | --- |
| `makePointXYZCloud` | 构造 deterministic dense `PointXYZ` corpus。 | correctness / bench input。 |
| `makeNearCancellationCloud` | 构造大公共偏移和小扰动输入。 | 数值压力样本。 |
| `makeRigid2DTransform` | 固定 2D rotation + translation。 | public semantic anchor。 |
| `transformCloud2D` | 用固定矩阵生成 target。 | 输入构造。 |

## 标量 Reference

| helper | 作用 | 证据角色 |
| --- | --- | --- |
| `estimatePublic2D` | 调用当前 production public overload。 | scalar truth / public semantics anchor。 |
| `estimateFused2DStd` | 标量两遍中心化 fused reference。 | RVV candidate 的 same-chain reference。 |
| `solveTransform2DFromAccumulation` | 从 2D 质心和中心化 `H` 写出 4x4 transform。 | 公式 reference。 |

## Candidate / Diagnostic Helper

| helper | 作用 | 证据角色 |
| --- | --- | --- |
| `estimateFused2DCandidate` | RVV 构建下尝试 dense finite ordered-cloud-pair candidate；其它情况 fallback。 | diagnostic candidate。 |
| `estimateFused2DSourceIndexedCandidate` | 读取 source indices，将 source row materialize 成顺序点云对，再复用 fused candidate；展开成本计入 bench。 | source-indexed row-source diagnostic；不实现 production gather。 |
| `estimateFused2DDualIndexedCandidate` | 读取 source / target 两侧 indices，物化成顺序点云对，再复用 fused candidate；展开成本计入 bench。 | dual-indexed row-source diagnostic；不实现 production gather。 |
| `estimateFused2DCorrespondenceCandidate` | 读取 correspondence 的 query / match，物化成顺序点云对，再复用 fused candidate；展开成本计入 bench。 | correspondence row-source diagnostic；不实现 production gather。 |
| `accumulateFused2DRVV` | 第一遍 RVV 求 x/y 质心，第二遍 RVV 累加中心化 2x2 correlation。 | asm attribution input。 |
| `isDenseFiniteOrderedPair` | 检查 source/target 数量、dense 标记和 `pcl::isFinite`。 | test-only gate，避免改变非有限 production 语义。 |

## Bench Harness 与 Case Registry

| 文件 | case-filter | 说明 |
| --- | --- | --- |
| `src/bench_te2d.cpp` | `ordered-cloud-pair-public` | 真实 public overload probe；当前对应窄范围 production dispatch。 |
| `src/bench_te2d.cpp` | `ordered-cloud-pair-fused` | test-only fused candidate。 |
| `src/bench_te2d.cpp` | `row-source-fused` | source-indexed、dual-indexed、correspondence 三类 materialize-to-ordered candidate，各覆盖 4K/64K/256K。 |

## Scripts 与 Evidence Output

| 路径 | 作用 | 提交边界 |
| --- | --- | --- |
| `script/generate_te2d_asm_summary.py` | 解析 Std / RVV objdump，生成 diagnostic 和 production-public asm attribution summary。 | topic-local script；可提交。 |
| `script/generate_te2d_qemu_evidence_manifest.py` | 把 QEMU smoke log 和 asm summary 转成 Evidence Doctor manifest。 | topic-local script；可提交。 |
| `script/generate_te2d_board_repeated_summary.py` | 解析 board repeated `run-*` 日志，生成 diagnostic 或 production-public summary 和 manifest。 | topic-local script；可提交。 |
| `log/qemu/run_test_std.log` | Std correctness 输出。 | 本地生成；默认不提交。 |
| `log/qemu/run_test_rvv.log` | RVV correctness 输出。 | 本地生成；默认不提交。 |
| `log/qemu/run_bench_ordered_cloud_pair_smoke_rvv.log` | QEMU diagnostic bench smoke 输出。 | 本地生成；默认不提交。 |
| `log/qemu/asm_attribution.md` / `.json` | diagnostic asm attribution 摘要。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/evidence_manifest.json` / `evidence_doctor.md` | diagnostic QEMU Evidence Doctor manifest / report。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/production_public/run_bench_ordered_cloud_pair_public_rvv.log` | QEMU production-public smoke 输出。 | 本地生成；默认不提交。 |
| `log/qemu/production_public/asm_attribution.md` / `.json` | production-public asm attribution 摘要。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/production_public/evidence_manifest.json` / `evidence_doctor.md` | production-public QEMU Evidence Doctor manifest / report。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/row_source/run_bench_row_source_fused_rvv.log` | row-source QEMU smoke 输出。 | 本地生成；默认不提交。 |
| `log/qemu/row_source/asm_attribution.md` / `.json` | 聚焦 `row_source_lambda_boundary` 的 row-source asm attribution。 | 本地生成；summary-only 证据指针。 |
| `log/qemu/row_source/evidence_manifest.json` / `evidence_doctor.md` | 9-case row-source QEMU manifest / report。 | 本地生成；summary-only 证据指针。 |
| `log/board/ordered_cloud_pair_repeated/summary.md` | diagnostic board repeated summary。 | 本地生成；summary-only 证据指针。 |
| `log/board/ordered_cloud_pair_repeated/evidence_manifest.json` / `evidence_doctor.md` | diagnostic board manifest / report。 | 本地生成；summary-only 证据指针。 |
| `log/board/ordered_cloud_pair_public_repeated/summary.md` | production-public board repeated summary。 | 本地生成；summary-only 证据指针。 |
| `log/board/ordered_cloud_pair_public_repeated/evidence_manifest.json` / `evidence_doctor.md` | production-public board manifest / report。 | 本地生成；summary-only 证据指针。 |
| `log/board/row_source_fused_repeated/summary.md` | row-source board repeated summary。 | 已生成；摘要包含 9 个 case，64K 稳定性由 Doctor 标记。 |
| `log/evidence_registry.json` | 本地 evidence registry。 | 本地生成；默认不提交。 |
| `build/asm/riscv/bench_transformation_estimation_2D_rvv.asm` | RVV 指令 grep 结果。 | 本地生成；默认不提交。 |

Topic-local scripts 已由 Phase 050 接入。全局 `test-rvv/script/evidence_doctor.py` 只读取规范 manifest，不直接理解 `te2d` 的 case label 或 asm 符号。

## Production 与 Test Support 边界

当前 production 文件保留窄范围 RVV patch；production intrinsic 只在 exact `PointXYZ` / `float` / dense finite ordered-cloud-pair gate 命中。test-rvv 中的 row-source intrinsic 仍是诊断实现，不代表 production indexed / correspondence dispatch。

## 拆分审计

| shape | present | paths | roles found | risk if unchanged | decision | next action |
| --- | --- | --- | --- | --- | --- | --- |
| root test source | no | not_present | none | none | not_applicable with evidence | none |
| root bench source | no | not_present | none | none | not_applicable with evidence | none |
| `src/` source | yes | `src/test_te2d.cpp`、`src/bench_te2d.cpp` | gtest、bench thin entry、row-source case registry | 当前规模可审查。 | adopted | board evidence 完成后复审 case 统计。 |
| aggregator header | yes | `include/te2d.h` | stable include | none | adopted | none |
| internal helpers | yes | `include/impl/te2d_candidates.hpp` | fixtures、reference、candidate、assertions-adjacent helper | 394 行，低于 soft limit；职责混合但仍可审查。 | adopted | 后续 row source / scripts 增长时拆 fixtures、references、candidates。 |
| legacy `test_support/` directory | no | not_present | none | none | not_applicable with evidence | none |
| topic-local script | yes | `script/generate_te2d_asm_summary.py`、`script/generate_te2d_qemu_evidence_manifest.py`、`script/generate_te2d_board_repeated_summary.py` | asm attribution、QEMU manifest、board repeated summary | 已覆盖 row-source label、row-source boundary 和 manifest 字段。 | adopted | 若 board summary 增加字段再扩展 parser。 |
| bench case registry | structured | `src/bench_te2d.cpp` | 三类 case-filter 和 row-source case label | 当前足够区分 diagnostic、production-public 和 row-source candidate。 | adopted | 只有出现组件消融时再拆 internal bench cases。 |
| Makefile / board target | yes | `Makefile`、`board.mk` | build、QEMU、registry、board repeated target | none | adopted | none |
| topic-local docs | yes | `doc/*.zh.md`、`doc/phases/**` | doc suite | Phase 030 和 Phase 050 已补入最新 board 数值、Doctor 和 QEMU/board 分层。 | adopted | 新 candidate 或新复跑后继续刷新。 |
| `doc-rvv` long-term doc | planned | `doc-rvv/registration/transformation_estimation_2D-RVV.zh.md` | production long-term doc | 只记录窄范围 production patch，不写 row-source 诊断为已采用。 | applicable with narrow scope | 用户 review/adoption 后继续更新。 |
| evidence registry | local generated | `log/evidence_registry.json` | freshness pointer | 默认不提交，提交前需说明。 | adopted as local-only | 若用户要提交 evidence logs，再脱敏并重新审计。 |
