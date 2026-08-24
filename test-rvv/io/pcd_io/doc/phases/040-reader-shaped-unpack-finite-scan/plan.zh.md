# Phase 040 计划：reader shaped unpack + finite scan 诊断

本阶段验证 reader compressed path（读取压缩路径）的 production-shaped diagnostic（生产形态诊断）：
LZF decompression（LZF 解压）之后执行 field-major（字段连续）到 AoS（结构数组）unpack，再执行 scalar
finite scan（有限值扫描）。目标是判断 Phase 000 中 reader unpack component 的收益是否会被 decompression
和 density scan（密度扫描）稀释。

## 阶段意图和边界

validated_scope：

- source boundary：`PCDReader::readBodyBinary` compressed path 中的 payload decode、unpack 和 finite scan。
- candidate family：4 字节字段 unpack 使用现有 RVV stride store candidate；finite scan 保持标量。
- layout：`xyzi` 4x u32 连续布局和尾部 padding 布局。
- evidence role：production-shaped diagnostic，不是 production direct。

unvalidated_scope：

- 真实 `PCDReader::read` public overload、header parsing、mmap range、file I/O、uncompressed binary path。
- `FLOAT64` finite scan RVV、non-4-byte fields、mixed datatype count、reader production dispatch。
- writer production patch。PI2 仍需要用户确认。

phase_closeout_boundary：

- 本阶段只能说明 reader shaped diagnostic 是否值得继续。
- 若结果 weak / negative / neutral / unstable，不直接推出 reader no-production；只更新 roadmap 和 matrix。

## 当前状态清单

| area | current evidence |
| --- | --- |
| unpack component | Phase 000：`unpack_xyzi_307k` median 1.11x；`unpack_padded_xyzi_307k` median 1.03x weak-positive。 |
| writer shaped | Phase 010：writer payload positive。 |
| production state | `io/src/pcd_io.cpp` 未修改。 |
| doc suite | Phase 030 已补齐 topic-local role docs。 |

## TDD 计划

| step | action | expected RED / GREEN |
| --- | --- | --- |
| R1 | 在 `src/test_pcd_io.cpp` 新增 compressed reader body scalar/candidate test | RED：helper 缺失导致编译失败。 |
| G1 | 在 `include/impl/pcd_io_support.hpp` 实现 helper | GREEN：payload decode、unpack、finite scan 与 scalar 一致。 |
| B1 | 在 `src/bench_pcd_io.cpp` 增加 reader shaped bench case | 能用 `--case-filter reader_payload` 隔离。 |
| E1 | 扩展 manifest wrapper 和 Makefile board repeated target | 生成 reader shaped summary / manifest / doctor / registry。 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper / reader-shaped payload wrapper |
| 当前决策问题 | reader shaped unpack RVV 是否仍值得后续 production probe |
| diagnostic 是否可外推到 production | partial yes。覆盖 decompress + unpack + finite scan；不覆盖 public read、header parsing、mmap 或 file I/O。 |
| comparison-boundary / baseline mismatch 风险 | yes。baseline / candidate 仍是 test helper，不是 production dispatch。 |
| weak / negative / neutral / unstable 时 bounded production probe 条件 | 若 shaped 结果不稳定或退化，reader 只保留为 deferred diagnostic，不进入 production probe。 |
| clean adoption 是否需要 production boundary 内 A/B | yes。reader 若继续 production，必须另做 PI1-PI5。 |

## 优化矩阵

| candidate family | row source | layout | correctness | bench / board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| reader shaped RVV unpack + scalar finite scan | PCLPointCloud2 compressed reader payload | 4 字节 fields，continuous / padded xyzi | planned | planned | planned | planned | planned |

## Evidence Doctor 和 registry 规则

- 新增 run label：`board-pcd-io-reader-payload-repeat-phase040`。
- output dir：`log/board/reader_payload_repeat_5`。
- case-filter：`reader_payload`。
- run budget：5-run repeated，`--iterations 20 --warmup-iterations 3`。

## 继续 / 停止条件

- 若 correctness、QEMU、asm、board、doctor 和 registry 都闭合，更新 phase result、matrix、roadmap、evaluation 和 role docs。
- 若 board 不可达、doctor Error 未修复、checksum 不一致或结果跨桶摇摆，停在 blocked / degraded evidence。
- 若 shaped result positive，下一步只能是 reader PI1 计划；不得直接修改 production。
