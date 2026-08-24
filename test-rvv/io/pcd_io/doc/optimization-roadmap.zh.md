# pcd_io 优化路线图

## 当前边界

topic scope（主题范围）是 `io/src/pcd_io.cpp` 的 PCLPointCloud2 binary / binary_compressed layout conversion。
当前证据已覆盖 test-only component ablation（组件消融）、writer compressed payload production-shaped
diagnostic（生产形态诊断）、reader compressed payload shaped diagnostic，以及 writer `std::ostream`
overload 的 production-public（真实公开入口）接入后证据。production（生产源码）当前只采用
`PCDWriter::writeBinaryCompressed(std::ostream&, ...)` 的 4 字节对齐有效字段 pack，不覆盖 reader、
file-name overload、templated writer、ASCII writer、PLY / VTK family 或 generic PCD 所有 datatype。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 4-byte field pack/unpack RVV stride path | 当前源码 + 已完成 field conversion topic | `x/y/z/rgb` 等 4 字节字段，AoS 与 field-major 互转 | 减少双重循环中的小 `memcpy` 和标量指针推进成本 | reader padded unpack 只有 weak-positive，不能外推到 production reader | correctness、QEMU、asm、board repeated、Evidence Doctor | attempted / diagnostic-positive | phase 000 complete |
| production-shaped compressed writer diagnostic | phase 000 + 复筛报告 PCD field-layout family | `writeBinaryCompressed` 的压缩 payload 生成 | 判断收益能否穿透 LZF | header、ostream flush、mmap / file I/O 未覆盖 | shaped bench、board、doctor、diagnostic-to-production mismatch audit | adopted upstream by Phase 050 | phase 010 complete |
| writer production integration | phase 010 positive + phase 020 PI1 | `PCDWriter::writeBinaryCompressed(std::ostream&, ...)` 的 4 字节字段 pack | production-public median 1.33x / 1.25x | 只覆盖 public ostream 和 4-byte aligned fields | PI2-PI5、fallback、asm、board、doctor、registry | adopted production behavior | phase 050 complete |
| topic-local doc suite structure | phase 020 result | README、testing overview、correctness、bench/evidence、optimization evidence、code map、production topic doc | 降低 reviewer 恢复成本，明确证据提交边界 | 不产生新的性能证据 | doc-suite role inventory、target granularity audit、artifact tracking | complete | phase 050 closeout complete |
| reader shaped unpack + scalar finite scan | Phase 000 reader unpack weak-positive | `readBodyBinary` compressed payload：LZF 解压、unpack、finite scan | 判断 reader unpack 收益是否穿透解压和 dense scan | 不覆盖 public read、mmap、file I/O 或 finite scan RVV | shaped correctness、bench、board、doctor、registry | attempted / weak-positive | phase 040 complete |
| finite scan RVV mask | `readBodyBinary` 后半段源码 | `FLOAT32/FLOAT64` finite 检查 | 可能减少 dense scan 成本 | 类型 switch 和 count 组合复杂；Phase 040 说明 reader shaped 收益偏弱 | 独立 component ablation 和 finite / NaN oracle | deferred / lower priority | 用户明确继续 reader 方向或 profile 指向 scan 时再排 |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | writer pack 比 reader unpack 更稳定，先把 writer 升级到 production-shaped diagnostic | component pack `1.12x`-`1.20x`，reader padded unpack 只有 `1.03x` median | writer payload shaped bench、board、doctor | high / done in phase 010 |
| 010 | writer payload 穿透 LZF 后仍 positive，可进入 PI1 计划 | writer payload median `1.18x` / `1.12x`，Doctor Errors=0 / Warnings=0 | production direct tests、fallback matrix、production asm、board production bench | high / needs user confirmation |
| 020 | PI1 范围已冻结，主线下一步是生产补丁 | fallback 矩阵和暂停条件已经写入 phase result | 用户确认 production patch 后执行 PI2-PI5 | high / blocked by authorization |
| 030 | doc-suite 结构已补齐 | PI2 前 reviewer 可以从稳定 role docs 恢复测试、bench、证据和 helper 边界 | PI2 后补 production direct role 扩展 | medium / complete |
| 040 | reader shaped payload 只有 weak-positive，不建议 reader production probe | LZF 解压和标量 finite scan 稀释 unpack 收益；padding median 1.03x near-threshold | 若继续 reader 方向，需要独立 finite scan RVV mask 或真实 profile 证据 | low / deferred |
| 050 | writer production-public 证据稳定 positive，当前窄 production patch 可采纳 | 接入后 5-run median 1.33x / 1.25x，Doctor Errors=0 / Warnings=0；fallback tests 已闭合 | 若继续扩大，需要新 phase 和新授权边界 | stop / adopted |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| ASCII writer RVV | stream formatting 和 locale 成本主导风险高；当前复筛也只建议记录风险 | profile 明确指向数值格式化外的可批处理成本 |
| templated writer `impl/pcd_io.hpp` | 已在复筛中作为单独 topic；本 topic 不混入 `PointCloud<PointT>` writer | `src/pcd_io.cpp` 组件证据完成后另开或切换 topic |

## 默认恢复动作

1. `turn_stop_deferred with stop_condition_hit`：当前默认暂停。Phase 050 已完成当前授权 production boundary，
   roadmap 和 matrix 中没有同边界 high-priority unblocked code shape。
2. `adopted`：topic-local doc suite 和 `doc-rvv/io/pcd_io-RVV.zh.md` 已同步 Phase 050 current truth。
3. `rejected with evidence for production probe`：reader unpack / scalar finite scan shaped path 已在 Phase 040
   关闭为 weak-positive；当前不建议 reader production probe。
4. `phase_deferred + lower_priority`：finite scan RVV mask 只有在用户明确继续 reader 方向或 profile 指向 scan
   时另建 phase；filename overload、templated writer、ASCII writer 和 non-4-byte fields 也需要独立授权。
