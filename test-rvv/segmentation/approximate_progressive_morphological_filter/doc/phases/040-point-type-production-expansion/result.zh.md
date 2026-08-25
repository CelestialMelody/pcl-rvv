# 040 point-type-production-expansion 结果

## 当前结论

本阶段已完成 point type expansion（点类型扩展）闭环。`PointXYZI`、`PointXYZRGB` 和 `PointXYZRGBA` 在真实公开入口 `ApproximateProgressiveMorphologicalFilter<PointT>::extract(Indices&)` 上均通过 correctness（正确性）对拍，并在板卡 5-run production public repeated bench（真实公开入口重复性能测试）中保持 positive bucket（正向决策桶）。Evidence Doctor（证据体检）结果为 Errors=0 / Warnings=0 / Suggestions=0。

因此本阶段把 `point-type-production-expansion` 判为 `adopted within measured point types`。生产实现继续保留 `RVVXYZAoSFloatLayout<PointT>` gate（布局准入条件）；文档中的性能结论扩大到已测的 `PointXYZ`、`PointXYZI`、`PointXYZRGB` 和 `PointXYZRGBA`，不外推到其它 `PCL_XYZ_POINT_TYPES`、normal 复合点型、用户自定义点型或 `Scalar=double`。

## 计划动作回填

| action | status | 证据 | 说明 |
| --- | --- | --- | --- |
| 补 public correctness | done | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter run_test_compare` | Std / RVV 构建均 14/14 passed，新增点型均与 `PointXYZ` reference 对拍一致。 |
| 扩 production bench | done | `src/bench_apmf_production.cpp` + QEMU smoke | bench 输出 8 个 production public label，包含 `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 的 dense / non-dense。 |
| 扩 manifest 点型解析 | done | `script/generate_apmf_board_evidence_manifest.py` | `point_type_for()` 按 label 区分 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`，旧 `PointXYZ` label 保持兼容。 |
| 本地验证 | done | `check_production_rvv_asm`、显式 RVV 编译 | production bench RVV binary 包含 `apmfExtractRVV` 和关键 RVV 指令；显式实例化编译 exit 0。 |
| QEMU log-shape smoke | done | `log/qemu/analyze_bench_production_point_types_compare.log` | Std/RVV checksum 一致，label 可解析；QEMU timing 不作为性能结论。 |
| 板卡 repeated bench | done | `log/board/point-type-production-repeated-v1/summary.md` | 先跑 3-run，因 Evidence Doctor 报 low_run_count warning 扩到 5-run；8 个 production public label 均为 positive。 |
| Evidence Doctor | done | `log/board/point-type-production-repeated-v1/evidence_doctor.md` | 5-run manifest comparisons=8，Errors=0 / Warnings=0 / Suggestions=0。 |

## 5-run 板卡结果

| case | point type | runs | median speedup | min | max | decision |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| `apmf production public dense` | `PointXYZ` | 5 | 1.48x | 1.47x | 1.49x | positive |
| `apmf production public non-dense` | `PointXYZ` | 5 | 1.28x | 1.27x | 1.28x | positive |
| `apmf production public PointXYZI dense` | `PointXYZI` | 5 | 1.52x | 1.52x | 1.54x | positive |
| `apmf production public PointXYZI non-dense` | `PointXYZI` | 5 | 1.42x | 1.42x | 1.44x | positive |
| `apmf production public PointXYZRGB dense` | `PointXYZRGB` | 5 | 1.50x | 1.49x | 1.50x | positive |
| `apmf production public PointXYZRGB non-dense` | `PointXYZRGB` | 5 | 1.43x | 1.42x | 1.50x | positive |
| `apmf production public PointXYZRGBA dense` | `PointXYZRGBA` | 5 | 1.51x | 1.50x | 1.55x | positive |
| `apmf production public PointXYZRGBA non-dense` | `PointXYZRGBA` | 5 | 1.43x | 1.42x | 1.49x | positive |

## 证据链

| 证据类别 | 命令 / 路径 | 结果 | 不能证明什么 |
| --- | --- | --- | --- |
| correctness（正确性） | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter run_test_compare` | Std / RVV 各 14/14 passed | 不证明未测点型的板卡性能。 |
| asm attribution（反汇编归属） | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter check_production_rvv_asm` | passed；production RVV bench binary 中可定位 `apmfExtractRVV` 和 RVV 指令 | 不区分每个输入数据分布的热点占比。 |
| explicit compile（显式编译） | 交叉编译 `segmentation/src/approximate_progressive_morphological_filter.cpp` with `__RVV10__` | exit 0 | 不代表运行时性能。 |
| QEMU smoke | `ALLOW_QEMU_BENCH_COMPARE=1 BENCH_ARGS='--size 4096 --half 2 --iterations 1 --warmup 1' make -C ... run_bench_compare ...` | 8 个 label 均输出 Std/RVV checksum，checksum 一致 | QEMU timing 不能进入性能排序或采纳判断。 |
| board performance（板卡性能） | `log/board/point-type-production-repeated-v1/summary.md` | 8 个 production public label 5-run 均为 positive | 不证明其它 `PCL_XYZ_POINT_TYPES`、normal 复合点型、用户自定义点型或多线程场景。 |
| Evidence Doctor（证据体检） | `log/board/point-type-production-repeated-v1/evidence_doctor.md` | comparisons=8，Errors=0 / Warnings=0 / Suggestions=0 | governor/freq/temperature/VLEN 仍未记录；当前脚本未把这些字段缺失升为 finding。 |

## Diagnostic-To-Production Mismatch Audit 回填

| question | answer |
| --- | --- |
| evidence role | `production-public` |
| A/B boundary | `public_overload` |
| 当前决策问题 | `RVV-vs-scalar`，判断同一点型公开入口 RVV 构建是否快于标量构建 |
| diagnostic 是否可外推到 production | 不使用 diagnostic 外推；本阶段直接跑 production public |
| comparison-boundary / baseline mismatch 风险 | 低。Std/RVV 两侧使用同一 bench wrapper、同一输入构造、同一参数和 checksum policy。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段所有新增点型均为 positive，不需要额外 probe。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要。本阶段不是新 RVV family selection（实现族选择），只是同一 adopted family 的点型覆盖扩展。 |

## Optimization Matrix 更新

| candidate family | point type | correctness / fallback | bench / board | asm | doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `point-type-production-expansion` | `PointXYZI` | done，dense / non-dense public 对拍通过 | 5-run board positive：dense 1.52x、non-dense 1.42x median | done | clean | adopted | none for measured type |
| `point-type-production-expansion` | `PointXYZRGB` | done，dense / non-dense public 对拍通过 | 5-run board positive：dense 1.50x、non-dense 1.43x median | done | clean | adopted | none for measured type |
| `point-type-production-expansion` | `PointXYZRGBA` | done，dense / non-dense public 对拍通过 | 5-run board positive：dense 1.51x、non-dense 1.43x median | done | clean | adopted | none for measured type |

## Continue / Stop Decision

continue_stop_decision：`phase_complete`。

本阶段不再命中板卡阻塞。`PointXYZI`、`PointXYZRGB` 和 `PointXYZRGBA` 的 dense / non-dense production public 证据均为 positive，且 Evidence Doctor clean，因此按用户规则可采纳到已测点型的性能结论。

下一阶段默认入口：`050-tail-vector-filter-probe`。该阶段已执行并记录在 `doc/phases/050-tail-vector-filter-probe/result.zh.md`；050 相对 040 已采纳 RVV 基线整体为中性，生产源码改动已回退。
