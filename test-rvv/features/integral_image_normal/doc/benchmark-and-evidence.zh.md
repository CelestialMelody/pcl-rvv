# Integral Image Normal Bench 与证据

## 本文职责

本文记录 bench（性能测试）输出合同、case 字典、summary / manifest / Evidence Doctor（证据体检）路径、evidence registry（证据登记表）和提交边界。它只解释证据如何复核，不把 diagnostic helper 的速度写成 production performance（生产性能）。

## Bench 输出格式

`src/bench_integral_image_normal.cpp` 输出：

- build 角色：`Build: Std` 或 `Build: RVV`。
- evidence metadata：整体 bench banner 仍保留 diagnostic / mixed-by-case 提示；manifest 会把 `prod_compute_*`
  单独登记为 `production_direct` / `public_compute`。
- dataset：`integral_image_normal_synthetic_diagnostics`。
- iterations 和 warmup iterations。
- 每个 case 的 `ms / iter`。
- `checksum_<case>=... width=... height=...`。

checksum（校验和）来自每个 case 自己的抽样加权和。`map_prep_*` 采样 distance map；`avg3d_diff_*` 采样 diff_x / diff_y；`avg3d_profile_*` 采样 normal query 输出；`profile_component_*` 分别采样 diff-buffer、积分图 prefix 和 query 输出。`pcl_iin_*` 和 `pcl_avg3d_profile_*` 使用真实 PCL `IntegralImage2D<float,3>` 的 `setInput()` 与 query API（查询接口）。`prod_compute_*` 调用真实 public `IntegralImageNormalEstimation<PointXYZ, Normal>::compute()` 并采样 `getDistanceMap()`。checksum 作用是确认 Std / RVV 在当前 checksum policy 下产生同一输出形状；它不能替代 correctness test。

## Case 字典

| case | 规模 | 计时边界 | 证明点 | 不能证明什么 |
| --- | --- | --- | --- | --- |
| `map_prep_320x240` | VGA-like `320x240` | 只包含 depth-change map + distance initialization | 常规 organized image map-prep helper throughput（吞吐） | 完整 `computeFeature()` 性能 |
| `map_prep_641x481_tail` | 非整齐宽高，触发 tail | 同上 | VL tail 和非整齐行宽的性能稳定性 | 泛型点类型或 production fallback |
| `avg3d_diff_320x240` | VGA-like `320x240` | 只包含 4-float stride 点型的 diff_x / diff_y buffer 写入 | 常规 organized image diff-buffer helper throughput | 积分图构建、泛型 `PointInT` layout、完整 AVERAGE_3D_GRADIENT 性能 |
| `avg3d_diff_641x481_tail` | 非整齐宽高，触发 tail | 同上 | 大图 tail 形态下 strided load/store 是否有收益 | production direct 或小图稳定收益 |
| `avg3d_profile_320x240` | VGA-like `320x240` | diff-buffer + 测试专用积分图构建 + normal query | 局部 diff-buffer 收益进入完整 production-shaped diagnostic 后是否仍成立 | 真实 production `IntegralImage2D` 和 public dispatch |
| `avg3d_profile_641x481_tail` | 非整齐宽高，触发 tail | 同上 | 大图 tail 完整链路是否仍正向 | production direct 或泛型 layout |
| `profile_component_diff_*` | 同对应规模 | 只包含 profile harness 中的 diff-buffer | diff-buffer 在 profile wrapper 内的局部占比和收益 | 完整 AVERAGE_3D_GRADIENT |
| `profile_component_integral_*` | 同对应规模 | 只包含测试专用积分图构建 | 判断积分图构建是否占主成本 | PCL `IntegralImage2D::setInput()` 精确实现 |
| `profile_component_query_*` | 同对应规模 | 只包含测试专用 normal query loop | 判断 query loop 是否占主成本 | production `computePointNormal*()` 完整状态 |
| `pcl_iin_diff_*` | 同对应规模 | exact PCL profile wrapper 中的 diff-buffer | diff-buffer 在 PCL boundary wrapper 内的局部收益 | PCL integral/query 之外的 production dispatch |
| `pcl_iin_setinput_dxdy_*` | 同对应规模 | 真实 PCL `IntegralImage2D<float,3>::setInput()`，dx / dy 各一次 | PCL 积分图构建成本是否吞掉 diff-buffer 局部收益 | production fallback、泛型点类型、second-order integral |
| `pcl_iin_query_*` | 同对应规模 | 真实 PCL `getFiniteElementsCount()` + `getFirstOrderSum()` query loop | PCL query 成本是否稳定 | viewpoint flip、输出写回、mirror policy |
| `pcl_avg3d_profile_*` | 同对应规模 | diff-buffer + PCL `setInput()` + PCL query | exact PCL boundary 下完整诊断链路是否正向 | production direct 或 public dispatch |
| `prod_compute_avg_depth_320x240` | VGA-like `320x240` | 真实 public `IntegralImageNormalEstimation<PointXYZ, Normal>::compute()`，normal method 为 `AVERAGE_DEPTH_CHANGE` | production patch 接入后 public 入口是否快于 Std | 其它点类型、indices output、其它 normal method |
| `prod_compute_avg_depth_641x481_tail` | 非整齐宽高，触发 tail | 同上 | public 入口在 tail 规模下是否仍有收益 | 泛型点类型完整性能外推 |

## 当前板卡证据

当前结论以 repeated summary（重复摘要）为准：

| path | role |
| --- | --- |
| `log/board/repeated-summary.md` | 5-run board summary，当前性能数字主归属。 |
| `log/board/evidence_manifest.json` | topic-local wrapper 生成的 Evidence Doctor manifest（证据清单）。 |
| `log/board/evidence_doctor.md` | Evidence Doctor 输出，当前为 Errors=3 / Warnings=21 / Suggestions=8。 |
| `log/evidence_registry.json` | registry record target 生成；用于恢复时检查 summary / manifest / doctor 是否被覆盖。 |

5-run 结果：

| case | min speedup | median speedup | mean speedup | max speedup | checksum | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| `avg3d_diff_320x240` | 1.02x | 1.05x | 1.05x | 1.08x | `20440.8` | needs_review |
| `avg3d_diff_641x481_tail` | 1.33x | 1.37x | 1.37x | 1.44x | `150825` | positive for diff-only diagnostic |
| `avg3d_profile_320x240` | 1.00x | 1.10x | 1.10x | 1.16x | `-5.85432e+07` | weak / unstable |
| `avg3d_profile_641x481_tail` | 1.00x | 1.06x | 1.09x | 1.25x | `-5.6621e+07` | weak / unstable |
| `map_prep_320x240` | 5.19x | 5.27x | 5.25x | 5.29x | `1.7621e+07` | positive |
| `map_prep_641x481_tail` | 5.07x | 5.13x | 5.11x | 5.15x | `1.41569e+08` | positive |
| `pcl_avg3d_profile_320x240` | 1.00x | 1.01x | 1.01x | 1.02x | `-5.85432e+07` | neutral / weak |
| `pcl_avg3d_profile_641x481_tail` | 1.00x | 1.02x | 1.03x | 1.11x | `-5.6621e+07` | weak / unstable |
| `pcl_iin_query_320x240` | 0.86x | 1.00x | 0.98x | 1.06x | `-5.85432e+07` | unstable component |
| `pcl_iin_setinput_dxdy_641x481_tail` | 0.68x | 1.03x | 1.22x | 2.00x | `248276` | unstable component |
| `prod_compute_avg_depth_320x240` | 1.00x | 1.06x | 1.06x | 1.12x | `39192.8` | weak-positive production public |
| `prod_compute_avg_depth_641x481_tail` | 0.99x | 1.06x | 1.06x | 1.12x | `42918.4` | weak-positive production public |

Evidence Doctor 的 Error 来自非 production-direct 的 `pcl_iin_query_320x240`、
`profile_component_diff_320x240` 和 `profile_component_query_320x240` 退化频率。它们不是 checksum 错误，
而是说明 production-shaped profile 的 component 链路不能支撑 diff-buffer 生产化。`prod_compute_*`
两项只有 1/5 低于 1 的 warning，因此 production public 结论写成 weak-positive，并停在 PI5 用户检查点。

## 复现和刷新命令

```bash
make -C test-rvv/features/integral_image_normal run_board_evidence_doctor
make -C test-rvv/features/integral_image_normal record_board_evidence_state
make -C test-rvv/features/integral_image_normal run_board_integral_image_normal_repeated
make -C test-rvv/features/integral_image_normal evidence_status
```

`run_board_evidence_doctor` 不重新跑板卡；它解析已存在的 `repeated_001` 到 `repeated_005` 目录并刷新 manifest / summary / doctor。`run_board_integral_image_normal_repeated` 会重新部署当前 bench，采集 5-run repeated，并刷新 manifest / doctor / registry；运行后必须同步本文件、phase result、evaluation 和 Handoff。

## ASM Attribution

`make dump_test_rvv` 生成 `build/asm/riscv/test_integral_image_normal_rvv.asm`。`make dump_bench_rvv` 生成 `build/asm/riscv/bench_integral_image_normal_rvv.asm`。map-prep 相关关键 RVV 指令包括 strided load、masked `vse8.v`、`vmseq`、`vmerge` 和 `vsetvli`。diff-buffer 相关关键 RVV 指令包括 `vlse32.v`、`vfsub.vv` 和 `vsse32.v`。这些 asm 证明 RVV build 中目标 path（路径）存在；production helper 是 header inline template（头文件内联模板），符号归属按 bench/test RVV 指令和 production-direct case 共同解释。

## 提交边界

- topic source/docs 可作为 review-required 主题资产。
- `log/board/repeated-summary.md`、`evidence_manifest.json`、`evidence_doctor.md` 和 `log/evidence_registry.json` 是 summary evidence（摘要证据），默认 ignored-local；用户要求提交 evidence summary 时再用 `git add -f` 精确选择。
- raw `run_bench_*.log`、`build/`、完整 asm dump、本机 `config.mk` 和板卡私有地址不默认提交。
