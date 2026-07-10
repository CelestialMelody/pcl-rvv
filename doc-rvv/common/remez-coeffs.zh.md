# RVV 常用超越函数

本文档汇总 `test-rvv/common/common/script/` 下各 Remez / minimax 相关脚本的数学含义、推荐用法与常见误区，便于与 `common/include/pcl/common/impl/common.hpp` 中的 float 实现对照。

---

## 1. 总览：三种「求系数」层次

| 层次 | 含义 | 典型工具 | 与 float 工程实现 |
|------|------|----------|-------------------|
| 离散 L∞（LP） | 在区间 [a,b] 的稠密栅格上最小化 \(\max_k \lvert f(t_k)-P(t_k)\rvert\)（\(\max_k\) 为对所有采样点取最大），约束为线性 | `scipy.optimize.linprog`（HiGHS），见 `script/lp_minimax.py` | 与连续 minimax 极接近；在 atan、log(1+u) 上已验证与文献/文章常数一致或更稳 |
| Remez（分两类，见下表 §1.1） | **第一算法风格**：在参考点集上解交替线性方程组，按误差振荡更新参考点（交换法）。**第二算法 / 数值代理**：在密栅上直接优化 \(\max \lvert f-P\rvert\)（无显式交换点迭代） | **remez1**：交换 + `numpy.linalg.solve`（`parms_atan2.remez_atan_odd_first`、`parms_acos.fit_reduced_remez1`）。**remez2**：`scipy.optimize.minimize`（Powell）+ 密栅，初值常取 LP | remez1 便于对照教材；atan 在 \([0,1]\) 上奇次核 remez1 典型 max 绝对误差约 **\(10^{-4}\,\mathrm{rad}\)** 量级，remez2/LP 约 **\(10^{-6}\,\mathrm{rad}\)**。acos 约化 \(\sqrt{1-x}\,Q(1-x)\) 时 remez1 在 float64 拟合下可极接近 minimax（与 remez2 同阶），低次多项式上二者差异更明显 |
| 连续 minimax | 在实数区间上针对指定基（单项式、奇次单项式等）的逼近 | Sollya `fpminimax` 等 | 与 LP 在双精度上常对齐到很多位；不随仓库分发，需本机安装 |

### 1.1 Remez：交换法（remez1）与密栅优化（remez2）

下表与 `parms_atan2.py`、`parms_acos.py` 中 `--method` 命名一致。

| 项 | remez1（第一算法风格 / 交换法） | remez2（第二算法的常用数值替代） |
|----|--------------------------------|----------------------------------|
| **核心操作** | 在 \(n+2\) 个参考点上令误差等幅交替，解 \((n+2)\times(n+2)\) 线性方程组（含偏差未知数 \(\pm E\)）；在密栅上找误差极值、按符号交替挑选新参考点并替换 | 在固定密栅上把 \(\max \lvert f-P\rvert\) 当作目标，对系数用 Powell 等无导数法下降；初值多为同区间上的 LP 解 |
| **atan 核**（`parms_atan2.py`） | \(P(t)=t\cdot\sum_k a_{2k+1} t^{2k}\)，\([0,1]\)；初值参考点为 **\(t\)** 区间上的 Chebyshev 型节点；实现：`remez_atan_odd_first`（内部调用历史名 `remez_atan_odd_first_algorithm_legacy`，默认 `a=0` 覆盖全区间） | 同上奇次形式；`remez_atan_odd_second` |
| **acos 约化**（`parms_acos.py`） | \(u=1-x\)，\(\mathrm{acos}(x)\approx \sqrt{u}\,Q(u)\)，\(Q\) 为 \(u\) 的 \(d\) 次多项式；`fit_reduced_remez1` | `fit_reduced_remez2`（Powell + 密栅，初值 LP） |
| **CLI** | `--method remez1`；acos 另有 `remez1-deg7`、`remez1-deg5` | `--method remez2`；acos 另有 `remez2-deg7`、`remez2-deg5` |
| **依赖** | 仅需 **numpy**（解稠密阵） | **scipy**（`minimize`） |
| **注意** | 交换失败时会退回均匀参考点， atan 上易变差；工程主路径仍推荐 remez2 或 LP | 与「教科书 Remez 第二算法」目标一致，实现上避开了病态的牛顿解方程组 |

实践建议

- atan2 / `parms_atan2.py`（推荐通读）：默认 `python script/parms_atan2.py` 打印**六路**对照：(1) [mazzo.li / vectorized atan2](https://mazzo.li/posts/vectorized-atan2.html) 文章常数（注释）；(2) **Remez1** 交换法（`--method remez1`）；(3) **Remez2**（Powell + 密栅）；(4) 离散 LP；(5)(6) Sollya 全次数 5 / 11（Horner `c0..`；与 (2)–(4) 的 \(t\cdot(a_1+a_3 t^2+\cdots)\) 形式不同）。六路快照写在脚本顶部注释块；Sollya deg11 用 `--run-sollya`，deg5 用 `--run-sollya-deg5`。`atan2_test.cpp` 为标量 **(1)–(6)** + **(7) RVV**（系数同 (1)）。Sollya 路径在负 `t=y/x` 上按 \(\mathrm{sign}(t)\cdot P(\lvert t\rvert)\) 对 `atan` 做奇延拓。
- log(1+u)：在 \([0,1]\) 上逼近光滑，`common.hpp` 中 `kLogfLog1pC0..C7` 当前保留历史 LP-derived baseline。注意 `parms_log1p.py` 当前 report 中 `(1) baseline/current` 与 `(4) lp` 是不同候选，不应把 current/common.hpp baseline 等同于当前 `--method lp` 输出。本轮 QEMU/板卡复核后暂不替换 logf 系数，理由见下方 logf 补充。
- exp(r) 默认在 [-ln2/2, ln2/2]：与 expf 的 `n=round(x/ln2)` 约化一致。`parms_expf.py` 提供 `remez1`（绝对误差，第一算法风格交换实现）以及 `remez1-rel` / `remez2-rel` / `lp-rel`（相对误差目标），另有 `sollya-script/sollya-run`。当前 `common.hpp` 采用 `remez1-rel`；板卡 `run_expf_test` 专项网格实测 `max rel = 2.234380e-07`，显著优于旧 baseline 的同网格结果 `1.517213e-06`。
- acos：`parms_acos.py` 默认**四路** report：(1) PCL sqrt 八常数基线；(2) 约化模型 `acos(x) ~= sqrt(1-x)*Q(1-x)` 的 **remez1**（交换法，`fit_reduced_remez1`）；(3) 同模型的 **remez2**；(4) 离散 LP。`--powell-grid` 同时用作 remez1 迭代中的误差密栅长度与 remez2 目标栅格。单跑 remez1 仅需 numpy；默认 report 仍依赖 scipy（remez2 与 LP）。`x_hi` 与 `acos_test.cpp` 的 `k_x_hi`、脚本 `--x-hi` 一致（默认 `0.999`）。C++ `acos_test.cpp`：标量 **(1)–(10)**（历史 PCL + deg11/7/5 × remez1/remez2/LP），RVV **(11)–(13)**；`common.hpp` 当前默认采用 **deg5 remez2** reduced 形式，deg11 的 remez1 在 C++ 侧用**稠密** Horner（`q0..q11` 全用），与 remez2 所用稀疏结构区分。
- acos（模型说明）：`lp_minimax.minimax_polynomial_lp` 与报告中的 remez2 初值都要求逼近式对所求系数是线性的（单项式或给定基下的线性组合）。PCL 的 `(a0+x(a1+x a2))√(b0+b1 x)+(c0+x(c1+x c2))` 对八个参数是非线性的，不能原样塞进上述 LP；因此改用线性可解的约化模型 `sqrt(1-x)*Q(1-x)`。若希望「泰勒式」少用系数，可在区间上固定解析形状，仅拟合少量参数；得到的仍是 minimax / LP 系数，不是截断泰勒的解析系数。

---

## 2. 为何 exp 的系数优化目标要看 relative

若目标是 `expf` 端到端相对误差，推荐直接优化 relative 指标（`max |e^r-P(r)| / e^r`）。仅优化 absolute 指标（`max |e^r-P(r)|`）时，float Horner 与重构链路上的相对误差不一定更好。

具体原因可以分几条看：

1. 区间一致性：应在 \([-\ln 2/2,\ln 2/2]\) 上拟合，匹配 `n=round(x/ln2)` 后的余项区间；若用 \([0,\ln2]\) 会引入负半区失配。
2. 病态 Vandermonde：在单项式基 \(1,r,\ldots,r^7\) 下，系数对舍入敏感，不同求解器/栅格密度可能得到不同系数族。
3. 目标一致性：硬件验收更看 `float Horner + 2^n` 重构后的 relative 误差，因此 `remez1-rel/remez2-rel/lp-rel` 与验收目标更一致。`parms_expf.py` 在 report 中会同时打印 `f32 Horner` 与“真实链路 f32 仿真”指标。
4. 历史同源：`remez1`（绝对误差）与当前头文件系数同族，常作为基线保留。

结论：exp 的候选系数优先比较 relative 指标，并以真实链路仿真/板卡结果做最终准入；absolute 指标仅作旁证。

当前 `common.hpp` 采用的 expf 系数（`parms_expf.py` 的 `remez1-rel`）：

```cpp
static const float kExpfRemezC0 = 0.9999999999876557f;
static const float kExpfRemezC1 = 1.000000000027863f;
static const float kExpfRemezC2 = 0.5000000053614374f;
static const float kExpfRemezC3 = 0.16666666439294f;
static const float kExpfRemezC4 = 0.04166635362288752f;
static const float kExpfRemezC5 = 0.008333359419394903f;
static const float kExpfRemezC6 = 0.001394106053653905f;
static const float kExpfRemezC7 = 0.0001986611354469939f;
```

补充（板卡 `run_expf_test` 专项网格）：

- 精度：当前 `common.hpp` / `remez1-rel` 的 `max rel = 2.234380e-07`，`mean abs = 6.639316e+27`；旧 baseline 在同网格下为 `max rel = 1.517213e-06`。`parms_expf.py` 另输出 dense chain f32 仿真指标，例如当前系数 `max relative error (f32 chain, 200k x in [-88,88]) = 3.891403569248383e-06`，两者测试口径不同。
- 一致性：`[pcl::expf_RVV vs 标量 (1)]` 与 `[RVV-* vs scalar-*] max |diff|` 全部为 `0`，说明当前 RVV 与标量路径在该测试网格上数值一致。
- 性能：选择阶段板卡上 `remez1-rel` 候选耗时 `4.342 ms`，约 `9.73x vs std`；旧 baseline 耗时 `4.500 ms`，约 `9.39x vs std`。替换后 direct `pcl::expf_RVV_f32m2` 复测保持在 `4.5 ms` 量级，性能与旧 baseline 同阶。标量各系数方案耗时几乎一致；收益主要来自 RVV 向量化、FMA 与 `2^n` 位构造。

补充（acos 当前采用 deg5 remez2）：

- 参数：`q0=1.414212408248559`、`q1=0.117926522053977`、`q2=0.02571508511147162`、`q3=0.01095480727067022`、`q4=-0.002360310714948563`、`q5=0.004346735271181379`。
- QEMU 专项复核：当前 `pcl::acos_RVV_f32m2` 最大误差 `1.311302e-06 rad`，与标量 deg5 remez2 的 `max |diff|` 为 `0`；历史 PCL 八常数 baseline 最大误差 `7.749423e-04 rad`。
- 板卡专项复核：当前 `pcl::acos_RVV_f32m2` 最大误差 `1.311302e-06 rad`，与标量 deg5 remez2 的 `max |diff|` 为 `0`；计时 `29.203 ms`，`17.01x vs std`。同次测试中 RVV deg7 remez2 最大误差 `2.384186e-07 rad`，速度约为当前 common.hpp 的 `0.89x`。

补充（logf 当前保留 current/common.hpp baseline）：

- 口径：`common.hpp` 的 `kLogfLog1pC0..C7` 是历史 LP-derived baseline；`parms_log1p.py` 当前 `(4) lp` 是另一个候选。
- 精度（QEMU/板卡 `run_logf_test` 专项网格一致）：current/common.hpp `max rel = 6.739247e-05`、`mean abs = 1.585479e-07`；remez1 `max rel = 4.477345e-05`、`mean abs = 2.058984e-07`；lp `max rel = 5.738253e-05`、`mean abs = 1.610150e-07`。
- 性能：板卡 direct `pcl::logf_RVV_f32m2` 耗时 `9.031 ms`，约 `3.99x vs std`；候选 RVV 路径性能差异较小。
- 下游：norms 的 QEMU/板卡 `run_test_rvv` 与 `run_test_std_vs_rvv_compare` 均通过，覆盖 `Div_Norm` / `KL_Norm` 的当前 baseline。
- 结论：remez1 虽降低 max rel，但 mean abs 变大；lp 也不是 current baseline 且 mean abs 略高。收益不足以抵消影响 `Div_Norm` / `KL_Norm` 容差和文档口径的风险，因此保留 current/common.hpp baseline。

对「约化 + 核函数」的进一步优化（不互相排斥）：

- 持续改进 **atan remez1**（`remez_atan_odd_first`）的交换点筛选与收敛判据（例如波瓣分段 + 极值窗口），可进一步提升稳定性；目前主实现已采用稳健交换策略，历史退化逻辑仅保留在 `remez_atan_odd_first_algorithm_legacy` 供对照。
- 对 exp / log1p 的「单项式 P」：用 Sollya `fpminimax` 在闭区间上求连续意义下的多项式，再量化到 `float`（仓库内已加示例 `.sollya` 文件，见下节）。
- 在 Chebyshev 基或缩放变量下做 LP，再把系数变回 `r^i` 或 `u^i` 形式，减轻 Vandermonde 病态（需额外实现与验证）。

---

## 3. 如何运行脚本并对比不同方案的精度

环境（在 `test-rvv/common/common` 下）：

```bash
cd test-rvv/common/common
uv venv .venv && uv pip install numpy scipy
# 或:  .venv/bin/python  直接调用 script/ 下各文件
```

也可用 Makefile（同样目录）：

```bash
make parms_atan2 parms_log1p parms_expf parms_acos
```

对比时：`parms_atan2.py` 默认六路 report；单路可用 `--method remez1` / `lp` / `remez2`。

| 脚本 | 对比什么 | 常用命令 |
|------|----------|----------|
| `parms_atan2.py` | **六路** report 或单路 **remez1** / `lp` / `remez2` | `.venv/bin/python script/parms_atan2.py`；`... --method remez1` / `lp` / `remez2` |
| `parms_log1p.py` | `lp` / `remez1` / `remez2` / `sollya` 五路报告 | `.venv/bin/python script/parms_log1p.py`；单路 `... --method lp` / `remez1` / `remez2` |
| `parms_expf.py` | `remez1` / `remez1-rel` / `remez2-rel` / `lp-rel` / `sollya-script` | `.venv/bin/python script/parms_expf.py`；单路 `... --method remez1-rel` / `remez2-rel` / `lp-rel` |
| `parms_acos.py` | **四路** report 或单路 **remez1** / `lp` / `remez2` / `pcl-baseline`；快捷 `report-deg7` 等 | `python script/parms_acos.py`；`--method remez1`；`remez1-deg7` / `remez1-deg5` |

atan 的 remez1 与历史实现 `remez_atan_odd_first_algorithm_legacy` 为同一算法族；正式入口为 **`remez_atan_odd_first`**（CLI：`--method remez1`）。在 \([0,1]\)、`a=0` 上奇次到 \(t^{11}\) 时，当前稳健版典型 max 误差可到 **\(10^{-6}\,\mathrm{rad}\)** 量级；历史版（legacy）仍可复现约 \(10^{-4}\,\mathrm{rad}\) 的退化行为，适合教学对比。

与 `std::expf` / `std::logf` / `std::atan2` / `std::acos` 的端到端差异，请用仓库里已有测试（如 `expf_test`、`logf_test`、`atan2_test`、`acos_test`）在目标架构上跑，脚本输出的是「多项式核」本身的误差上界，不是整条向量函数在 libm 下的 ulp 报告。

---

## 4. 脚本与公共工具模块

| 文件 | 作用 |
|------|------|
| `lp_minimax.py` | 通用 `minimax_polynomial_lp`（离散 L∞ / LP） |
| `sollya_utils.py` | `SOLLYA_ATAN_*`、Sollya 运行/解析与报告辅助 |
| `parms_atan2.py` | 默认**六路** report；`--method remez1` / `lp` / `remez2`；`--run-sollya`、`--run-sollya-deg5`；`remez_atan_odd_first` / `remez_atan_odd_first_algorithm_legacy` |
| `parms_log1p.py` | `log(1+u)`，默认 `report`（含 baseline/remez1/remez2/lp/sollya） |
| `parms_expf.py` | `exp(r)`，默认 `report`（含 baseline/remez1/remez1-rel/remez2-rel/lp-rel/sollya，区间默认 [-ln2/2, ln2/2]） |
| `parms_acos.py` | 默认**四路** report（PCL / remez1 / remez2 / LP）；`fit_reduced_remez1` 仅需 numpy；**report** 仍需 scipy |

依赖：`numpy` 必须。**`parms_atan2.py`**：`report` / `lp` / `remez2` 需 `scipy`；单跑 **`remez1` 仅需 numpy**（与 `parms_acos.py` 的 remez1 相同）。**`parms_acos.py`**：单跑 `remez1` 仅需 numpy；默认 `report` 需 scipy。

---

## 5. Sollya 示例（可选，不随 CI）

本机已测 Sollya 8.0，下列方式可得到 `fpminimax` 多项式。

```bash
cd test-rvv/common/common
# atan：六路 report；或仅 Sollya deg11 / deg5
.venv/bin/python script/parms_atan2.py
.venv/bin/python script/parms_atan2.py --run-sollya-deg5
.venv/bin/python script/parms_atan2.py --run-sollya --sollya-save script/sollya_atan_fpminimax_report.txt

# acos：四路（PCL8 / 约化 remez1 / 约化 remez2 / 约化 LP）
.venv/bin/python script/parms_acos.py
.venv/bin/python script/parms_acos.py --method remez1
.venv/bin/python script/parms_acos.py --method remez2
.venv/bin/python script/parms_acos.py --method lp

cd test-rvv/common/common/script
sollya sollya_fpminimax_exp.sollya          # exp(x)，x∈[-ln2/2,ln2/2]，7 次，relative
sollya sollya_fpminimax_log1p.sollya        # log(1+x)，x∈[0,0.999999]，7 次，absolute
sollya sollya_fpminimax_atan_deg11_absolute.sollya   # atan deg11（12 常数）
sollya sollya_fpminimax_atan_deg5_absolute.sollya    # atan deg5（6 常数，与六路 report 中 Sollya deg5 一致）
```

Sollya 进程退出码（与「算没算出来」无必然矛盾）：按 [Sollya 用户手册对交互进程的约定](https://www.sollya.org/sollya-current/sollya.php)（*Exit status of the sollya tool* 一节）：

- `0`：以 `quit` 正常结束，或只跑了 `--help` / `--version`。
- `3`：最后一条命令已解析并执行成功，但输入在 EOF 处结束、此前未执行 `quit`。从文件读入一段脚本、跑完即退出时，这就是预期行为，不是 `fpminimax` 失败。
- 若希望 shell 里看到 `0`，在 `.sollya` 文件末尾加一行 `quit;` 即可；对 `--run-sollya` 生成的报告而言，有 Horner 长行输出即可视为已得到多项式。

- 在 \([0,1]\) 上对仅含奇次项的 `atan` Hastings 型多项式，若用 Sollya 的 `fpminimax` 并限制为奇次单项式基，在 Sollya 8 上可能因非 Haar/奇异阵失败；工程上奇次因式与 LP（`parms_atan2.py --method lp`）或 mazzo 文章常数更稳。`parms_atan2.py --print-sollya` 仍打印一版可手工修改的试验片段，以 [Sollya 手册](https://sollya.gforge.inria.fr/) 为准。

---

## 6. 本仓库里的 C++ 回归测试怎么跑

在 `test-rvv/common/common` 下，先按默认 `ARCH`（本机常见为 `x86` 或需查看 `Makefile` 首段）编出可执行文件，再跑与 `libm` 对比的测试。示例（x86/本地 g++ 能编过时）：

```bash
cd test-rvv/common/common
make run_atan2_test    # 标量；RVV 需 ARCH=riscv 且链工具齐全
make run_acos_test
make run_expf_test
make run_logf_test
```

可选：`make run_expf_remez_vs_taylor`（Remez 与 Taylor 对比）。日志默认 tee 到 `output/run_*.log`（以 Makefile 中 `OUTPUT_DIR` 为准）。若交叉编译到 RISC-V 板卡，用仓库里已有的 `deploy_*` 与 `make run_* ARCH=riscv` 及 `board.mk` / QEMU 等流程，与 `logf-RVV.zh.md`、`expf-RVV.zh.md` 中板卡节一致。

---

## 7. 与《atan2 文章》及单元测试的衔接

- 标量/向量数值对照：`test-rvv/common/common/atan2_test.cpp`（文章、mazzo 奇次核、**Remez1**、**Remez2**、LP、Sollya deg5/deg11、RVV）；`acos_test.cpp`（PCL8、约化 **remez1 / remez2 / LP**（deg11/7/5）、RVV 约化 remez2）。
- `atan2` 与 RVV 的专题说明仍见 `doc-rvv/common/atan2-RVV.zh.md`；系数选择策略以本文与脚本头注释为准（文章常数可保留，LP 用于复核与回归）。

---

## 8. 参考

- Mazzo, *Vectorized atan2*, <https://mazzo.li/posts/vectorized-atan2.html>
- Sollya: <https://sollya.gforge.inria.fr/>
- SciPy `linprog`（HiGHS 后端）离散 Chebyshev / minimax 工程用法
