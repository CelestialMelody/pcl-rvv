# pcd_io_templated_writer phase index

| phase | status | scope | default recovery |
| --- | --- | --- | --- |
| `000-current-state-and-component-ablation` | done | templated compressed writer 4 字节字段布局转换组件消融 | result 已回填 |
| `010-production-shaped-compressed-writer-diagnostic` | done | pack + LZF compression 生产形态诊断 | result 已回填；结论为 partial-production-candidate |
| `020-PI1-production-integration-plan` | done | 窄范围 production integration plan | result 已回填；PI2 需要用户明确授权 |
| `030-structure-parity-doc-suite` | done | topic-local doc suite 和 structure parity | result 已回填；文档套件已补齐 |
| `040-binary-writer-component-ablation` | done | `writeBinary<PointT>` packed output loop 的 test-only binary component ablation | result 已回填；positive 已触发 Phase 050 PI1，不是 production evidence |
| `050-binary-writer-PI1-production-integration-plan` | done | `writeBinary<PointT>` 的窄范围 production integration plan | result 已回填；PI2 需要用户明确授权 |
| `060-compressed-writer-PI2-production-patch` | PI5 checkpoint | `writeBinaryCompressed<PointT>` 的 4 字节字段 production patch、production direct test 和板卡 production-public bench | result 已回填；证据 positive，等待用户确认采纳 / 回滚 |
| `070-compressed-writer-production-closeout` | done | 用户确认采纳后的 compressed writer production closeout 和正式 `doc-rvv` 文档 | result 已回填；compressed writer adopted |
| `080-binary-writer-PI2-production-patch` | rollback/no-production | `writeBinary<PointT>` 的 field-outer 4 字节字段 production patch、production direct test 和板卡 production-public bench | result 已回填；board mean `0.9842x / 0.9727x / 0.9810x`，Doctor Errors=3；用户确认负收益可回滚后，该 field-outer patch 和旧 `production_binary_*` 入口已移除。 |
| `090-binary-writer-tuple-segment-diagnostic` | diagnostic-positive | `writeBinary<PointT>` tuple / segment output family 的 test-only diagnostic | result 已回填；diagnostic board compact mean `9.8247x`，padding mean `4.0905x`，Doctor Errors=0 Warnings=2。 |
| `100-binary-writer-tuple-segment-production-probe` | adopted production behavior | `writeBinary<PointT>` 4 个连续 4-byte fields 的 compact memcpy / padding segment production path | result 已回填；production-public board compact mean `1.3046x`，padding mean `1.3240x`，small smoke mean `1.1082x`；Doctor Errors=0 Warnings=1。 |

当前默认恢复动作：

1. compressed writer 已完成 production closeout；正式文档为
   `doc-rvv/io/pcd_io_templated_writer-RVV.zh.md`。
2. binary writer Phase 080 field-outer path 已完成 rollback/no-production closeout；Phase 080 negative
   evidence 仅作为历史证据保留。
3. binary writer Phase 100 tuple / segment path 已完成 production closeout；当前 `writeBinary<PointT>`
   对 4 个连续 4-byte effective fields 采用 compact memcpy / padding segment path，其它布局 fallback。
4. 当前没有默认继续的高优先级 RVV production 动作；任意字段 compact memcpy、non-4-byte fields、
   更多 layout / point type 或 ASCII writer profile 都需要另开 phase 并先冻结范围。
