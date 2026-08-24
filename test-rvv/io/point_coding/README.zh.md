# point_coding RVV 导航

本 topic 来自 `doc-rvv/library-screening/io/io-retained-candidate-rescreen.zh.md` 的执行清单，目标源码是
`io/include/pcl/compression/point_coding.h`。当前已完成两段 production integration loop（生产接入闭环）：
Phase 060 采纳 exact `PointXYZ` decode RVV path，Phase 070 进一步采纳 PointXYZ-like traits gate（类似
PointXYZ 的点型特征门控）。Phase 080 补了完整 public octree boundary（公开八叉树入口边界）板卡审计，
结果为 near-threshold weak signal（接近阈值的弱信号），不作为新的稳定公开入口正向证据。

正式长期文档是 `doc-rvv/io/point_coding-RVV.zh.md`，其中的性能数据使用接入后的 production-direct（真实生产路径直连）板卡数据。

## 当前结论

`PointCoding<PointT>::decodePoints` 在 RVV 构建下对满足
`pcl::rvv::kRVVXYZAoSPointCompatible<PointT>` 的 xyz AoS（结构数组）点型走 RVV helper；非 RVV 构建和不满足 traits gate 的点型走 `decodePointsStd`。RVV helper 保留 production 标量的 double reference rounding（双精度参考点舍入）语义，只改循环组织、跨步加载和 AoS 写回方式。

接入后板卡证据：

| phase | case group | board result | Evidence Doctor |
| --- | --- | --- | --- |
| Phase 060 | `decode_production_direct_256/1024/4096/16384` for exact `PointXYZ` | 10-run median `1.22x / 1.23x / 1.18x / 1.16x`，min 全部大于 `1.0x` | `Errors=0，Warnings=1` |
| Phase 070 | `decode_production_direct_traits_xyzi_*` / `decode_production_direct_traits_xyzrgb_*` | 10-run median `1.27x / 1.20x / 1.27x / 1.19x`，min 全部大于 `1.0x` | `Errors=0，Warnings=2` |
| Phase 080 | `octree_decode_public_*` / `octree_roundtrip_public_*` | 10-run median `1.01x / 1.01x / 1.01x / 1.02x`，部分 1024 case min 为 `0.99x` | `Errors=1，Warnings=2，Suggestions=4` |

encode 当前保持 production 标量路径。Phase 020/030 已证明 f32 快速量化有 double 语义风险，f64 exact（精确双精度）量化虽正确但板卡成本过高。

## 阅读路径

| 读者问题 | 文档 |
| --- | --- |
| 函数做什么、当前是否值得继续 | `doc/point_coding-evaluation.zh.md` |
| 怎么运行 test、bench、board 和 Evidence Doctor | `doc/testing-overview.zh.md` |
| 每个 correctness test 覆盖什么 | `doc/correctness-tests.zh.md` |
| bench case、板卡摘要和证据边界 | `doc/benchmark-and-evidence.zh.md` |
| 候选族、反汇编和取舍 | `doc/optimization-evidence.zh.md` |
| 测试支撑代码地图 | `doc/test-support-code-map.zh.md` |
| 跨 phase 搜索空间和默认恢复动作 | `doc/optimization-roadmap.zh.md` |
| phase 计划、结果和优化矩阵 | `doc/phases/README.zh.md` |

## 常用命令

在 `test-rvv/io/point_coding` 下运行：

```bash
make run_test_compare
make run_qemu_bench_smoke
make dump_bench_rvv
make check_board_ssh
make run_board_test
make collect_board_repeated run_board_repeated_evidence_doctor \
  POINT_CODING_REPEATED_RUNS=10 \
  POINT_CODING_REPEATED_DIR=log/board/repeated_phase070_traits_decode \
  BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter decode_production_direct_traits_*'
make collect_board_repeated run_board_repeated_evidence_doctor \
  POINT_CODING_REPEATED_RUNS=10 \
  POINT_CODING_REPEATED_DIR=log/board/repeated_phase080_public_octree \
  BENCH_ARGS='--iterations 10 --warmup-iterations 2 --case-filter octree_*'
```

QEMU 只用于 correctness（正确性）、构建和日志形状，不写性能结论。`build/`、`log/qemu/` 和 `log/board/`
是生成产物；除非用户明确要求提交脱敏日志，否则默认不提交 raw logs（原始日志）。
