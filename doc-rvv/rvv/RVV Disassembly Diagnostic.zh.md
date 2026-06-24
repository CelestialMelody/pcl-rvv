# RVV 反汇编诊断操作手册

本文说明如何用反汇编把 PCL RVV 优化中的 correctness 差异收敛为可验证的因果链。它面向已经有 Std/RVV 测试、bench 或 production-shaped diagnostic 的主题；目标不是只证明“二进制里有 RVV 指令”，而是确认某个 case 是否命中了预期 helper、指令序列是否对齐标量语义、差异是否来自 FMA contraction、浮点转换、FRM/FCSR、`vcompress` 顺序、gather 布局或标量 tail 边界。

相关文档：

- [RVV Eigen 表达式语义对齐](RVV%20Eigen%20Expression%20Semantic%20Alignment.zh.md)
- [RVV Float-to-Int Rounding and FRM](RVV%20Float-to-Int%20Rounding%20and%20FRM.zh.md)
- [RVV 多字段压缩 staging 输出模式](RVV%20Multi-Field%20Compress%20Staging.zh.md)

## 1. 适用场景

出现以下任一现象时，应进入反汇编诊断：

- Std / RVV checksum、输出数量、首个差异元素或 ordered sequence 不一致；
- 单个 helper 测试通过，但 production-shaped diagnostic 或 production direct case 失败；
- QEMU correctness 失败，源码公式看起来相同；
- 差异集中在像素、voxel、bucket、threshold、`static_cast<int>`、floor 或 tie 附近；
- 手工展开了 Eigen 小矩阵、小向量、`getVector*fMap()` 或 `Vector3f::norm()`；
- 怀疑 `vfcvt`、`vcompress`、`vsetvli`、`vfred*`、`fsrm`、`frrm`、`frcsr` 等指令影响结果；
- 需要证明 production RVV helper 的真实路径命中，而不是只依赖源码分流判断。

如果问题只涉及性能，先看板卡 bench 和 profile。反汇编能证明路径、指令形态和语义线索，但不能替代真实硬件性能结论。

## 2. 诊断闭环

一次完整反汇编诊断应形成下面闭环：

```text
记录现象
-> 固定最小复现 / adversarial case
-> 确认入口和分流条件
-> 导出反汇编
-> 定位符号和关键指令窗口
-> 对照标量 lowering 与 RVV intrinsic
-> 修改 intrinsic / gate / fallback / tail
-> 复跑 test、bench checksum 和 asm
-> 写入主题文档与评估文档
```

每一步都要保留可复查的信息。不能只写“浮点误差导致失败”或“反汇编确认 RVV 命中”。

### 2.1 记录现象

先记录 case、入口层级、输出差异和构建环境。建议至少包含：

```text
case: ceop production non-identity fake-indices pointxyz 64K
entry: CorrespondenceEstimationOrganizedProjection::determineCorrespondences
std checksum: 13376866430852120216
rvv checksum: 13376866430852120216
output size: same / different
first mismatch: source_index / target_index / distance / predicate
build: std binary vs rvv binary
platform: QEMU / board
```

若输出是 sequence，例如 correspondences、indices、sampled indices 或 staging candidates，应同时记录输出数量和首个差异元素。checksum 只能告诉结果不同，不能告诉差异来自顺序、数量还是字段值。

### 2.2 固定最小复现

反汇编诊断应优先配合一个最小 adversarial case，而不是只依赖大规模随机 bench。

常见构造：

| 风险                       | 最小 case                                                                |
| -------------------------- | ------------------------------------------------------------------------ |
| FMA contraction            | 让 `fx*x + cx*z` 或 row-dot 结果接近整数 / 阈值边界。                  |
| `static_cast<int>` / RTZ | 让除法结果位于 `N - epsilon`、`N`、`N + epsilon` 附近。            |
| `Vector3f::norm()`       | 让 `float_norm` 等于或紧邻 `float(max_distance)`。                   |
| `vcompress` 顺序         | 构造 keep mask 类似 `[1,0,1,1,0]`，检查压缩后字段是否同 lane 对齐。    |
| gather layout              | 使用 `setIndices()` 的乱序、重复、非连续 subset 和不同点类型。         |
| fallback tail              | 构造前一阶段 RVV 成功、后一阶段因规模 / VLEN / traits gate 失败的 case。 |

最小 case 通过后，再回到 production direct 或 full diagnostic case 验证。

### 2.3 确认入口和 gate

在看 asm 前，先确认 case 应该命中哪一层：

- low-level helper；
- production-shaped diagnostic；
- production direct；
- fallback / staged fallback；
- RVV-only bit / threshold 诊断。

同时检查 gate：

- `__RVV10__` 是否定义；
- 点类型 traits 是否满足；
- `indices.size()`、`vlmax`、32-bit offset、organized width/height 是否满足；
- identity fast path 或 non-identity path；
- 是否在某个 staging 后回到 scalar tail。

反汇编只能解释二进制实际执行的代码。若 case 本来回退标量，就不应在 RVV helper 中寻找差异。

## 3. 导出与定位反汇编

### 3.1 生成 asm

主题目录通常提供：

```bash
make -C test-rvv/<module>/<topic> dump_bench_rvv
```

常见输出：

```text
test-rvv/<module>/<topic>/build/asm/riscv/bench_<topic>_rvv.full.asm
test-rvv/<module>/<topic>/build/asm/riscv/bench_<topic>_rvv.asm
```

`full.asm` 更适合定位完整符号和编译器插入路径；精简 asm 更适合人工阅读。

### 3.2 搜索符号

先按 helper 名定位，而不是直接从全部 `vfmacc` 里猜：

```bash
rg -n "projectOrganizedProjectionCandidatesRVV|projectOrganizedProjectionPixelsRVV|acceptProjectedOrganizedProjectionCandidatesRVV" \
  test-rvv/registration/correspondence_estimation_organized_projection/build/asm/riscv/bench_correspondence_estimation_organized_projection_rvv.full.asm
```

若符号被模板实例化或内联，搜索更短的稳定片段：

```bash
rg -n "OrganizedProjection|ProjectedOrganized|AcceptedOrganized|determineCorrespondences" \
  test-rvv/<module>/<topic>/build/asm/riscv/bench_<topic>_rvv.full.asm
```

### 3.3 搜索指令族

通用搜索：

```bash
rg -n "vfmacc|vfmul|vfadd|vfsqrt|vfcvt|vcompress|vcpop|vluxei32|vlse32|vlsseg|vsetvli|fsrmi|fsrm|frrm|frcsr|fscsr" \
  test-rvv/<module>/<topic>/build/asm/riscv/bench_<topic>_rvv.full.asm
```

按问题缩小范围：

```bash
# FMA / sqrt / convert
rg -n "fmadd|fmsub|fsqrt|fcvt|vfmacc|vfmul|vfadd|vfsqrt|vfcvt" build/asm/riscv/*.asm

# mask / compress / sequence
rg -n "vmand|vmor|vmnot|vcompress|vcpop|vfirst" build/asm/riscv/*.asm

# FRM / FCSR
rg -n "fsrmi|fsrm|frrm|frcsr|fscsr" build/asm/riscv/*.asm

# gather / layout
rg -n "vluxei32|vsuxei32|vlse32|vsse32|vlseg|vlsseg|vsseg|vssseg" build/asm/riscv/*.asm
```

## 4. 指令线索如何解释

| 指令 / 形态                                 | 诊断含义                           | 需要追问                                                 |
| ------------------------------------------- | ---------------------------------- | -------------------------------------------------------- |
| `fmadd.s` / `vfmacc.*`                  | fused multiply-add，乘加只舍入一次 | 标量和 RVV 是否都 fused，操作数顺序是否一致。            |
| `fmul` + `fadd` / `vfmul` + `vfadd` | 分离乘法和加法，多次舍入           | 是否与标量 lowering 一致，边界 predicate 是否受影响。    |
| `fsqrt.s` / `vfsqrt.v`                  | sqrt 的计算域通常是当前元素类型    | `Vector3f::norm()` 是否先得到 float，再拓宽为 double。 |
| `fcvt.d.s` / `vfwcvt`                   | float 结果拓宽                     | 谓词是 double(float_result) 还是 double 计算结果。       |
| `vfcvt.rtz.x.f.v`                         | 向 0 截断                          | 输入值是否已与标量一致；转换本身不能修复前序 FMA 差异。  |
| `fsrm` / `frrm` / `frcsr`             | 浮点环境读写                       | helper 是否保存恢复，是否污染后续 fallback。             |
| `vcompress.vm` + `vcpop.m`              | 按 mask 保序压缩                   | 多字段是否使用同一 mask 和同一 count。                   |
| `vsetvli ... e32,m2`                      | 当前 SEW/LMUL/tail policy          | 固定 buffer 的 `vlmax` gate 是否匹配。                 |
| `vluxei32.v`                              | 32-bit byte-offset gather          | offset、field layout、target_index 是否正确。            |
| `vfred*`                                  | 向量规约                           | 是否允许容差，是否改变顺序敏感输出。                     |

反汇编结论应写成“某个源码表达式对应某组指令”，例如：

```text
`projection_matrix_ * p_src3` 中 uv0 的 RVV 路径为 `vfmul.vf` 初始化 `z*cx`，
再用 `vfmacc.vf` 融合 `fx*x`，随后 `vfdiv.vv` 和 `vfcvt.rtz.x.f.v`。
这对齐标量路径中进入 `static_cast<int>` 前的 FMA contraction。
```

## 5. 把线索变成修复

### 5.1 FMA contraction 差异

现象：

- projection / transform / distance 公式源码相同，但边界 lane 输出不同；
- `static_cast<int>` 后像素差一格；
- 阈值附近输出数量不同。

诊断：

1. 查看标量路径是否有 `fmadd.s` / `fmsub.s`。
2. 查看 RVV 是否写成 `vfmul + vfadd`。
3. 构造像素或阈值边界 case。
4. 改成 `vfmacc` / `vfmsac` 或与标量相同的部分和结构。

CEOP projection-pixel 的修复就是该模式：早期 RVV 用分离乘加计算 `(fx*x + cx*z)`，QEMU 对拍在像素边界改变 correspondence 数量。反汇编确认标量存在 FMA contraction 后，RVV 改为：

```text
uv0 = z * cx
uv0 = vfmacc(uv0, fx, x)
uv1 = z * cy
uv1 = vfmacc(uv1, fy, y)
```

修复后 fake indices、`setIndices()` subset、identity / non-identity projection boundary case 都恢复一致。

### 5.2 `Vector3f::norm()` 计算域差异

现象：

- distance 阈值附近 Std/RVV 输出数量不同；
- 源码写成 `const double dist = (...).norm()`，容易误判为 double 域距离。

诊断：

1. 确认对象类型是 `Eigen::Vector3f` 还是 `Vector3d`。
2. 查标量反汇编是否为 `fmul.s + fmadd.s + fmadd.s + fsqrt.s + fcvt.d.s + flt.d`。
3. RVV distance predicate 应使用 `vfmul.vv + vfmacc.vv + vfmacc.vv + vfsqrt.v` 得到 float norm。
4. threshold 谓词保留 `double(float_norm) < max_distance`，不能简化为 `float_norm < float(max_distance)`。

CEOP production 采用 `<` / `<=` 分支处理 double threshold 边界：当 `double(float(max_distance)) < max_distance` 时，用 `<= float(max_distance)` 接受等于 float threshold 的 lane；否则用 `< float(max_distance)`。`DistanceRvvMatchesEigenNormBits` 和 `DistanceRvvThresholdPredicateMatchesDoubleScalarPredicate` 分别保护 bit pattern 与 predicate。

### 5.3 `vfcvt` 和 FRM/FCSR

现象：

- float-to-int 后 index / voxel / pixel 变化；
- 单独跑 fallback 正常，先跑某个 RVV case 后 fallback 异常；
- asm 中出现 `fsrmi`、`fsrm`、`frrm`。

诊断：

1. 区分转换输入差异和转换指令差异。`vfcvt.rtz.x.f.v` 只保证同一输入按向 0 截断。
2. 若使用 `_rm` 或显式修改 FRM，确认 helper 进入和退出时保存恢复。
3. 增加同进程序列测试：先跑 RVV 主路径，再跑 fallback / scalar case。
4. 文档记录是否读写 FRM/FCSR。

CEOP final helper 不使用 `_rm` intrinsic，不读写 FRM/FCSR；projection-pixel 使用 `vfcvt.rtz.x.f.v` 明确对齐 `static_cast<int>` 的向 0 截断。早期失败点在转换前的 FMA 输入值，而不是 RTZ 指令。

### 5.4 `vcompress` 顺序和多字段 staging

现象：

- 输出数量相同但顺序不同；
- 单字段 checksum 对齐，多字段字段错位；
- staging tail 读取到错误的 `source_index` / `target_index` / `x/y/z` 组合。

诊断：

1. 确认所有字段使用同一个 keep mask。
2. 确认 `vcpop` 的 count 同时用于所有字段 store。
3. 检查固定 buffer 是否有 `vlmax <= buffer_length` gate。
4. 构造稀疏 mask case，检查压缩后第 0、1、2 个 candidate 的字段都来自同一原 lane。

CEOP projection-pixel staging 把 `source_index`、`target_index`、`x`、`y`、`z` 分别 `vcompress` 到临时 SoA buffer，再用短标量循环组装 `ProjectedOrganizedProjectionCandidate`。该短标量循环不是重新执行 predicate，只负责 AoS staging 组装。production helper 使用 `vlmax_e32m2 <= 64` gate 保护 `[64]` buffer。

### 5.5 gather / layout / target index

现象：

- 某个点类型通过，另一个点类型失败；
- fake indices 通过，显式 subset 失败；
- organized target 读取位置错。

诊断：

1. 核实 traits 和字段 offset。
2. 核实 byte offset 是否能放入 32-bit offset。
3. 对 organized cloud，确认 `target_index = v * width + u` 与 `target.at(u, v)` 的线性映射一致。
4. subset case 覆盖乱序、重复、非连续 indices。

CEOP target-predicate production 使用 projected staging 中的 `target_index` 直接 gather target `x/y/z`。该 index 来自 projection-pixel staging 的 `v * width + u`，对应 PCL organized cloud 的二维访问语义。

## 6. CEOP 反汇编诊断案例

本节记录 `registration/include/pcl/registration/impl/correspondence_estimation_organized_projection.hpp` 当前 production-ready 状态下的诊断方式。它不是只描述 CEOP，也提供后续主题可复用的诊断样板。

### 6.1 标量语义边界

CEOP 标量入口的核心流程：

```cpp
for (const auto& src_idx : (*indices_)) {
  if (isFinite((*input_)[src_idx])) {
    const Eigen::Vector4f p_src(src_to_tgt_transformation_ *
                                (*input_)[src_idx].getVector4fMap());
    const Eigen::Vector3f p_src3(p_src[0], p_src[1], p_src[2]);
    const Eigen::Vector3f uv(projection_matrix_ * p_src3);
    if (uv[2] <= 0)
      continue;
    const int u = static_cast<int>(uv[0] / uv[2]);
    const int v = static_cast<int>(uv[1] / uv[2]);
    const PointTarget& pt_tgt = target_->at(u, v);
    const double dist = (p_src3 - pt_tgt.getVector3fMap()).norm();
    if (dist < max_distance)
      append correspondence;
  }
}
```

当前 production RVV 已覆盖三段：

| production helper                                     | RVV 负责                                                                                                   | 反汇编重点                                                                                   |
| ----------------------------------------------------- | ---------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------- |
| `projectOrganizedProjectionCandidatesRVV()`         | source gather、finite、identity fast path、Eigen-aligned 4x4 transform、`z > 0`、source staging compress | `vluxei32.v`、`vfmacc.vf`、`vfadd.vv`、`vcompress.vm`                                |
| `projectOrganizedProjectionPixelsRVV()`             | projection-pixel、`vfdiv`、RTZ 截断、in-bounds、`target_index`、projected staging compress             | `vfmacc.vf`、`vfdiv.vv`、`vfcvt.rtz.x.f.v`、`vcompress.vm`                           |
| `acceptProjectedOrganizedProjectionCandidatesRVV()` | target gather、target finite、depth mask、distance predicate、accepted staging compress                    | `vluxei32.v`、`vfmul.vv`、`vfmacc.vv`、`vfsqrt.v`、`vmflt/vmfle`、`vcompress.vm` |

append 和 stored distance 写出仍保留标量。accepted lane 写出 `pcl::Correspondence` 前重算 Eigen `norm()`，用于保持 `Correspondence::distance` bit pattern 绑定 production 标量表达式。

### 6.2 source transform：Eigen 4x4 row-dot

non-identity transform 的风险是把 `Matrix4f * getVector4fMap()` 按数学式随意重排。CEOP 诊断确认当前标量 lowering 是两个 FMA 部分和再相加，RVV 使用同构结构：

```text
lo = m1*y
lo = fmacc(lo, m0, x)
hi = m3*w
hi = fmacc(hi, m2, z)
out = hi + lo
```

RVV 对应形态：

```cpp
vfloat32m2_t tx_lo = __riscv_vfmul_vf_f32m2(y, transform(0, 1), vl);
tx_lo = __riscv_vfmacc_vf_f32m2(tx_lo, transform(0, 0), x, vl);
vfloat32m2_t tx_hi = __riscv_vfmv_v_f_f32m2(transform(0, 3), vl);
tx_hi = __riscv_vfmacc_vf_f32m2(tx_hi, transform(0, 2), z, vl);
vfloat32m2_t tx = __riscv_vfadd_vv_f32m2(tx_hi, tx_lo, vl);
```

asm 窗口中应能看到：

```text
vluxei32.v    # gather source x/y/z
vfmul.vf      # 部分和初值
vfmacc.vf     # fused multiply-add
vfadd.vv      # 两个部分和合并
vcompress.vm  # 保序输出 OrganizedProjectionCandidate
```

诊断结论：4x4 transform 不是“任意 FMA 链都等价”。若输出进入 projection boundary，应对齐当前 Eigen lowering，并用 non-identity boundary case 回归。

### 6.3 projection-pixel：FMA 后接 RTZ

标量 projection 在 CEOP 中等价为：

```text
uv0 = fx*x + cx*z
uv1 = fy*y + cy*z
u = static_cast<int>(uv0 / z)
v = static_cast<int>(uv1 / z)
```

RVV production 使用：

```text
uv0 = z*cx
uv0 = vfmacc(uv0, fx, x)
uv1 = z*cy
uv1 = vfmacc(uv1, fy, y)
u/v = vfdiv.vv + vfcvt.rtz.x.f.v
```

典型 asm 线索：

```asm
vfmul.vf        # z * cx / z * cy
vfmacc.vf       # + fx*x / + fy*y
vfdiv.vv        # uv / z
vfcvt.rtz.x.f.v # static_cast<int>
vmand.mm        # in-bounds mask
vcompress.vm    # source_index / target_index / x / y / z
vcpop.m
vse32.v
```

诊断结论：`vfcvt.rtz.x.f.v` 只证明转换方向正确。若转换输入来自不同 FMA 结构，像素仍可能不同。修复应从 projection 表达式的 FMA contraction 入手。

### 6.4 target-predicate：float norm 与 double predicate

标量源码：

```cpp
const double dist = (p_src3 - pt_tgt.getVector3fMap()).norm();
if (dist < max_distance)
  append correspondence;
```

当前计算对象是 `Eigen::Vector3f`。标量核心是 float 域：

```text
dx/dy/dz float
sum = dx*dx + dy*dy + dz*dz
float_norm = sqrtf(sum)
dist = double(float_norm)
predicate = dist < max_distance
```

production RVV distance predicate 对应：

```text
vfmul.vv       # dx*dx
vfmacc.vv      # + dy*dy
vfmacc.vv      # + dz*dz
vfsqrt.v       # float norm
vmflt/vmfle.vf # double(float_norm) < max_distance 的等价 float predicate
vcompress.vm   # accepted candidates
```

压缩后 stored distance 标量重算仍可在 asm 中看到：

```text
fsqrt.s
fcvt.d.s
flt.d
```

这些标量指令不是 production RVV predicate 未覆盖的证据，而是 append 前重算 stored distance 的刻意边界。诊断时要区分“predicate 已 RVV 化”和“输出字段仍标量重算”。

### 6.5 证据与回归

CEOP closeout 使用的关键证据：

- QEMU `run_test_compare`：std 构建 39 个测试运行、10 个 RVV-only 诊断按预期 skip；RVV 构建 39/39 pass。
- QEMU `run_bench_compare`：production identity checksum `10393124863019881355`，production non-identity checksum `13376866430852120216`，std/RVV 对齐。
- `dump_bench_rvv`：确认 production `acceptProjectedOrganizedProjectionCandidatesRVV<PointXYZ>` 窗口出现 `vluxei32.v`、`vfmul.vv`、`vfmacc.vv`、`vfsqrt.v`、`vmfle.vf`、`vcompress.vm`、`vcpop.m`、`vse32.v`；压缩后 stored distance 标量重算仍可见 `fsqrt.s`、`fcvt.d.s`、`flt.d`。
- board `board_smoke`：39/39 tests passed；production identity fake/explicit `1.64x` / `1.65x`；production non-identity fake/explicit `2.36x` / `2.36x`。

这些证据分工明确：QEMU 和 asm 证明 correctness、checksum、路径和指令形态；board 证明真实性能；RVV-only bit / threshold tests 保护已知语义边界；production direct tests 是最终入口证据。

## 7. 其它常见诊断模式

### 7.1 自动向量化干扰

有些主题手写 RVV 后仍保留短标量尾段。编译器可能把尾段自动改成 vector reduction，导致差异同时包含“手写 RVV 近似”和“尾段规约顺序变化”。这类判断不能只凭源码或模型推断，应通过反汇编确认。若后续复核 `filters/bilateral` 或类似邻域权重路径，应把尾段是否出现非预期自动 vector reduction 作为独立检查项，再决定是否需要局部限制 tree vectorizer 或调整诊断归因。

反汇编检查点：

```text
helper 内应出现手写 RVV load / exp / store
尾段不应出现非预期自动 vector reduction
```

如果最终采用 pragma 或其它编译器控制，文档要说明作用范围、保护的语义边界，以及它不影响手写 RVV intrinsic。

### 7.2 规约类路径

`norms`、`centroid`、covariance 这类路径通常会出现 `vfredosum`、`vfredmax`、`vfredmin`。这类指令改变累加树或条带归约顺序，通常不能要求 bit 等价，但必须说明容差预算和业务可接受性。

反汇编结论应写成：

```text
该路径命中 `vfredosum`，因此与标量左到右累加顺序不同；测试采用容差而非 bit equality。
```

若规约结果进入排序、采样、bucket 或确定性输出序列，应重新评估是否允许容差。`covariance_sampling` 的 full diagnostic checksum 不一致，说明局部 RVV 片段和近似数值不足以证明 sampled-index 序列等价，因此保留 bench-only。

### 7.3 多谓词 mask 收敛

`frustum_culling` 这类路径会对同一 VL chunk 计算多个几何谓词，再用 `vmand` 收敛：

```text
distance = a*x + b*y + c*z + d
mask = distance <= 0
inside = mask_left & mask_right & ... & mask_near
vcompress(source_index, inside)
```

反汇编重点是 `vfmacc` / `vmfle` 重复出现、`vmand` 链、最终 `vcompress`。若 `negative_` 或 removed indices 也参与输出，要确认 keep / removed mask 的互补关系和两路压缩顺序。

## 8. 结果记录模板

写入主题文档或评估文档时，建议使用的字段：

```text
### 反汇编诊断：<问题名>

现象：
- case:
- Std/RVV checksum:
- output size / first mismatch:

最小复现：
- adversarial data:
- 覆盖入口:

标量语义：
- 源码表达式:
- 标量 lowering:

RVV 线索：
- helper / symbol:
- 关键指令:
- 与源码阶段的映射:

归因：
- FMA / conversion / FRM / compress / gather / tail:

修复：
- intrinsic / gate / fallback / tail 变化:

回归：
- tests:
- bench checksum:
- asm:
- board performance:

生产判断：
- production direct / diagnostic / bench-only:
- 保留标量边界:
```

该模板可以压缩成段落，但信息不要丢。尤其要写清“哪些证据只是 QEMU 路径和 checksum，哪些证据来自板卡性能”。

## 9. 常用命令

```bash
make -C test-rvv/<module>/<topic> run_test_compare
make -C test-rvv/<module>/<topic> run_bench_compare
make -C test-rvv/<module>/<topic> dump_bench_rvv
```

CEOP 示例：

```bash
make -C test-rvv/registration/correspondence_estimation_organized_projection run_test_compare
make -C test-rvv/registration/correspondence_estimation_organized_projection run_bench_compare
make -C test-rvv/registration/correspondence_estimation_organized_projection dump_bench_rvv
```

常用搜索：

```bash
rg -n "vfmacc|vfmul|vfadd|vfsqrt|vfcvt|vcompress|vcpop|vluxei32|vsetvli|fsrm|frrm|frcsr" \
  test-rvv/<module>/<topic>/build/asm/riscv/bench_<topic>_rvv.full.asm

rg -n "fmadd|fsqrt|fcvt|flt|vfsqrt|vfmacc|vmflt|vmfle" \
  test-rvv/<module>/<topic>/build/asm/riscv/bench_<topic>_rvv.full.asm
```

文档写完后建议检查：

```bash
git diff --check -- doc-rvv/rvv/RVV\ Disassembly\ Diagnostic.zh.md
rg -n '[ \t]+$' doc-rvv/rvv/RVV\ Disassembly\ Diagnostic.zh.md
rg -n '<conversation-marker-pattern>' doc-rvv/rvv/RVV\ Disassembly\ Diagnostic.zh.md
```
