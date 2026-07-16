# RVV 数学测试目录与证据分层规则

本参考用于规划数学 helper 的 scratch 测试、benchmark、caller smoke 和迁移目录。它关注“证据放在哪里、如何拆 target、什么能作为 gate”，不记录具体函数的系数或实验结果。

## 1. 测试目录归属

本仓库的数学 helper / std-libm RVV 向量化专项测试应优先放在：

```text
test-rvv/rvv/math/<function>/
```

例如：

```text
test-rvv/rvv/math/
  Makefile
  board.mk
  README.zh.md
  script/
    lp_minimax.py
    sollya_utils.py
  sincos/
    sincos_test.cpp
    sincos_range_smoke.cpp
    script/parms_sincos.py
```

选择规则：

- 测试数学 helper 本体、参数脚本、系数候选、RVV intrinsic 链路、板卡 microbench 时，放 `test-rvv/rvv/math/<function>/`。
- 其它项目使用等价的 RVV math 专项目录；不要把数学 helper 专项测试混进业务 module 测试目录。
- 测试某个 PCL module 的 production caller 行为时，才放该 module 对应目录。
- 如果数学 helper 已经从 module 代码抽到 `common/include/pcl/common/impl/rvv_math.hpp` 等公共位置，测试也应从 module 目录迁到 RVV math 专项目录。
- 不要让同一个数学函数长期同时存在 `common/common` 和 `rvv/math` 两套入口；迁移后旧目录应只保留迁移说明或删除旧 target。

## 1.1 文档目录归属

本仓库的 RVV 数学函数实现/证据文档默认放在：

```text
doc-rvv/rvv/math/
```

选择规则：

- 数学 helper 本体文档、系数来源、参数脚本口径、特殊值合同、scalar/RVV 同构链路、QEMU/板卡证据，放 `doc-rvv/rvv/math/`。
- PCL common 模块函数文档仍放 `doc-rvv/common/`，用于记录 common 公开入口、数据路径、分派策略、回退条件和模块级验证。
- common 函数如果调用数学 helper，只在 common 文档中写调用关系、输入域是否满足 helper 合同、下游误差或 fallback 边界；数学细节通过链接指向 `doc-rvv/rvv/math/`。
- 目录迁移时同步修正旧的 `doc-rvv/common/<math-helper>.zh.md` 引用，避免后续 worker 继续把数学 helper 文档写回 common 目录。

## 2. 顶层与函数子目录

推荐顶层聚合、函数子目录自治：

- `test-rvv/rvv/math/Makefile`：聚合显式 target、统一构建/输出/部署规则。
- `test-rvv/rvv/math/board.mk`：板卡侧单函数运行规则。
- `test-rvv/rvv/math/README.zh.md`：说明目录职责、target 和证据边界。
- `test-rvv/rvv/math/script/`：只放跨函数复用脚本，例如 LP/minimax、Sollya 工具、公共常量脚本。
- `test-rvv/rvv/math/<function>/script/`：放函数专属参数脚本和 Sollya 文件。

产物按函数分桶，避免日志互相覆盖：

```text
build/<arch>/<function>/
output/qemu/<function>/
output/board/<function>/
log/<function>/
```

默认 target 不应自动跑全量 scratch、bench 或部署。全量目标可以存在，但必须显式请求，例如 `run_math_all`、`deploy_math_all`。

## 3. 文件与 target 拆分

一个函数的证据通常拆成四类：

- `parms_<function>`：参数脚本，报告系数来源、kernel error、dense-chain simulation 和量化后结果。
- `run_<function>_test`：数学 helper 本体测试，包含 dense/adversarial/special、scalar/RVV same-chain、必要的 ULP/bit-level 检查。
- `run_<function>_bench`：板卡性能信号，和 correctness 分离；QEMU timing 不能作为性能证据。
- `run_<function>_<caller>_smoke`：真实或 caller-shaped 下游 smoke。

当单个 C++ 文件同时包含数学 correctness、benchmark、caller smoke、生产 caller 仿真时，应拆文件或拆 target。常见拆法：

```text
<function>_test.cpp          # 数学专项 correctness 与 scalar/RVV same-chain
<function>_bench.cpp         # 可选，若 benchmark 逻辑变复杂
<function>_<caller>_smoke.cpp
```

scratch 阶段允许少量重复代码，以保持文件边界清楚；只有当两个以上函数或 smoke 复用同一套逻辑时，再抽公共 header / helper。

### 审查型注释

通用术语解释、文件级阅读提示、函数级作用/调用者说明见 `rvv-workflow/references/reviewability-and-language.zh.md`；数学函数专用术语见 [RVV 数学函数原型的术语补充](reviewability-and-language.zh.md)。即使注释整体采用英文主导，首次出现的专有术语也要用括号解释其含义。

scratch C++、Python 脚本和 Makefile target 应保留审查型注释，帮助读者确认测试意图和证据边界：

- 说明输入样本为什么代表某个 caller shape，尤其是角度、offset、FOV、range 或退化边界的来源。
- 说明 scalar same-chain、RVV path、reference path 各自承担的证据角色。
- 说明 caller smoke 的下游误差计算是否只覆盖 local shape，是否跳过 production transform / dispatch / fallback。
- 说明 gate 检查哪些条件，以及哪些输出只是诊断值。

production 代码不要写逐行复述语法的注释；但 `test-rvv` / prototype 代码可以更详细，尤其是长文件、复杂 gate、caller-shaped smoke 和 RVV intrinsic 链路。注释应服务审查者理解合同、证据和不可泛化的边界。

## 4. Caller smoke 必须是 gate

Caller smoke 不能只是打印统计。它至少应返回成功/失败，并在输出中给出 gate：

- `domain_out == 0`，或明确说明域外输入如何 fallback。
- `in_contract == samples`，若目标是有限域 helper。
- 下游误差阈值，例如 xyz/euclidean、分类一致率、权重误差或 pipeline 容差。
- 阈值来源：当前数学误差预算、caller 容差、历史 baseline、或 scratch 阶段临时阈值。
- 若用于证明 RVV helper 风险，应覆盖 RVV path，或明确声明只覆盖 scalar approximation。

Caller smoke 只证明下游风险，不替代：

- 参数脚本。
- Dense/adversarial/special 数学专项测试。
- Scalar/RVV same-chain 对拍。
- QEMU correctness。
- 板卡性能证据。

## 5. Caller 输入域闭合

记录 caller 输入域时必须说明：

- 变量来源和单位。
- 参数如何传导到 helper 输入。
- 是否包含端点。
- 是否有 offset、除法、归一化或投影步骤会放大输入域。
- 一个 caller 形态不能自动代表所有 caller。

例如 spherical range image 形态可以证明 `angle_x in [-pi, pi]`、`angle_y in [-pi/2, pi/2]`；但 base range image 若含 `angle_x / cos(angle_y)`，接近垂直边界时输入域可能扩大，不能用 spherical smoke 闭合所有 RangeImage production gate。

## 6. 迁移与重构规则

目录迁移和数学方案变化应分批：

- 先迁移目录并保持行为等价。
- 再继续拟合、换系数或接 production。
- 迁移后同步 README、Makefile/board.mk、参数脚本路径和 skill 路标。
- 旧目录不要保留可运行的隐藏入口，避免 worker 后续在旧路径继续开发。

如果迁移多个已有函数，按函数分桶移动资产；不要把所有测试平铺到顶层目录。

## 7. 收尾检查

迁移或新增测试后至少检查：

```bash
git diff --check
make -C test-rvv/rvv/math parms_<function>
make -C test-rvv/rvv/math ARCH=x86 run_<function>_test
make -C test-rvv/rvv/math run_<function>_test
```

若有 caller smoke：

```bash
make -C test-rvv/rvv/math run_<function>_<caller>_smoke
```

若有板卡证据：

```bash
make -C test-rvv/rvv/math deploy_<function>_test
ssh <board> 'cd /root/pcl-test/rvv/math && make -f board.mk run_<function>_test'
```

最后清理 `build/`、`output/`、`log/`、`.venv`、`__pycache__` 等生成物，确认 `git status --short` 只剩预期源码/文档改动。
