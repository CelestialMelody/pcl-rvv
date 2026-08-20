# bilateral_upsampling 测试总览

本文承担 `testing_overview` role：说明测试入口、target 粒度、QEMU / board 证据边界和覆盖矩阵。每个 gtest 的输入和断言详见 `doc/correctness-tests.zh.md`；bench label、计时边界和 Evidence Doctor 详见 `doc/benchmark-and-evidence.zh.md`；候选取舍详见 `doc/optimization-evidence.zh.md`。

## 运行入口分类

| 入口 | 类型 | 当前用途 | 不能证明 |
| --- | --- | --- | --- |
| `make -C test-rvv/surface/bilateral_upsampling run_test_compare` | correctness aggregate（正确性汇总入口） | 构建并运行 Std / RVV gtest，覆盖 diagnostic helper 和 production public tests | 不证明性能，也不替代板卡 bench |
| `make -C test-rvv/surface/bilateral_upsampling dump_bench_rvv` | QEMU / asm smoke（反汇编小型验证） | 生成 RVV bench binary 和 asm，确认关键 RVV 指令能归属到候选或 production helper | 不证明板卡收益 |
| `make -C test-rvv/surface/bilateral_upsampling board_smoke` | board smoke（板卡小型验证） | 在 Milkv-Jupiter 上运行 correctness 与 Std/RVV bench compare，输出 `log/board/analyze_bench_compare.log` | 当前不是多次 repeated 统计；决策依赖 Evidence Doctor 和 phase 预算说明 |
| gtest filter | correctness alias（正确性细分入口） | 当前未提供 Make alias；必要时可手动用 gtest filter 定位单个 TEST | 不作为默认 closeout 入口 |
| bench case-filter | bench diagnostic alias（bench 诊断细分入口） | 当前 bench CLI 没有 case-filter，固定运行全部 case；细分由 label 和文档字典完成 | 不能单独隔离一条 case 的耗时预算 |
| doctor / registry target | Evidence Doctor / registry alias（证据体检 / 登记入口） | 当前 phase 通过 manifest 和 `evidence-doctor.md` 留痕；`evidence_registry_status=not_available` | 没有独立 Make target 时不能声称 registry 自动闭合 |

## Target 粒度审计

| target 类别 | 当前形态 | 证明范围 | 缺口 / 处理 |
| --- | --- | --- | --- |
| correctness aggregate | `run_test_compare` | Std/RVV 共 13 个 TEST 通过，包含 same-type / cross public-entry correctness 和 infinity skip | adopted closeout 可用 |
| correctness aliases | 无固定 Make alias | 可通过 gtest filter 手动定位 | 当前文档用 `correctness-tests` 字典补足定位 |
| diagnostic bench aliases | 无 CLI filter；bench labels 区分家族 | table、direct-depth、production public、steady、helper-only、nan-mask、color-gather 均可从输出 label 复核 | 后续若继续扩候选，建议补 case-filter |
| QEMU smoke aliases | `dump_bench_rvv` 与窄 bench smoke | 构建、日志形状、asm 归属 | QEMU timing 不写成性能结论 |
| board smoke aliases | `board_smoke` | 板卡可运行、correctness、Std/RVV compare summary | 当前 `run_count=1`，不是 repeated benchmark suite |
| board repeated aliases | phase manifest 中记录 `run_count=1` | 本轮按 bounded smoke 形成 production decision | 若 reviewer 要求稳定性统计，可新开复跑 phase |
| doctor / registry aliases | phase 060/070 manifest + doctor 文件 | 暴露 Errors / Warnings / Suggestions | registry 暂无自动登记，Handoff 写明手工检查路径 |
| historical probe guarded aliases | phase 文档和 optimization matrix | 保留旧 family 负向证据，避免误读为当前 production 行为 | 默认恢复动作不继续旧 family |

## 输入数据总览

测试和 bench 使用合成 organized RGBD grid。规模覆盖 `80x60 w3 dense`、`120x90 w4 holes` 和 `180x120 w5 dense`；correctness 还包含 `32x24`、`40x28`、`8x7` 和全 NaN 小样本。当前 adopted production path 覆盖 RGB/RGBA exact family：`PointXYZRGB -> PointXYZRGB`、`PointXYZRGBA -> PointXYZRGBA`、`PointXYZRGB -> PointXYZRGBA`、`PointXYZRGBA -> PointXYZRGB`、`Scalar=float` 和 `RVVXYZAoSFloatLayout`。

`holes` case 用规律 NaN 深度触发 finite-depth skip（有限深度跳过）和 NaN fallback。RGB 查表和 depth 查表使用 `window_size`、`sigma_depth=0.5f`、`sigma_color=15.0f`，与 production helper 的预计算语义对齐。

## 覆盖矩阵

| 证据对象 | diagnostic helper | production public | fallback / 边界 | 当前 closeout 角色 |
| --- | --- | --- | --- | --- |
| table staged-window | covered | not adopted | 旧候选负向 | historical rejected |
| direct-depth diagnostic | covered | not adopted | 只能启发 production probe | historical precursor |
| old production exact-gate helper | covered | covered historically | public / steady / helper-only 负向 | rejected / superseded |
| local nan-mask k64 | covered in bench only | not production | NaN-only，不覆盖 infinity | attempted / negative |
| color-gather family | covered by bench-local precursor | covered by phase 073 RGB/RGBA exact-family production public / steady | RGB/RGBA exact family + AoS float layout gate | adopted / refreshed |
| cross RGB/RGBA output | not applicable as separate fallback | covered by phase 073 public / steady | alpha 不作为本阶段写回语义 | adopted scope expansion |
| 其它点型 / `Scalar=double` / 其它 layout | not covered | scalar fallback | 需要新 phase | deferred |

## 当前可提交证据

可引用的证据入口是 `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log`、`build/asm/riscv/bench_bilateral_upsampling_rvv.asm`、`log/board/analyze_bench_compare.log`、phase 060/070 的 `evidence_manifest.json` 和 `evidence-doctor.md`。默认不提交本地 build 输出、未脱敏 raw board log、私有板卡地址或聊天记录。

当前 closeout 结论由 phase 073 RGB/RGBA exact-family production direct board 结果、QEMU correctness、asm attribution 和 Evidence Doctor 共同支撑；QEMU bench timing 不参与性能判断。Phase 071 修复 finite mask、phase 072 收窄 same-type gate，phase 073 扩展交叉 RGB/RGBA 后，QEMU correctness、asm 和板卡 `board_smoke` 均已刷新。
