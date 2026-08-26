# 优化证据索引

本文按优化方式说明 adopted、rejected 和 deferred 路线对应的代码、测试、bench 和证据。详细阶段过程归属在 `doc/phases/`，正式生产行为归属在 `doc-rvv/segmentation/extract_polygonal_prism_data-RVV.zh.md`。

| 优化方式 | 状态 | 代码 / target | board evidence | 证据边界 |
| --- | --- | --- | --- | --- |
| full-scan polygon RVV | adopted | production `segmentRvv`；`make run_test_compare`；`make run_board_eppd_repeated_production` | `log/board/repeated-production/summary.md` median 1.75x | `PointXYZ`、single polygon、dense ordered、`indices_->size() >= 32` |
| indexed gather | adopted | production indexed branch；`SegmentRvvMatchesSegmentStdForIndexedSinglePolygon`；`make run_board_eppd_repeated_production_indexed` | `log/board/repeated-production-indexed/summary.md` median 1.75x | legal indices，32-bit byte offset 可表达 |
| concave hull XOR | adopted | multi polygon loop；`SegmentRvvMatchesSegmentStdForNestedPolygons` / indexed nested test | nested dense 2.18x；nested indexed 2.13x | legal `pcl::Vertices` polygon；退化或越界 fallback |
| point-type expansion <=32 bytes | adopted | `RVVXYZAoSFloatLayout<PointT>` + `sizeof(PointT) <= 32`；point-type tests | `PointXYZI` 1.85x、`PointXYZRGB` 1.83x、`PointXYZRGBA` 1.84x | 只读取 float xyz；不覆盖宽 stride |
| `PointXYZINormal` wide stride | rejected for RVV / scalar fallback adopted | `sizeof(PointT) > 32` compile-time fallback；`SegmentRvvDeclinesPointXYZINormal` | post-gate fallback median 1.00x；historical pre-gate confirm20 有退化频率 | 需要 dedicated wide-stride phase 才能重开 |
| threshold 32 | adopted | `indices_->size() < 32` fallback；`SegmentRvvDeclinesSmallInputs` | confirm5 median 1.19x、min 1.18x、doctor 0 / 0 / 0 | 不证明 `<32` 强行 RVV |
| `projectPoints` RVV 化 | deferred to separate topic | 当前 production RVV 不改 `SampleConsensusModelPlane::projectPoints` | 当前 production bench 已计入其标量前置成本 | 影响上游 sample_consensus 组件，需要 component ablation topic |
| `Scalar=double` / custom point type | deferred | production gate 只使用 float xyz layout | no current board evidence | 需要新的 layout / 数值证据 |

当前 topic 内没有高优先级 unblocked（未阻塞）优化路线。继续 wide-stride、`projectPoints` 或 custom point type 都会扩大当前 production boundary（生产边界），不建议在本 topic 自动 loop 中继续。
