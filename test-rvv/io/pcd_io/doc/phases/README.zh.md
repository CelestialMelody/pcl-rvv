# pcd_io phase 索引

| phase | 状态 | 作用 | plan | result |
| --- | --- | --- | --- | --- |
| 000-current-state-and-component-ablation | complete | 建立 PCLPointCloud2 compressed pack/unpack 的组件消融证据 | `000-current-state-and-component-ablation/plan.zh.md` | `000-current-state-and-component-ablation/result.zh.md` |
| 010-production-shaped-compressed-writer-diagnostic | complete | 验证 writer compressed payload 生产形态诊断是否穿透 LZF | `010-production-shaped-compressed-writer-diagnostic/plan.zh.md` | `010-production-shaped-compressed-writer-diagnostic/result.zh.md` |
| 020-production-integration-plan-writer-payload | complete | 冻结 writer `std::ostream` overload 的 PI1 生产接入计划、fallback 矩阵和暂停条件 | `020-production-integration-plan-writer-payload/plan.zh.md` | `020-production-integration-plan-writer-payload/result.zh.md` |
| 030-doc-suite-structure | complete | 补齐 topic-local 测试总览、correctness、bench/evidence、optimization evidence 和代码地图 | `030-doc-suite-structure/plan.zh.md` | `030-doc-suite-structure/result.zh.md` |
| 040-reader-shaped-unpack-finite-scan | complete | 验证 reader payload 中 LZF 解压、unpack 和标量 finite scan 是否保留收益 | `040-reader-shaped-unpack-finite-scan/plan.zh.md` | `040-reader-shaped-unpack-finite-scan/result.zh.md` |
| 050-production-writer-payload-integration | complete | 完成 writer `std::ostream` overload 的 PI2-PI5 production integration、production-public 板卡复跑和正式文档 closeout | `050-production-writer-payload-integration/plan.zh.md` | `050-production-writer-payload-integration/result.zh.md` |

当前默认恢复入口：暂停。Phase 050 已把 `PCDWriter::writeBinaryCompressed(std::ostream&, ...)`
的 4 字节对齐有效字段 payload pack 写成 adopted production behavior，并创建
`doc-rvv/io/pcd_io-RVV.zh.md`。当前不建议在同一 production patch 内继续扩大到 reader、file-name overload、
templated writer、ASCII writer 或 non-4-byte fields。若用户明确继续同 topic，应另建 phase。
