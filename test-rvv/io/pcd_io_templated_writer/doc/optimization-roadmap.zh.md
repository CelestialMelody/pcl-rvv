# pcd_io_templated_writer 优化路线图

## 当前边界

topic scope（主题范围）是 `io/include/pcl/io/impl/pcd_io.hpp` 的 templated
`PCDWriter` writer。当前证据已覆盖 `writeBinaryCompressed<PointT>` 压缩前置布局转换的
test-only component ablation（组件消融）、pack+LZF production-shaped diagnostic（生产形态诊断）
和 Phase 060 production-public（真实公开入口）板卡 repeated bench。当前存在 compressed writer
production patch，用户确认采纳后已进入 adopted production behavior，并创建
`doc-rvv/io/pcd_io_templated_writer-RVV.zh.md`。`io/src/pcd_io.cpp`
的 PCLPointCloud2 路径不在本 topic。Phase 080 已把 `writeBinary<PointT>` field-outer packed output
接入真实 public boundary 并完成 PI2-PI5；board repeated 为负向。用户确认负收益可回滚后，
该 field-outer production patch 和旧 `production_binary_*` 入口已移除。随后 Phase 090/100
验证并接入新的 tuple / segment output family；当前 `writeBinary<PointT>` 对 4 个连续 4-byte
effective fields 采用 compact memcpy / padding segment production path，其它布局回退标量。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 4-byte field compressed writer RVV stride path | 当前源码 + PCD field-layout family 复筛 | `PointXYZ` / `PointXYZRGB` 形态的 4 字节字段，PointCloud AoS 到 field-major | 减少内层小 `memcpy` 和指针推进成本 | strided load 是否被写 buffer / LZF 稀释 | correctness、QEMU、asm、board repeated、Evidence Doctor | attempted positive | `000-current-state-and-component-ablation` |
| binary writer field-outer output path | 当前源码 + Phase 040 | `writeBinary<PointT>` point-major 到 file map packed binary | component ablation 曾显示减少多字段 memcpy 成本 | 真实 public writer 中 `vsse32` 跨步写出和 mmap/checksum 边界吞掉收益 | Phase 080 production direct、asm、board repeated、Doctor | rollback/no-production | closed |
| binary writer tuple / segment output path | Phase 080 负向反思 | 4 个连续 4 字节有效字段的 `writeBinary<PointT>` packed output | 一趟按点写出，避免 field-outer `vsse32` 跨步 store | 仅覆盖 4-field 4-byte 连续 offset；其它布局 fallback | test-first、QEMU smoke、asm、board repeated、Doctor | adopted via Phase 100 | closed |
| arbitrary compact payload memcpy | Phase 100 反思 | 有效字段从 offset 0 连续覆盖整个 `PointT` payload 的 binary writer | 非 RVV fast path，可让更多 compact layout 直接 memcpy | 不是 RVV intrinsic；需要证明 field list / padding / datatype 语义，可能扩大范围到更多 point types | 新 phase plan、point-type/layout matrix、public writer correctness、board repeated | deferred / non-default | 只有用户明确要求扩大 binary writer fast path 时创建 |
| ASCII writer | 复筛报告风险记录 | `writeASCII<PointT>` | 当前不建议首选 | stream formatting / locale 可能主导 | profile 指向非格式化成本后再考虑 | rejected with evidence | 无默认恢复 |
| production-shaped compressed writer diagnostic | phase 结果触发 | 真实 templated writer 的 compressed context | 判断收益能否穿透 LZF 和 file boundary | 仍缺真实 writer 入口、fallback 和 layout gate | shaped bench、board、doctor、mismatch audit | attempted positive | `010-production-shaped-compressed-writer-diagnostic` |
| PI1 narrow production integration plan | Phase 010 positive | `writeBinaryCompressed<PointT>(file_name, cloud)` 的 4 字节字段 pack helper | 把诊断收益转成可审查 production probe | public entry direct test、fallback、asm、board production bench 曾未闭合，Phase 060 已闭合 | PI1 plan、production direct test、fallback matrix、production board | adopted via Phase 070 | closed |
| compressed writer production public patch | Phase 060 | `writeBinaryCompressed<PointT>` public overload；4 字节字段，offset / stride 对齐 | 大规模 public writer 仍有 `1.31x` / `1.25x` mean speedup | Doctor 有 long-tail warnings；small case 有 1/5 退化，只能 smoke | 正式 doc-rvv、production closeout | adopted production behavior | closed |
| structure parity doc suite | phase loop maturity audit | topic-local README、testing overview、correctness、bench/evidence、optimization evidence、code map | 提升 reviewer 恢复和证据边界可读性 | 不改变 production decision | doc-suite role inventory、artifact tracking、diff check | adopted | `030-structure-parity-doc-suite` |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | pack-only 组件证据稳定 positive，可进入 pack+LZF shaped diagnostic | 板卡 repeated `1.44x`-`1.57x`，Doctor clean | shaped correctness、board repeated、Doctor | high |
| 010 | pack+LZF 生产形态仍稳定 positive，可写窄 PI1 计划 | compressed case mean `1.3493x`-`1.3904x`，Doctor clean | production direct tests、fallback、production asm、board production bench | high |
| 030 | topic-local doc suite parity 已补齐 | README 旧状态会误导下一轮恢复，现已拆出 testing/correctness/bench/evidence/code-map role | PI2 后补 production direct 文档和 production evidence | medium |
| 040 | binary writer packed output component ablation 稳定 positive | 大规模 median `1.2738x`，padding median `1.1842x`；small case positive 但 Doctor 标记 outlier | 已在 Phase 050 补 PI1；真实 public write/mmap 边界仍缺失 | medium |
| 050 | binary writer PI1 计划已完成 | Phase 040 positive 后需要先冻结 production 入口、indices 不接入和 fallback 矩阵 | 用户明确授权后进入 binary writer PI2；否则保持 pending-production-authorization | medium |
| 060 | compressed writer public production bench 仍 positive | public writer 计入 header、pack、LZF、mmap/write 和文件 checksum 后，大规模 mean `1.3122x / 1.2495x` | 用户确认采纳后创建正式 `doc-rvv`；small case 只做 smoke | high |
| 070 | compressed writer 已完成 production closeout | 用户确认采纳后，正式 `doc-rvv` 已使用 Phase 060 接入后板卡数据 | binary writer PI2 需要另开 production boundary 并重跑 production-public 证据 | high |
| 080 | binary writer production-public 负向并已回滚 | 真实 public writer board mean `0.9842x / 0.9727x / 0.9810x`，Doctor Errors=3；用户确认负收益可回滚后，production patch 已移除 | 默认暂停；若继续，只能在新 phase 尝试 tuple / segment output 或 compact memcpy fast path | optional |
| 090 | binary tuple / segment diagnostic 正向 | test-only diagnostic board compact mean `9.8247x`，padding mean `4.0905x`，Doctor Errors=0 Warnings=2 | 进入 Phase 100 有界 production probe；不能直接采纳 | high |
| 100 | binary tuple / segment production probe 正向并采纳 | 接入后 production-public board compact mean `1.3046x`，padding mean `1.3240x`；Doctor Errors=0 Warnings=1 | 刷新正式 doc-rvv；默认不继续扩大范围 | high |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| ASCII writer RVV | stream formatting、trim 和 locale 语义风险高，当前筛选也只要求记录风险 | profile 明确显示格式化外的批量字段读取是主成本 |
| PCLPointCloud2 writer / reader | 已由 `test-rvv/io/pcd_io` 独立 topic 承载 | 该 topic 完成后做 family 汇总，不在本 topic 混测 |
| binary writer field-outer RVV production patch | Phase 080 production-public board repeated 为负向，且 Doctor Errors=3；已回滚，不采纳 | 已由 Phase 090/100 的 tuple / segment 新实现族取代；不要复用该 patch。 |
| arbitrary compact payload memcpy | 不是 RVV 主路径，且需要扩大 point type / layout 证明范围 | 用户明确要求继续扩大 binary writer fast path，且先写 phase plan。 |

## 默认恢复动作

1. compressed writer 已完成 production closeout，当前无需继续该方向。
2. binary writer Phase 100 已完成 production closeout；当前无需继续该方向。
3. 当前没有默认继续的高优先级 RVV production 动作。任意字段 compact memcpy、non-4-byte fields、
   更多 point type / layout 和 ASCII writer profile 都是新范围或非 RVV 主路径，应另开 phase 并先冻结证据计划。
4. ASCII writer、PCLPointCloud2 和 non-4-byte fields 不作为默认恢复动作。
