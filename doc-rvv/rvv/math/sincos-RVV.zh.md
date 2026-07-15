# sinf/cosf paired finite-domain RVV 受限接入原型设计与证据

本文记录 finite-domain（有限输入域）`sinf/cosf` paired RVV helper 的受限生产接入原型设计、验证证据和 production gate（生产接入门禁，未通过时不能进入生产接入）状态。它不是完整 production helper（生产 helper）实现说明：当前在 `common/include/pcl/common/impl/rvv_math.hpp` 中保留有限域原型入口，并通过 `test-rvv/rvv/math/sincos/prototype_include/` 下的测试专用头文件覆盖验证 RangeImageSpherical 接入形态；production RangeImageSpherical 头文件保持未修改，没有接入 base `RangeImage` 生产路径，也没有新增 public API。

相关实验资产位于 `test-rvv/rvv/math/sincos/`：

- `sincos/script/parms_sincos.py`：参数脚本，生成 Taylor baseline 与 `lp-abs` 候选，并报告 kernel（约化区间多项式核函数）/ dense-chain（密集链路模拟）误差。
- `sincos/sincos_finite_domain.hpp`：仅测试使用的 helper，按 kernel（约化区间多项式核函数）、lane-level helper（单个 RVV 向量寄存器级 helper）、batch wrapper（批量包装层）、single wrapper（单函数包装）分层组织测试入口。
- `sincos/sincos_test.cpp`：数学专项测试，覆盖 dense/adversarial（密集网格/边界对抗点）、special values（特殊值）、signed zero（带符号零）、scalar/RVV same-chain（标量/RVV 同构链路）和 microbench（微基准）。
- `sincos/sincos_range_smoke.cpp`：RangeImageSpherical 形态的调用方冒烟测试，只作为下游证据。
- `sincos/prototype_include/pcl/range_image/impl/range_image_spherical.hpp`：仅测试使用的 RangeImageSpherical 接入原型，只对 integration test（接入测试）生效，不修改 `common/include` 下的生产头文件。
- `sincos/sincos_range_image_spherical_integration_test.cpp`：RangeImageSpherical 仅测试接入验证，通过测试专用头文件覆盖中的 `calculate3DPoint` 原型命中 RVV helper。

这些文件刻意保留了比生产代码更详细的中文审查型注释：文件开头说明“本文件做什么”，英文术语首次出现时给出顺口的中文解释，非平凡函数标明属于参考链路、标量/RVV 同构链路、RVV 执行链路、验收条件、性能测试或调用方形态冒烟测试哪一类。这样做不是把测试代码伪装成生产代码，而是让 reviewer 能直接看出 kernel、完整 helper、lane-level RVV 入口、batch wrapper 和调用方形态冒烟测试的边界。

---

## 1. 目标与非目标

目标：

- 验证一个 finite-domain paired `sinf/cosf` RVV 实验 helper 的数学方案和测试流程。
- 在输入域 `[-pi, pi]` 上同时计算 `sin(x)` 与 `cos(x)`，并报告 caller-relevant 子域 `[-pi/2, pi/2]`。
- 使用 bounded mask range reduction，避免第一版依赖 `vfcvt_x_f`、FRM/FCSR 或 arbitrary finite float 级 range reduction。
- 对比 Taylor baseline 与可复现的 LP/minimax-on-grid absolute-error 候选。
- 建立参数脚本、C++ 标量同构链路、RVV intrinsic（RVV 内建函数）链路、QEMU、反汇编、板卡 microbench（微基准）和调用方形态冒烟测试的证据矩阵。
- 建立一个 `rvv_math.hpp` 风格的受限 lane-level 原型：`pcl::sincos_finite_domain_RVV_f32m2(x, s, c, vl)`。
- 验证 RangeImageSpherical 这个真实调用方在输入域可证明时，可以通过仅测试使用的原型文件展示接入方式。

非目标：

- 不是 strict libm replacement，不覆盖任意有限 `float` 的周期约化。
- 不替换 `std::sin`、`std::cos`、`sinf` 或 `cosf` 的公开语义。
- 不把 `common/include/pcl/common/impl/rvv_math.hpp` 中的受限原型视为 production helper（生产 helper）。
- 不把 `test-rvv` 下仅测试使用的 `RangeImageSpherical` 接入原型视为默认生产路径。
- 不修改 base `RangeImage` 生产路径。
- 不把 RangeImageSpherical smoke 当作数学专项测试的替代。

---

## 2. 语义合同

当前实验合同是 finite-domain fast approximation（有限输入域快速近似）：

- 接受域：有限 `float` 且 `x in [-pi, pi]`。
- 重点报告子域：`x in [-pi/2, pi/2]`。
- 域外有限输入：实验测试中返回 `{NaN, NaN}`，不 clamp 到合法角度。
- `NaN`、`+Inf`、`-Inf`：实验测试中返回 `{NaN, NaN}`。
- signed zero：`sin(+0) = +0`，`sin(-0) = -0`，`cos(+0) = cos(-0) = +1`。
- subnormal（次正规数）：属于有限且在合同域内，走正常快速路径。
- large finite：超出 `[-pi, pi]`，实验测试中归为 domain-out，返回 `{NaN, NaN}`。

特殊值矩阵摘要：

| 输入类别 | 当前实验行为 | 备注 |
| --- | --- | --- |
| `NaN` | `{NaN, NaN}` | NaN 分类也参与 RVV 与标量同构链路验收 |
| `+Inf` / `-Inf` | `{NaN, NaN}` | 不进入 polynomial 主路径 |
| `+0` | `{+0, +1}` | 位级带符号零验收 |
| `-0` | `{-0, +1}` | 位级带符号零验收 |
| 正负 subnormal | 正常计算 | 输入在合同域内 |
| `nextafter(pi, +Inf)` / `nextafter(-pi, -Inf)` | `{NaN, NaN}` | 有限但 domain-out |
| 大幅值 finite | `{NaN, NaN}` | 不做 Payne-Hanek 级约化 |

这份合同还不是完整生产合同。当前受限原型选择把 domain-out（域外输入）、NaN 和 Inf 合并为 quiet NaN，且不静默 clamp（夹到合法角度）。进入生产接入前仍必须决定这个策略是否作为正式 helper 合同保留，还是改为 fallback（回退路径）、caller precondition（调用方前置条件）或分流到标量 libm。

domain-out/fallback 合同（域外输入如何处理：回退标量、返回 NaN，还是要求调用方保证输入合法）当前尚未作为生产合同冻结。RangeImageSpherical 的仅测试接入实验依赖调用方输入域证明，因此不会触发域外 lane（向量通道）；这能验证白名单调用方的可用性，但不能替代通用 helper 的域外策略设计。

---

## 3. 数学方案

### 3.1 Range reduction

第一版采用 bounded mask range reduction（有限域 mask 分段范围约化），不使用 `k = nearest(x * 2/pi)`，也不使用 `vfcvt_x_f`。这样可以避免 FRM/FCSR 依赖，并让标量同构链路与 RVV intrinsic（RVV 内建函数）链路更容易逐操作对齐。

在 `x in [-pi, pi]` 上分段：

| 输入区间 | 余项 `r` | `sin(x)` 重构 | `cos(x)` 重构 |
| --- | --- | --- | --- |
| `x > 3*pi/4` | `x - pi` | `-sin(r)` | `-cos(r)` |
| `pi/4 < x <= 3*pi/4` | `x - pi/2` | `cos(r)` | `-sin(r)` |
| `-pi/4 <= x <= pi/4` | `x` | `sin(r)` | `cos(r)` |
| `-3*pi/4 <= x < -pi/4` | `x + pi/2` | `-cos(r)` | `sin(r)` |
| `x < -3*pi/4` | `x + pi` | `-sin(r)` | `-cos(r)` |

该方案显式使用比较 mask 和 `vmerge`，不依赖负数 `%` 的 C/C++ 语义，也不需要计算负 quadrant 的整数模。

### 3.2 hi/lo 常量补偿

`pi` 与 `pi/2` 使用 float32 high part 加 low part 补偿：

- `pi_hi = float32(pi)`，`pi_lo = pi - pi_hi`
- `pi2_hi = float32(pi/2)`，`pi2_lo = pi/2 - pi2_hi`

边界点实验显示，single-constant reduction 在 `+-pi`、`+-pi/2` 等过零点附近会放大 ULP 诊断值；hi/lo split 明显降低 dense-chain 与 adversarial worst-case。当前候选使用 hi/lo reduction。

### 3.3 Polynomial

reduced interval 为 `r in [-pi/4, pi/4]`。

- `sin(r)` 使用 odd polynomial：`sin(r) = r + r^3 * P(r^2)`。
- `cos(r)` 使用 even polynomial：`cos(r) = 1 + r^2 * Q(r^2)`。
- 标量与 RVV 均使用 float32 FMA Horner 链，保持同构链路可对拍。

### 3.4 helper 分层与受限接入原型

当前实现分为 `rvv_math.hpp` 受限原型和 `test-rvv` 驱动层。`rvv_math.hpp` 中的入口只验证 math_rvv 的 lane-level helper（单个 RVV 向量寄存器级 helper）形态；`test-rvv` 负责数组 strip-mining（分段处理）、候选对比、专项验收、调用方形态冒烟测试和性能测试。

分层如下：

- `sin_kernel_scalar` / `cos_kernel_scalar`：只处理已约化的 `r in [-pi/4, pi/4]`。它们是 reduced-domain polynomial kernel（约化区间多项式核函数），不能当作完整 `sinf` / `cosf` 使用。
- `sin_kernel_rvv` / `cos_kernel_rvv`：RVV 版约化区间多项式核函数，同样只接受约化后的 `r`，不包含 range reduction（范围约化）、象限重构或特殊值处理，不能等同于 `sinf_RVV_f32m2` / `cosf_RVV_f32m2`。
- `scalar_sincos_finite_domain`：完整的标量同构链路 helper，包含 domain check（输入域检查）、有限域 mask 分段范围约化、hi/lo compensation（高低位常量补偿）、sign/swap reconstruction（符号/交换重构）、带符号零处理和域外 NaN 合并。
- `pcl::sincos_finite_domain_RVV_f32m2(x, s, c, vl)`：位于 `rvv_math.hpp` 的受限 lane-level integration prototype（单个 RVV 向量寄存器级生产接入原型）。它输入 RVV vector register（向量寄存器）与 `vl`，输出 `sin` / `cos` 向量寄存器，不负责 `std::vector` load/store，也不负责 strip-mining。该入口固定当前最佳候选 `lp-abs-hi-lo`，包含输入域 mask、有限域 mask 分段范围约化、高低位常量补偿、符号/交换重构、带符号零合并和域外 NaN 合并。名称显式包含 `finite_domain`，避免伪装成 strict libm replacement（严格 libm 替换）。
- `rvv_sincos_finite_domain(xs, s, c)`：仅测试使用的批量包装层，只负责 strip-mining 和 load/store，并调用 `sincos_finite_domain_RVV_f32m2`。它不是未来 `rvv_math.hpp` 的核心入口，只是测试驱动层。
- `sincos_finite_domain_RVV_f32m2_experimental` / `rvv_sincos_finite_domain_experimental`：只用于保留 Taylor/LP 多候选 RVV 对拍。`ApproxConfig` 不进入接近生产形态的 lane-level helper。
- `scalar_sin_finite_domain` / `scalar_cos_finite_domain`、`rvv_sin_finite_domain` / `rvv_cos_finite_domain`：single wrapper（单函数包装），只是复用 paired helper 的仅测试入口形态；名称保留 `finite_domain`，避免伪装成严格 libm 替换。

`sincos_test.cpp` 中的 `run_rvv_scratch` 只保留为测试薄包装。`lp-abs-hi-lo` 路径内部通过 `rvv_sincos_finite_domain` 调到 `pcl::sincos_finite_domain_RVV_f32m2`；其它候选只走 experimental path（实验候选路径）做对拍。

`sincos_finite_domain.hpp` 中的分层边界如下：

- kernel（约化区间多项式核函数）只接受 reduced interval（约化区间），不做合同检查。
- full finite-domain helper 负责 domain mask、bounded mask range reduction、hi/lo compensation、sign/swap reconstruction、signed-zero merge 和 domain-out NaN merge。
- lane-level RVV helper 不负责 `std::vector`、load/store 或 strip-mining。
- batch wrapper（批量包装层）只服务 test-rvv 输入数组。
- experimental path（实验候选路径）只服务 Taylor/LP 对比，不代表未来 `rvv_math.hpp` 的核心入口。

候选：

- `taylor`：Taylor baseline，用于第一版闭环和回归比较。
- `taylor-hi-lo`：Taylor 系数配 hi/lo reduction。
- `lp-abs`：使用 `scipy.optimize.linprog` 在等距 grid 上求 absolute-error LP/minimax-on-grid 候选。
- `lp-abs-hi-lo`：LP 系数配 hi/lo reduction，当前最合理候选。

选择 `lp-abs-hi-lo` 的理由：

- 参数脚本中，它在 dense-chain `[-pi, pi]`、dense-chain `[-pi/2, pi/2]` 和 adversarial points 上相对 Taylor baseline 有更好的 max/mean 组合。
- C++ 标量/RVV 同构链路对拍为 `0`，没有引入 RVV 路径差异。
- RVV 快速路径与 Taylor-hi-lo 的结构成本基本一致：仍是同阶 odd/even polynomial 和同一组 mask/merge。
- 它仍然是实验候选，不是 production-ready（可进入生产）的系数结论；Remez/full-chain minimax 尚未形成对照证据。

---

## 4. 证据矩阵

| 证据 | 当前状态 | 含义 | 限制 |
| --- | --- | --- | --- |
| `parms_sincos` | PASS | 生成 Taylor 与 `lp-abs`，报告 kernel、dense-chain、float32 量化后误差 | 脚本不是 RVV intrinsic 链路 |
| x86 correctness | PASS | 标量同构链路可编译运行，dense/adversarial/special 验收通过 | 无 RVV 执行链路 |
| QEMU/RVV correctness | PASS | RISC-V 编译、RVV intrinsic 链路、特殊值验收通过 | QEMU timing 不能作为性能证据 |
| scalar/RVV same-chain | PASS, max abs diff = `0` | 当前 RVV 操作链与标量同构链路对齐 | 只对实验测试覆盖的输入和候选成立 |
| special/signed-zero | PASS | NaN/domain-out 分类、带符号零合同可被验收条件捕获 | 生产特殊值策略尚未定稿 |
| RangeImageSpherical smoke | PASS | 调用方形态 angle/range 下游误差与域覆盖通过；single-entry local smoke 与标量批量路径一致 | 只闭合 spherical 形态，不闭合 base RangeImage |
| RangeImageSpherical 仅测试接入 | PASS | 测试专用头文件覆盖中的 `calculate3DPoint` 原型在 QEMU 上命中 `pcl::sincos_finite_domain_RVV_f32m2`，并与标量参考链路比较 xyz 误差 | 这是 `test-rvv` 接入原型；production RangeImageSpherical 头文件保持不变 |
| 反汇编快速路径 | PASS | 针对 `pcl::sincos_finite_domain_RVV_f32m2` 受限原型复查：可见 `vsetvli`、`vle32`、`vse32`、`vfmacc`、mask compare、`vmerge`、hi/lo `vfadd/vfsub`；快速路径未发现 libm 调用 | `sin/cos/sinf/cosf/sincos` 符号存在于参考/性能测试路径 |
| 板卡 microbench | PASS, 实验性能信号为正 | 受限 `rvv_math.hpp` 原型后复跑：RVV `lp-abs-hi-lo` 相比 float-libm median 约 `5.79x`，相比标量同构链路 median 约 `3.38x` | 板卡结果是实验微基准，不等于生产接入门禁 |

本轮阶段性复核结果：

```bash
make -C test-rvv/rvv/math parms_sincos
make -C test-rvv/rvv/math ARCH=x86 run_sincos_test
make -C test-rvv/rvv/math run_sincos_test
make -C test-rvv/rvv/math run_sincos_range_smoke
git diff --check
```

均通过。受限 `rvv_math.hpp` 原型后的板卡 repeat microbench 显示 RVV `lp-abs-hi-lo`：

- 相对 float-libm `sin/cos`：min `5.831x`，median `5.787x`。
- 相对标量同构链路 `lp-abs-hi-lo`：min `3.396x`，median `3.375x`。

该结果说明当前 math_rvv lane-level prototype 值得保留为受限接入原型，不表示生产接入门禁已闭合。

反汇编刷新结果：

- `pcl::sincos_finite_domain_RVV_f32m2` 在 `-O3` 下被内联到测试调用链，因此按快速循环的指令特征确认。
- 二进制中统计到 `vsetvli`、`vle32.v`、`vse32.v`、`vfmacc.vv`、`vmf*` compare、`vmerge.vvm`、`vfadd.vf`、`vfsub.vf` / `vfsub.vv`。
- 快速循环中可见 bounded mask reduction、hi/lo add/sub、polynomial FMA、sign/swap merge、zero/domain merge、load/store。
- `nm` 中仍有 `sin`、`cos`、`sinf`、`cosf`、`sincos` 未定义符号；反汇编定位显示这些调用位于 double-ref error 统计和性能测试基线，不在 RVV 快速路径中。
- RangeImageSpherical 仅测试接入二进制中，`calculate3DPoint` 的 `vl==2` 快速分支可见 RVV load/store、mask、merge、FMA 和 hi/lo add/sub。该原型不使用全局 hit counter（命中计数器）；`vl != 2` 异常路径直接产生 NaN 并使 xyz 验收失败，配合反汇编确认 RVV 快速路径存在，证明测试目标不会静默退回标量路径。

---

## 5. Caller smoke

### 5.1 RangeImageSpherical 输入域

`RangeImageSpherical::getAnglesFromImagePoint` 的公式为：

```cpp
angle_y = (image_y + image_offset_y_) * angular_resolution_y_ - pi/2;
angle_x = (image_x + image_offset_x_) * angular_resolution_x_ - pi;
```

full spherical 形态下，`angle_x` 可映射到 `[-pi, pi]`，`angle_y` 可映射到 `[-pi/2, pi/2]`。这正好落入当前 finite-domain `sinf/cosf` helper 合同。

`sincos_range_smoke.cpp` 镜像该角度/距离形态，但不调用 production `RangeImageSpherical` 或 `RangeImage`：

- `build_range_image_spherical_samples` 只生成 RangeImageSpherical 形态输入，公式与 `getAnglesFromImagePoint` 对齐；grid 为 `width=720`、`height=360`、`offset_x=0`、`offset_y=0`。
- range：`1`、`10`、`80`。
- 边界：覆盖 `0`、`+-pi/4`、`+-pi/2`、`+-pi` 附近的点。
- `calculate3DPoint_local_with_scratch_sincos(angle_x, angle_y, range)` 是一眼可读的调用方形态入口：它镜像 RangeImageSpherical local xyz 公式，只把 `std::sin` / `std::cos` 换成实验用的 finite-domain sincos helper。
- `calculate3DPoint_local_with_scratch_sincos_batch` 先批量调用标量实验 helper 得到 `sin/cos` 输出，再用这些输出计算 local xyz；不会在每个点上重复调用 helper。
- `calculate3DPoint_local_with_scratch_sincos_rvv_batch` 在 `__RVV10__` 下通过 `rvv_sincos_finite_domain`，间接调用 lane-level `sincos_finite_domain_RVV_f32m2`，计算同一 local xyz 形态。
- `check_single_entry_matches_batch` 抽取代表点，检查 single-entry local smoke 与标量批量输出完全一致。这个验收条件让读者能确认批量 smoke 没有偏离正常调用方形态单点入口。
- `compute_xyz` 只表达 local shape：`x=range*sin(angle_x)*cos(angle_y)`、`y=range*sin(angle_y)`、`z=range*cos(angle_x)*cos(angle_y)`。它不覆盖 production `to_world_system_` 变换。
- `update_reference_error` 用 `std::sin` / `std::cos` 参考链路计算 local xyz，并统计实验 helper 的下游误差。
- `update_rvv_diff` 只比较 RVV 实验链路与标量同构链路的 sincos 输出和 local xyz 输出。
- 验收条件：`angle_x in [-pi, pi]`，`angle_y in [-pi/2, pi/2]`，`domain_out == 0`，`in_contract == samples`，`max euclidean <= 1e-5`，RVV vs scalar same-chain xyz diff 为 `0`。

阶段性结果：

```text
samples=777789
in_contract=777789
domain_out=0
angle_x range=[-pi, pi]
angle_y range=[-pi/2, pi/2]
euclidean max=9.244339122e-06
RVV vs scalar same-chain sin/cos max |diff|=0
RVV xyz max euclidean diff=0
single-entry local smoke vs scalar batch max diff=0
```

### 5.2 RangeImageSpherical 仅测试接入验证

`sincos_range_image_spherical_integration_test.cpp` 使用 `test-rvv/rvv/math/sincos/prototype_include/pcl/range_image/impl/range_image_spherical.hpp` 做测试专用头文件覆盖。该头文件复刻 `RangeImageSpherical::calculate3DPoint` 的接入位置，并在 RVV 构建下调用当前 helper；`common/include/pcl/range_image/impl/range_image_spherical.hpp` 保持未接入状态。该实验不改变 public API，也不接 base `RangeImage`。

仅测试接入点位于测试专用头文件覆盖中的 `RangeImageSpherical::calculate3DPoint` 内：

- `__RVV10__` 有效时，函数把 `angle_x` 和 `angle_y` 放入一个 RVV vector register，用 `pcl::sincos_finite_domain_RVV_f32m2` 同时计算两组 `sin/cos`。
- `angle_x` 与 `angle_y` 仍来自真实 `RangeImageSpherical::getAnglesFromImagePoint`，输入域分别落在 `[-pi, pi]` 与 `[-pi/2, pi/2]`。
- xyz 公式仍是 `x=range*sin(angle_x)*cos(angle_y)`、`y=range*sin(angle_y)`、`z=range*cos(angle_x)*cos(angle_y)`，之后继续执行原有 `to_world_system_ * point`，因此不改变 `to_world_system_` 语义。
- 该实验没有接 base `RangeImage`，也不覆盖 base `RangeImage` 的 `angle_x / cos(angle_y)` 放大路径。

验证入口使用真实 `RangeImageSpherical` 对象，但方法体来自测试专用头文件覆盖。标量参考链路使用 `getAnglesFromImagePoint` 加 `std::sin/std::cos` 复现同一 local xyz 公式，并应用 `getTransformationToWorldSystem()`。RVV 构建下，该原型没有静默标量 fallback（回退路径）：`vl != 2` 会返回 NaN 并使验收失败；同时使用反汇编确认二进制中存在 RVV 快速路径。

阶段性结果：

```text
samples=87630
in_contract=87630
domain_out=0
angle_x range=[-pi, pi]
angle_y range=[-pi/2, pi/2]
xyz euclidean max=1.525878906e-05
xyz euclidean gate=2.0e-05
QEMU/RVV: PASS
```

该验收条件使用 `2e-5`，因为它验证真实 `Eigen::Vector3f` 单点路径和 `to_world_system_` 应用后的 xyz 输出，不是数学专项误差阈值。数学专项误差仍由 `sincos_test.cpp` 的 dense/adversarial/special 验收管理。

### 5.3 base RangeImage 未闭合

base `RangeImage::getAnglesFromImagePoint` 的公式不同：

```cpp
angle_y = (image_y + image_offset_y_) * angular_resolution_y_ - pi/2;
float cos_angle_y = std::cos(angle_y);
angle_x = (cos_angle_y == 0.0f ? 0.0f
           : ((image_x + image_offset_x_) * angular_resolution_x_ - pi) / cos_angle_y);
```

当 `angle_y` 接近 `+-pi/2` 时，`cos(angle_y)` 很小，`angle_x` 可能被除法放大到 `[-pi, pi]` 之外。因此 RangeImageSpherical smoke 和仅测试接入验证不能自动代表 base RangeImage，也不能闭合所有 RangeImage 生产接入门禁。

Caller smoke（调用方形态冒烟测试）只能证明某个下游形态的风险；它不能替代参数脚本、dense/adversarial/special 数学专项测试，也不能替代标量/RVV 同构链路对拍。

---

## 6. Production Gate 状态

Production：No。

Production integration prototype（生产接入原型）：Yes, limited。

当前允许进入受限生产接入评估。这里的生产接入评估只表示可以围绕受限 helper、调用方白名单和回退策略写设计与实验代码；它不表示可以默认接入所有 PCL 调用方，也不表示可以把 `test-rvv` 接入原型当作最终实现。

Production gate（生产接入门禁）未闭合项如下。

- `rvv_math.hpp` 中的 `pcl::sincos_finite_domain_RVV_f32m2` 仍是受限原型。它已经具备 lane-level helper 形态，并且被数学测试和 RangeImageSpherical 仅测试接入测试调用；当前没有闭合为 production helper，是因为它还缺少正式命名、调用方准入规则和域外策略评审。完成这一项可以降低“实验 helper 被误当作通用 libm 替换”的风险。当前阶段必须保留受限标识，但不必须立刻改成最终 production 入口。

- helper 命名/包装策略（paired sincos 与单独 sin/cos 的组织方式）尚未定稿。当前实现以 paired `sincos` 为核心，测试层提供单独 `sin` / `cos` 包装；这没有闭合为 production 设计，是因为 PCL 真实调用方可能既有成对调用，也有单函数调用。完成这一项可以证明调用方式接近 `std::sin` / `std::cos`，同时避免重复 range reduction。当前阶段不必须完成，但进入正式 production patch 前必须完成。

- domain-out/fallback 合同尚未定稿。当前 helper 对 NaN、Inf 和 `[-pi, pi]` 外输入返回 quiet NaN；RangeImageSpherical 仅测试接入依赖输入域证明，因此不会触发该策略。这一项没有闭合，是因为通用 helper 可能需要回退标量 libm、返回 NaN，或要求 caller 保证合法输入。完成这一项可以降低域外角度被错误静默处理的风险。当前阶段对 RangeImageSpherical 评估不是必须完成，但进入通用 production helper 前必须完成。

- caller 白名单（允许使用该 helper 的真实调用方集合）目前只包含 RangeImageSpherical 候选。它还没有闭合为 production 白名单，是因为 base RangeImage 的输入域不同，`angle_x` 可能被 `cos(angle_y)` 除法放大到 `[-pi, pi]` 之外。完成这一项可以证明每个接入点都有输入域证据，不会把有限域 helper 用到任意周期输入上。当前阶段必须明确只评估 RangeImageSpherical，不能扩展到 base RangeImage。

- RangeImageSpherical 仅测试接入已经通过 QEMU correctness，但还不是建议保留的生产代码。它没有闭合为 production，是因为当前实现位于 `test-rvv` 的测试专用头文件覆盖中，只服务验证；同时单点 `calculate3DPoint` 使用 `vl=2`，性能价值还没有通过真实调用方批量场景证明。完成这一项可以证明真实调用方收益和维护成本是否匹配。当前阶段必须把它作为实验代码对待，建议只保留仅测试使用的 prototype 与文档证据，除非另开正式 production patch。

- 反汇编与板卡证据已经刷新，但仍是原型证据。它没有闭合生产接入门禁，是因为反汇编只证明当前二进制快速路径没有意外 libm 调用，板卡微基准只证明实验性能信号。完成正式生产评审时需要对最终接入代码重新跑这些证据。当前阶段必须记录结果，但不必须继续做新的系数优化或扩大输入域。

允许设计的范围：

- 仅设计 finite-domain paired `sinf/cosf` helper。
- 明确只服务输入域可证明落在 `[-pi, pi]` 的 caller。
- 明确 `[-pi/2, pi/2]` 是重要子域，而不是唯一合同。
- 将 base RangeImage 排除，或为其设计 fallback/domain guard。
- 不声明为 strict libm replacement。

有限 production design 至少需要明确：

- production 入口与命名：是否设计 paired 内部 helper，以及单独 `sin` / `cos` 入口如何复用它。
- domain-out 策略：fallback、NaN merge、caller precondition 或分流，不能静默 clamp。
- caller 白名单与输入域证明，至少明确 RangeImageSpherical 与 base RangeImage 的区别。
- 生产接入原型后的 RVV 执行链路反汇编和板卡复跑。
- 文档同步：production 接入后再写 common helper 实现说明，不能只引用 scratch 证据。

---

## 7. 可复现命令

参数脚本：

```bash
make -C test-rvv/rvv/math parms_sincos
```

x86 scalar correctness：

```bash
make -C test-rvv/rvv/math ARCH=x86 run_sincos_test
```

QEMU/RVV correctness：

```bash
make -C test-rvv/rvv/math run_sincos_test
```

RangeImageSpherical-shaped smoke：

```bash
make -C test-rvv/rvv/math run_sincos_range_smoke
```

RangeImageSpherical 仅测试接入验证：

```bash
make -C test-rvv/rvv/math run_sincos_range_image_spherical_integration_test
```

本机或 QEMU benchmark 入口，注意 QEMU timing 只作结构信号：

```bash
make -C test-rvv/rvv/math run_sincos_bench
```

板卡部署与运行：

```bash
make -C test-rvv/rvv/math deploy_sincos_test
ssh <board> 'cd /root/pcl-test/rvv/math && make -f board.mk run_sincos_test'
ssh <board> 'cd /root/pcl-test/rvv/math && make -f board.mk run_sincos_bench'
```

RangeImageSpherical smoke 的板卡运行：

```bash
make -C test-rvv/rvv/math deploy_sincos_range_smoke
ssh <board> 'cd /root/pcl-test/rvv/math && make -f board.mk run_sincos_range_smoke'
```

RangeImageSpherical 仅测试接入验证的板卡运行：

```bash
make -C test-rvv/rvv/math deploy_sincos_range_image_spherical_integration_test
ssh <board> 'cd /root/pcl-test/rvv/math && make -f board.mk run_sincos_range_image_spherical_integration_test'
```

代码格式检查：

```bash
git diff --check
```

清理：

```bash
make -C test-rvv/rvv/math ARCH=x86 clean_sincos
make -C test-rvv/rvv/math clean_sincos
rm -rf test-rvv/rvv/math/.venv
```
