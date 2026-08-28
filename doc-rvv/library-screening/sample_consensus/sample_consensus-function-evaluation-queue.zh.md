# sample_consensus 模块 RVV 第二轮函数评估队列

本文档基于 `doc-rvv/library-screening/modules/sample_consensus-file-candidate-screening.zh.md` 的第一轮 high/mid 基线下钻源码，形成第二轮函数级评估队列。本文只做源码阅读、静态分析和队列整理；不重跑第一轮筛选，不修改 production 源码，不建立 `test-rvv` topic，不运行板卡 bench，不提交。

## 1. 输入依据

- 第一轮筛选：`doc-rvv/library-screening/modules/sample_consensus-file-candidate-screening.zh.md`，基线为 `high=8`、`mid=10`，共 18 个必查候选。
- 筛选规则：`.agents/skills/rvv-screening/SKILL.md`、`references/screening-criteria.md`、`references/stage-and-queue-policy.md`、`references/evidence-boundaries.md`、`references/templates/function-evaluation-queue-template.md`。
- 现有 RVV 证据：
  - `doc-rvv/sample_consensus/countWithinDistanceRVV.zh.md`
  - `doc-rvv/sample_consensus/selectWithinDistance_getDistancesToModel_RVV.zh.md`
  - `test-rvv/sample_consensus/README.zh.md`
  - `test-rvv/sample_consensus/plane_models/`
  - `test-rvv/sample_consensus/quadric_models/`
- 源码范围：`sample_consensus/include/pcl/sample_consensus/**` 中第一轮 high/mid 文件对应实现。

现有 RVV 覆盖边界：

- `impl/sac_model_plane.hpp`：当前源码已有 `selectWithinDistance`、`countWithinDistance` 和 `getDistancesToModel` 三入口 production RVV；Phase 010 又只为 select/count 采纳 identity-index strided load，`getDistancesToModel` 保持 gather-only。
- `impl/sac_model_circle.hpp`：当前源码只在 `countWithinDistance` 入口有 `__RVV10__` 分流和 `countWithinDistanceRVV`；不要外推为 `selectWithinDistance` 或 `getDistancesToModel` 已覆盖。
- `impl/sac_model_sphere.hpp`：当前源码已有 `countWithinDistanceRVV`，且 Phase 020 已为 `selectWithinDistance` 接入 production RVV；`getDistancesToModel` 仍保持标量。
- `impl/sac_model_normal_plane.hpp`：当前源码已有 `selectWithinDistanceRVV`、`countWithinDistanceRVV`、`getDistancesToModelRVV` 三条入口；仍需把 float/RVV 与 double/standard 的数值边界、预分配写回语义和生产接入证据作为后续收敛项。
- 其它 high/mid 文件：未发现当前源码内已有 RVV 实现入口；如进入建议或保留队列，均为新增 RVV 候选或诊断候选。

## 2. 二轮筛选统计

| 项目 | 数量 | 说明 |
| --- | ---: | --- |
| 第一轮源码文件总数 | 70 | 第一轮已完成 `70/70` 文件级覆盖 |
| 第一轮 high | 8 | 二轮必查 |
| 第一轮 mid | 10 | 二轮必查 |
| 第一轮 low | 52 | 队列创建阶段未补入 |
| 第二轮初始候选基线 | 18 | high + mid，逐项交代去向 |
| 新增补充候选 | 0 | 未发现需要补入 low 文件的明确漏判证据 |
| 第二轮候选总数 | 18 | 去重后仍为 18 |
| 建议进行 RVV 优化的文件 | 6 | 覆盖公开入口主路径或已有 RVV 资产仍需收敛 |
| 保留实施的候选文件 | 6 | 有 RVV 片段，但需先回答数值、布局、profile、归属或诊断问题 |
| 暂缓或不推荐考虑 RVV 优化的文件 | 6 | 当前不支持独立 RVV topic，或只适合作为伴随 helper |
| 源码冲突、合并、删除或不单独实施 | 0 | 18 个 high/mid 文件均仍存在；部分降为暂缓但未删除 |
| high/mid 降级为保留或暂缓 | 12 | `high` 中 2 个转保留；`mid` 中 4 个转保留、6 个转暂缓 |

## 3. 文件级变化理由

| 第一轮文件 | 第一轮 | 第二轮去向 | 变化理由 |
| --- | --- | --- | --- |
| `impl/sac_model_plane.hpp` | high | 建议 | 已完成三入口 production RVV 接入；Phase 010 窄采纳 select/count identity-index strided load，getDistances identity 分支 rejected with evidence。Phase 020 已补代表性 AoS 点型 correctness；当前无新的未阻塞性能优化候选。 |
| `impl/sac_model_normal_plane.hpp` | high | 建议 | 已有三入口 RVV 和 test/bench 资产；不视为全部完成，后续重点是数值边界、fallback、生产形态和文档收敛。 |
| `impl/sac_model_sphere.hpp` | high | 建议 | `countWithinDistance` 已有 RVV；Phase 020 已把 `selectWithinDistance` 接入 production RVV，接入后 board repeated median `1.5020x`；`getDistancesToModel` 当前候选退化并保持标量。 |
| `impl/sac_model_circle.hpp` | high | 建议 | `countWithinDistance` 已有 RVV；2D x/y shell 判定简单，可扩展到 select/getDistances 或先做 component bench。 |
| `impl/sac_model_line.hpp` | high | 建议 | 无现有 RVV，但 line 的 point-to-line cross3 距离核覆盖 select/count/getDistances 主路径，循环独立且上游 line tests 可迁移。 |
| `impl/sac_model_stick.hpp` | high | 建议 | 无现有 RVV，几何核与 line 相近；`countWithinDistance` 的 `nr_i/nr_o` 语义需单独对拍，但仍是直接主路径。 |
| `impl/sac_model_cylinder.hpp` | high | 保留 | 点/法线主循环算术密集，但 `dir.norm()`、`getAngle3D`、early continue 和优化前 staging 需要先做 production-shaped diagnostic。 |
| `impl/sac_model_normal_sphere.hpp` | high | 保留 | 与 normal-plane 形态相近，但当前无 RVV；球面法线方向、early continue 和 float/double 误差需先验证。 |
| `impl/sac_model_circle3d.hpp` | mid | 保留 | 逐点 3D 投影核明确，但 double normalize/divide 较重；建议先隔离 count/select 距离核做诊断。 |
| `impl/sac_model_cone.hpp` | mid | 保留 | cone 点/法线循环有可 RVV 子核，但 `pointToAxisDistance`、三角函数常量、height/dir normalize 与角度近似风险高。 |
| `impl/sac_model_registration.hpp` | mid | 保留 | correspondence transform 距离是直接循环，但双索引 gather、registration 模块归属和 SVD 外部成本需先确认。 |
| `impl/mlesac.hpp` | mid | 保留 | EM 后处理连续数组循环可诊断，但每轮先调用模型 `getDistancesToModel`，且 `exp/log`、规约顺序和 median/sort 稀释收益。 |
| `impl/sac_model_ellipse3d.hpp` | mid | 暂缓 | 外层逐点循环存在，但核心 `dvec2ellipse` 内含区间分支和 golden-section `while` 搜索，不适合作为第一批 RVV 主路径。 |
| `impl/sac_model_torus.hpp` | mid | 暂缓 | 每点调用 `projectPointToTorus`，几何 helper 与 LM 优化路径复杂；当前更适合作为后续专项数学重构而非直接 RVV。 |
| `impl/sac_model_registration_2d.hpp` | mid | 暂缓 | 有 transform + projection 循环，但 uv 深度分支、除法、target `u/v` 字段和归属问题使当前独立收益不明确。 |
| `impl/msac.hpp` | mid | 暂缓 | 本文件只处理 `distances` 的 min/sum、threshold 和 inlier 压缩，主成本由派生模型距离核承担。 |
| `impl/rmsac.hpp` | mid | 暂缓 | 与 MSAC 类似，且增加随机 pre-test；本地循环是 tail/diagnostic，不建议独立实施。 |
| `sac.h` | mid | 暂缓 | `refineModel` 主要是虚调用 `optimizeModelCoefficients`/`selectWithinDistance`、收敛状态和 inlier 序列比较；不具备独立 RVV 边界。 |

## 4. 建议进行 RVV 优化的文件

| 文件路径 | 关键入口 / 函数族 | 主题簇 | 主成本覆盖类型 | trip count / 数据规模来源 | RVV 适配点 | 现有 RVV 覆盖情况 | 主要风险 | 测试 / bench 可行性 | 第二轮去向 | 建议实施顺序或理由 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_plane.hpp` | `selectWithinDistance`、`countWithinDistance`、`getDistancesToModel`；源码入口见 50-91、277-305、582-610，RVV 实现见 161-273、471-578、655-758 | 已有 normal-plane RVV 收敛 | `direct-main-path` | `indices_->size()`；同一 index 访问 input 点、normal 和 curvature | RVV strip-mining、`indices_` 连续加载、点/法线/曲率 gather、平面距离 + acute angle、mask popcount、select compress 写回、float 转 double | 已有三入口 RVV；`plane_models` GTest/bench 覆盖 select/count/getDistances；板卡日志显示 normal-plane 三项已有加速证据 | RVV 以 float 算术近似 standard double + `getAngle3D`；阈值附近 inlier 差异、`acos` 近似、预分配写回峰值内存、PointT/PointNT layout fallback | 已有 `test-rvv/sample_consensus/plane_models`；可继续补边界 case、layout fallback、QEMU objdump 和板卡 compare | 建议 | 顺序 1：先收敛已有资产，避免后续模型复用错误边界；不是判定“已全部完成”。 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_plane.hpp` | `getDistancesToModel`、`selectWithinDistance`、`countWithinDistance` 公开入口及对应 Standard/RVV helper；Phase 010 新增 `sacModelPlaneRVVLoadXYZ` identity/gather 加载 helper | 基础几何模型：plane | `direct-main-path` | `indices_->size()`，通常是全云或用户索引子集 | x/y/z gather 后做 `abs(ax+by+cz+d)`；count 用 mask popcount；select 用 mask/compress；identity select/count 可用 strided load；getDistances dense store double | 已完成三入口 production RVV；Phase 010 窄采纳 select/count identity strided load，getDistances identity branch 已撤回；Phase 020 代表性点型 correctness 已通过；Phase 025 显式空 `indices_` correctness 已通过 | float/double 距离差异、非 AoS fallback runtime fixture、代表点型 dedicated performance、环境 metadata 和 binary hash 缺失 | `test-rvv/sample_consensus/sac_model_plane/` 已有 7 个 correctness gtest、identity/shuffled bench mode、QEMU/asm、Phase 000/010 board repeated、Evidence Doctor 摘要和板卡 RVV gtest smoke | 建议 | 顺序 2 当前状态：production adopted；当前无新的未阻塞性能优化候选，已进入 topic closeout / submit readiness。 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_sphere.hpp` | `getDistancesToModel` 180-199、`selectWithinDistance` 202-235、`countWithinDistance` 238-257、`countWithinDistanceRVV` 370-434，Phase 020 新增 `selectWithinDistanceRVV` | 基础几何模型：sphere | `direct-main-path` | `indices_->size()` | x/y/z gather、平方距离、半径 shell 双边界 mask；select 命中后 exact sqrt 写 `error_sqr_dists_`；getDistances 每点 sqrt | 已有 `countWithinDistanceRVV`；`selectWithinDistanceRVV` 已按当前 scope production adopted；无 `getDistancesToModelRVV` | shell 边界含等号，需保持 standard 的 `<=`/`>=`；select 写回顺序；getDistances sqrt 成本；半径限制和 threshold 接近 0 的边界；点型性能扩展未闭合 | `test-rvv/sample_consensus/sac_model_sphere/` 已覆盖 production direct correctness、asm、board repeated 和 Evidence Doctor；`getDistancesToModel` 候选退化 | 建议 | 顺序 3 当前 closeout：`selectWithinDistance` 已采纳，5-run board median `1.5020x`；后续只在 `040-select-vcompress-ablation`、`050-point-type-expansion` 或 `030-sqrt-helper-audit` 中继续。 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle.hpp` | `getDistancesToModel` 163-185、`selectWithinDistance` 188-223、`countWithinDistance` 226-247、`countWithinDistanceRVV` 359-422 | 基础几何模型：circle2d | `direct-main-path` | `indices_->size()` | x/y gather、平方距离、半径 shell 双边界 mask；select 可 mask/compress，getDistances 需 sqrt dense store | 已有 `countWithinDistanceRVV`；无 `selectWithinDistanceRVV` / `getDistancesToModelRVV` | 只要求 x/y field layout；边界等号需与 standard 对齐；输出 inlier 顺序和 `error_sqr_dists_` exact distance | `quadric_models` 已有 Circle2D count smoke/perf 和 RANSAC tests；可复用 sphere shell test 框架 | 建议 | 顺序 4：与 sphere 共享双边界策略，代码量小；可和 sphere 同一主题比较 x/y vs x/y/z gather 成本。 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp` | `getDistancesToModel` 106-130、`selectWithinDistance` 134-168、`countWithinDistance` 171-199 | 基础几何模型：line | `direct-main-path` | `indices_->size()`；优化阶段另有 `inliers.size()` 但不作为第一目标 | x/y/z gather、`line_pt - point`、cross3 展开、squaredNorm、threshold mask；getDistances 需 sqrt dense store | 无现有 RVV | Eigen `getVector4fMap()`/cross3 展开需转手写公式；浮点阈值、direction normalize 只应在 loop 外；project/optimize 不应一并纳入 | 上游 `test/sample_consensus/test_sample_consensus_line_models.cpp` 有 line 回归；可新增 `test-rvv` direct count/select/getDistances 对拍 | 建议 | 顺序 5：无角度/法线依赖，新增 RVV 价值清楚；建议先做 `countWithinDistance`，再决定 select/getDistances。 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_stick.hpp` | `getDistancesToModel` 106-143、`selectWithinDistance` 146-193、`countWithinDistance` 196-245 | 基础几何模型：stick | `direct-main-path` | `indices_->size()` | 与 line 相同的 x/y/z gather + cross3/squaredNorm；`countWithinDistance` 需要两个阈值区间计数 `nr_i`/`nr_o` | 无现有 RVV | `countWithinDistance` 返回 `nr_i <= nr_o ? 0 : nr_i - nr_o`，不是普通 inlier count；`getDistancesToModel` 对 outlier 乘 2；文件名像 line 但语义不同 | 当前 test-rvv 未覆盖 stick；可从 line direct test 复制数据生成器，并加入 `nr_i/nr_o` 边界用例 | 建议 | 顺序 6：可复用 line 几何核，但必须独立验证 stick 语义，不和 line 合并实现结论。 |

## 5. 保留实施的候选文件

| 文件路径 | 关键入口 / 函数族 | 主题簇 | 主成本覆盖类型 | trip count / 数据规模来源 | RVV 适配点 | 现有 RVV 覆盖情况 | 主要风险 | 测试 / bench 可行性 | 第二轮去向 | 建议实施顺序或保留理由 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_cylinder.hpp` | `getDistancesToModel` 143-179、`selectWithinDistance` 182-227、`countWithinDistance` 230-265；优化前 staging 288-298 | 法线几何模型：cylinder | `diagnostic` | `indices_->size()`；优化前 staging 为 `inliers.size()` | 点/法线 gather、axis projection、radial norm、normal angle、weighted distance、mask/count/compress；staging 可单独 array write | 无现有 RVV | early continue 会导致两阶段 mask；`dir.norm()` 与 `getAngle3D` 成本和近似误差高；LM 优化由外部 helper/Eigen 主导，不能算 RVV 覆盖 | `quadric_models` 有 cylinder RANSAC、projectPoints、RadiusAccuracy、sample tests；缺 direct RVV oracle，需要新增 component ablation | 保留 | 顺序 7：normal-plane 收敛后再做 production-shaped diagnostic，先回答角度近似和 early mask 是否可控。 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp` | `selectWithinDistance` 48-106、`countWithinDistance` 109-157、`getDistancesToModel` 160-206 | 法线几何模型：normal-sphere | `diagnostic` | `indices_->size()` | 点/法线 gather、球心方向 `n_dir`、球面距离、normal angle、weighted mask/count/compress | 无现有 RVV；不可外推 sphere count 或 normal-plane 三入口覆盖 | `n_dir` 随点变化且可能退化；early continue、`getAngle3D` 近似、normal weight、float/double 边界 | `quadric_models` 有 NormalSphere RANSAC；需新增直接对拍和边界点法线用例 | 保留 | 顺序 8：与 cylinder/cone 同属法线几何，建议在 normal-plane 数值策略确定后复筛。 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle3d.hpp` | `getDistancesToModel` 138-181、`selectWithinDistance` 185-234、`countWithinDistance` 237-276 | 3D 圆 / 投影几何 | `diagnostic` | `indices_->size()` | x/y/z gather、点到圆平面投影、局部径向 normalize、距离平方 mask | 无现有 RVV | double 中间量、`N.dot(N)` 除法、`helper_vectorP_projC.normalized()` 退化、select 与 getDistances 的 lambda 符号差异需确认 | `quadric_models` 有 Circle3D RANSAC；可新增 direct count/select 对拍，但代表数据规模小 | 保留 | 顺序 9：先做 count/select 的 component ablation，不直接承诺 production 接入。 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_cone.hpp` | `getDistancesToModel` 149-202、`selectWithinDistance` 205-270、`countWithinDistance` 273-328；`pointToAxisDistance` 515-520；优化前 staging 351-361 | 法线几何模型：cone | `diagnostic` | `indices_->size()`；staging 为 `inliers.size()` | 点/法线 gather、axis projection、height/radius、point-to-axis distance、normal angle、mask/count/compress | 无现有 RVV | `pointToAxisDistance` 内部 sqrt；每点 normalize 两次；opening angle 三角函数虽可 loop 外广播但几何退化多；输出语义比 cylinder 更复杂 | `quadric_models` 有 Cone RANSAC；缺 direct RVV 对拍和 cone 退化边界 | 保留 | 顺序 10：若 cylinder 证明 normal-angle RVV 模式可控，再评估 cone；否则暂缓。 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_registration.hpp` | `getDistancesToModel` 100-143、`selectWithinDistance` 146-200、`countWithinDistance` 203-249；SVD/staging 252-321 | correspondence registration | `diagnostic` | `indices_->size()` 且必须等于 `indices_tgt_->size()`；SVD staging 为 `inliers.size()` | source/target 双 gather、4x4 transform、差值 squaredNorm、threshold mask/count/compress | 无现有 RVV | 双索引 gather 成本、target cloud layout、correspondence 顺序、`computeModelCoefficients`/`optimize` 依赖 map 和 SVD；模块归属可能更接近 registration | 上游 sample_consensus registration 测试需单独确认；可先建立 direct synthetic correspondence test | 保留 | 顺序 11：作为 profile prerequisite，先确认 sample_consensus 内真实使用热度和 topic 归属。 |
| `sample_consensus/include/pcl/sample_consensus/impl/mlesac.hpp` | `computeModel` EM 概率循环 122-141，final inlier 压缩 194-202；MAD/minmax/median 211-280 | SAC 方法后处理：MLESAC | `diagnostic` | RANSAC 外层迭代数 × `indices_size`；MAD/minmax/median 为 `indices->size()` | 连续 `std::vector<double>` 的 exp/log、gamma 规约、threshold 压缩；min/max 点字段 gather 可单独诊断 | 无现有 RVV；不能把模型距离 RVV 外推到本文件 | 主成本先由 `sac_model_->getDistancesToModel` 生成 distances；`exp/log` 近似和规约顺序风险；median 依赖排序/选择算法 | `plane_models` 有 MLESAC 回归；可做 test-only component ablation，但需要真实 profile 判断收益 | 保留 | 顺序 12：仅在模型距离核优化后仍显示 MLESAC 后处理占比时启动。 |

## 6. 暂缓或不推荐考虑 RVV 优化的文件

| 文件路径 | 关键入口 / 函数族 | 主题簇 | 主成本覆盖类型 | trip count / 数据规模来源 | RVV 适配点 | 现有 RVV 覆盖情况 | 主要风险 | 测试 / bench 可行性 | 第二轮去向 | 不单独实施理由 / 重新考虑条件 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_ellipse3d.hpp` | `getDistancesToModel` 246-307、`selectWithinDistance` 310-372、`countWithinDistance` 375-427；`dvec2ellipse` 在 `sac_model_ellipse3d.h` 103-150 | 复杂曲线模型：ellipse3d | `diagnostic` | `indices_->size()` | 外层可做 x/y/z gather、局部坐标 3x3 变换、threshold mask | 无现有 RVV | 核心距离 helper 每点进入 `atan2`、四象限分支和 golden-section `while` 搜索；外层向量化无法覆盖主成本；`select` 中每点重建 params | `quadric_models` 有 Ellipse3D RANSAC，但 direct oracle 和性能隔离不足 | 暂缓 | 当前不建议独立 RVV。重新考虑条件：先重构/证明 `dvec2ellipse` 可批处理，或 profile 显示外层局部变换而非 helper 占主成本。 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_torus.hpp` | `getDistancesToModel` 273-297、`selectWithinDistance` 300-332、`countWithinDistance` 335-360；`projectPointToTorus` 413 起；LM 优化 363-408 | 复杂曲面模型：torus | `diagnostic` | `indices_->size()` | 外层点/法线 gather 和 threshold mask 可批处理 | 无现有 RVV | 每点调用 `projectPointToTorus`，其几何投影依赖点法线和模型系数；优化为 Eigen LM；现有多种 torus RANSAC case 更适合 correctness 回归而非直接 RVV | `quadric_models` 有多组 torus RANSAC/QEMU/board 日志，但没有局部 helper bench | 暂缓 | 不单独实施。重新考虑条件：先对 `projectPointToTorus` 做数学/性能拆解，证明可 VLA 化的局部核占主成本。 |
| `sample_consensus/include/pcl/sample_consensus/impl/sac_model_registration_2d.hpp` | `getDistancesToModel` 66-117、`selectWithinDistance` 120-177、`countWithinDistance` 180-234 | correspondence registration-2d | `diagnostic` | `indices_->size()` 且等于 `indices_tgt_->size()` | source gather、4x4 transform、3x3 projection、uv divide、target `u/v` gather、threshold mask | 无现有 RVV | `uv[2] < 0` 分支造成 skipped distance；除法和 target 2D field layout 需要额外 traits；registration 主题归属不清 | 当前 test-rvv 未见 Registration2D direct tests；需新 synthetic camera/correspondence 数据 | 暂缓 | 暂不推荐独立实施。若 3D registration 已证明价值且有 2D 使用 profile，再复筛。 |
| `sample_consensus/include/pcl/sample_consensus/impl/msac.hpp` | `computeModel` 中 distances penalty 100-101、inlier count 112-116、final inlier 压缩 154-162 | SAC 方法后处理：MSAC | `tail-compress` | 外层 RANSAC 迭代数 × `distances.size()`；`distances` 来自派生模型 | 连续 double min/sum、threshold count、indices compress | 无现有 RVV | 主成本通常在 `sac_model_->getDistancesToModel`；外层随机采样/早停/虚调用不可向量化；浮点规约顺序影响 penalty | `plane_models` 有 MSAC 回归，可做后处理 ablation | 暂缓 | 不单独实施。只有在模型距离已优化且 profile 显示 MSAC 后处理占比高时复筛。 |
| `sample_consensus/include/pcl/sample_consensus/impl/rmsac.hpp` | `computeModel` 中 random pre-test 94-106、distances penalty 118-134、final inlier 压缩 171-179 | SAC 方法后处理：RMSAC | `tail-compress` | 外层 RANSAC 迭代数 × `distances.size()`，另有随机 pre-test 子集 | 连续 double min/sum、threshold count、indices compress | 无现有 RVV | 随机子集验证和模型距离主导；本地循环覆盖尾段，规约顺序和早停影响 k 更新 | `plane_models` 有 RMSAC 回归，可诊断但难证明生产价值 | 暂缓 | 不单独实施。复筛条件同 MSAC，并需隔离 pre-test 对总耗时的影响。 |
| `sample_consensus/include/pcl/sample_consensus/sac.h` | `refineModel` 188-275：循环调用 optimize/select，比较 inlier 序列 248-257 | SAC 基类控制 | `non-standalone` | `max_iterations` × `inliers_.size()`，但每轮主成本下沉到派生模型 | inlier 序列等值比较理论可批量化 | 无现有 RVV | 虚调用、收敛状态、threshold 更新、variance 和 oscillation 检测主导；批量比较只是控制尾段 | 上游 SAC 回归可覆盖，但无法独立证明 RVV 收益 | 暂缓 | 不单独实施。应随派生模型 select/optimize 主题自然受益，除非 profile 明确指向 inlier 序列比较。 |

## 7. 执行清单 / 状态表

### 7.1 建议进行 RVV 优化的文件

| 顺序 | 主题 | 主文件 | 推荐入口 / 第一 RVV 目标 | 状态 | 当前结论 / 下一步条件 |
| ---: | --- | --- | --- | --- | --- |
| 1 | normal-plane 已有 RVV 收敛 | `impl/sac_model_normal_plane.hpp` | 三入口 correctness、fallback、QEMU objdump、板卡 compare、文档边界 | phase 000 positive；phase 010 structure closed；phase 020 evidence registry closed；phase 030 repeated board positive-stable；phase 040 representative AoS source correctness closed；phase 050 representative source performance positive-stable；phase 060 representative normal layout correctness closed；phase 070 representative cross correctness closed | 已确认 normal-plane 三入口已有 RVV patch 可保留：`PointXYZ + Normal` 的公开入口、fallback、helper 缓冲区、QEMU、反汇编、板卡和 Evidence Doctor 已闭合。phase 010 已关闭 `src/` source layout、board fixture 默认参数和 topic-local doc suite；phase 020 已关闭 manifest wrapper、doctor alias 和 registry freshness check；phase 030 已生成 `PointXYZ + Normal` 5-run repeated board summary，三条 helper 的 median/min 均超过 positive-stable 阈值；phase 040 已把 public dispatch 收紧到 source AoS byte-offset gate，并补 `PointXYZI` / `PointXYZINormal` 代表点型 correctness 和 non-AoS source fallback；phase 050 已补两个代表 source 点型的 dedicated helper performance，`PointXYZI` 三项 median 为 8.04x / 6.10x / 5.41x，`PointXYZINormal` 三项 median 为 7.37x / 8.52x / 8.44x；phase 060 已补 `PointNormal` / `PointXYZINormal` normal cloud public correctness 和 non-AoS normal fallback；phase 070 已补 `PointXYZI` / `PointXYZINormal` source 与 `PointNormal` / `PointXYZINormal` normal cloud 的 4 个代表性交叉组合 public correctness。 |
| 2 | plane select/getDistances 扩展 | `impl/sac_model_plane.hpp` | `selectWithinDistance`、`countWithinDistance`、`getDistancesToModel` 的基础平面距离 kernel | production adopted；Phase 010 narrow adopted；Phase 020/025 correctness adopted；Phase 030 ready_for_topic_commit | 已采纳 `PointXYZ` / registered float xyz AoS / direct indexed 范围的 production RVV patch；Phase 000 5-run board 三入口 positive。Phase 010 只为 select/count 采纳 identity-index strided load，identity medians 为 3.3896x / 2.1969x，shuffled medians 为 3.1895x / 1.6663x；getDistances identity 分支已拒绝并保持 gather-only。Phase 020 补 `PointXYZI`、`PointXYZRGB/RGBA`、`PointXYZINormal` correctness；Phase 025 补显式空 `indices_` correctness；QEMU Std/RVV 各 7 个 gtest 和板卡 RVV gtest 均通过。当前无新的未阻塞性能优化候选，可以结束当前 topic 优化工作。 |
| 3 | sphere shell 扩展 | `impl/sac_model_sphere.hpp` | `selectWithinDistance` / `getDistancesToModel` shell + sqrt 路径 | production adopted for select；getDistances rejected/deferred | 独立 topic 位于 `test-rvv/sample_consensus/sac_model_sphere/`。Phase 020 已把 `selectWithinDistance` 接入 production，Std/RVV 各 4 个 gtest 通过，`selectWithinDistanceRVV` 反汇编归属 17 条 RVV 指令，5-run board median `1.5020x`。`countWithinDistance` 回归 median `3.5177x`；当前 `getDistancesToModel` scratch + scalar sqrt 候选 median `0.7775x`，保持标量。后续可选 `040-select-vcompress-ablation` 或 `050-point-type-expansion`；恢复 `getDistancesToModel` 需先做 `030-sqrt-helper-audit`。 |
| 4 | circle2d shell 扩展 | `impl/sac_model_circle.hpp` | `selectWithinDistance` / `getDistancesToModel` x/y shell | 候选 | 与 sphere 共用策略，单独验证 x/y field layout 和 exact error distance。 |
| 5 | line 几何核 | `impl/sac_model_line.hpp` | `countWithinDistance` cross3/squaredNorm | 新增候选 | 先做 direct count 对拍；通过后再评估 select compress 和 getDistances dense store。 |
| 6 | stick 几何核 | `impl/sac_model_stick.hpp` | `countWithinDistance` 双计数语义 | 新增候选 | 复用 line 公式但不复用 line 语义结论；必须覆盖 `nr_i/nr_o` 边界。 |

### 7.2 保留实施的候选文件

| 顺序 | 主题 | 主文件 | 推荐入口 / 第一 RVV 目标 | 状态 | 当前结论 / 下一步条件 |
| ---: | --- | --- | --- | --- | --- |
| 7 | cylinder 法线距离诊断 | `impl/sac_model_cylinder.hpp` | count/select 的 weighted euclid + normal angle component ablation | 保留 | 等 normal-plane 数值策略稳定后启动 production-shaped diagnostic。 |
| 8 | normal-sphere 法线距离诊断 | `impl/sac_model_normal_sphere.hpp` | count/select/getDistances 球面法线核 | 保留 | 先构造法线方向和球心退化边界，再评估 RVV。 |
| 9 | circle3d 投影核诊断 | `impl/sac_model_circle3d.hpp` | count/select 3D 投影距离核 | 保留 | 需要证明 normalize/divide 与退化条件可控。 |
| 10 | cone 法线距离诊断 | `impl/sac_model_cone.hpp` | cone count/select component ablation | 保留 | 依赖 cylinder 结论；否则暂缓。 |
| 11 | registration 归属/profile | `impl/sac_model_registration.hpp` | correspondence transform 距离核 | 保留 | 先确认 sample_consensus 归属和真实热度，避免与 registration 模块重复。 |
| 12 | MLESAC 后处理诊断 | `impl/mlesac.hpp` | EM probability/gamma/log-likelihood component ablation | 保留 | 仅在模型距离核优化后仍是热点时启动。 |

### 7.3 暂缓或不推荐考虑 RVV 优化的文件

| 顺序 | 主题 | 主文件 | 推荐入口 / 第一 RVV 目标 | 状态 | 当前结论 / 下一步条件 |
| ---: | --- | --- | --- | --- | --- |
| 13 | ellipse3d 暂缓复核 | `impl/sac_model_ellipse3d.hpp` | `dvec2ellipse` 可批处理性 | 暂缓 | 需要 helper 重构或 profile 证据。 |
| 14 | torus 暂缓复核 | `impl/sac_model_torus.hpp` | `projectPointToTorus` 可批处理性 | 暂缓 | 需要数学 helper 拆解和局部 bench。 |
| 15 | registration2d 暂缓复核 | `impl/sac_model_registration_2d.hpp` | projection distance kernel | 暂缓 | 等 3D registration 价值成立且有 2D profile 后复筛。 |
| 16 | MSAC 后处理暂缓 | `impl/msac.hpp` | distances min/sum/count/compress | 暂缓 | 只作为模型距离 topic 的伴随 ablation。 |
| 17 | RMSAC 后处理暂缓 | `impl/rmsac.hpp` | distances min/sum/count/compress | 暂缓 | 只作为模型距离 topic 的伴随 ablation。 |
| 18 | SAC refine 控制暂缓 | `sac.h` | inlier 序列比较 | 暂缓 | 当前为 non-standalone；随派生模型主题观察即可。 |

阶段状态：

| 阶段 | 状态 | 说明 |
| --- | --- | --- |
| 模块文件候选筛选 | 已完成 | 第一轮文档 high=8、mid=10、low=52。 |
| 函数评估队列 | 已完成 | 本文档覆盖 18 个 high/mid 去向，未补入 low。 |
| RVV 实现 | normal-plane 已推进；sphere select 已接 production | normal-plane 已有 production patch 继续完善；sphere Phase 020 已修改 `sac_model_sphere.hpp` / `.h`，当前采纳 `selectWithinDistance` production RVV。 |
| 专项测试 | normal-plane 已推进；sphere topic 已启动 | `test-rvv/sample_consensus/plane_models` 已补 Phase 040 source public / fallback 测试、Phase 050 representative source bench target、Phase 060 normal layout public / fallback 测试和 Phase 070 representative source × normal cross public tests；`test-rvv/sample_consensus/sac_model_sphere/` 已建立独立 test/bench/board/manifest 和 topic-local doc suite。 |
| QEMU / 反汇编 / 板卡 bench | normal-plane 已推进；sphere Phase 020 已完成 production select 证据 | normal-plane 性能引用 Phase 030 `PointXYZ + Normal` repeated board summary 和 Phase 050 representative source repeated board summary；sphere 已完成 QEMU correctness、RVV bench asm dump、board smoke、production 5-run repeated manifest / Evidence Doctor / registry。`selectWithinDistance` production row 支撑采纳；`getDistancesToModel` 仍保持标量。 |
| 主题文档 / 工作日志 | plane / normal-plane / sphere 已有 topic-local 文档 | plane 已创建 `doc-rvv/sample_consensus/sac_model_plane-RVV.zh.md` 并同步 Phase 010；后续逐文件 topic 由 RVV workflow 继续。 |
| normal-plane phase 000 | 已完成 | `test-rvv/sample_consensus/plane_models/doc/phases/000-normal-plane-current-state-and-public-entry-boundary/result.zh.md` 记录 positive closeout。 |
| normal-plane phase 010 | 已完成 | `test-rvv/sample_consensus/plane_models/doc/phases/010-normal-plane-test-support-structure/result.zh.md` 记录 source layout、board fixture 默认参数和 doc-suite role split closeout；默认进入 `020-normal-plane-evidence-registry-target-alias`。 |
| normal-plane phase 020 | 已完成 | `test-rvv/sample_consensus/plane_models/doc/phases/020-normal-plane-evidence-registry-target-alias/result.zh.md` 记录 manifest wrapper、Evidence Doctor / registry alias 和 `evidence_status=fresh`；默认进入 `030-normal-plane-repeated-board-summary`。 |
| normal-plane phase 030 | 已完成 | `test-rvv/sample_consensus/plane_models/doc/phases/030-normal-plane-repeated-board-summary/result.zh.md` 记录 5-run repeated board summary、Evidence Doctor 和 registry freshness；当前 bucket 为 `positive-stable`。 |
| normal-plane phase 040 | 已完成 | `test-rvv/sample_consensus/plane_models/doc/phases/040-normal-plane-aospoint-gate-expansion/result.zh.md` 记录 source AoS gate、`PointXYZI` / `PointXYZINormal` 代表性 source correctness、non-AoS source fallback 和板卡 public alias 6/6 通过；不新增性能 summary。 |
| normal-plane phase 050 | 已完成 | `test-rvv/sample_consensus/plane_models/doc/phases/050-normal-plane-representative-aos-source-performance/result.zh.md` 记录 `PointXYZI` / `PointXYZINormal` 代表性 source protected helper performance，5-run board summary 全部 positive-stable；Evidence Doctor 为 Errors=0、Warnings=5、Suggestions=0，Warnings 已解释。 |
| normal-plane phase 060 | 已完成 | `test-rvv/sample_consensus/plane_models/doc/phases/060-normal-plane-normal-layout-expansion/result.zh.md` 记录 `PointNormal` / `PointXYZINormal` normal cloud public correctness、non-AoS registered normal fallback 和板卡 public alias 9/9 通过；不新增性能 summary。 |
| normal-plane phase 070 | 已完成 | `test-rvv/sample_consensus/plane_models/doc/phases/070-normal-plane-cross-point-type-layout/result.zh.md` 记录 `PointXYZI` / `PointXYZINormal` source 与 `PointNormal` / `PointXYZINormal` normal cloud 的 4 个代表性交叉组合 public correctness，QEMU/RVV public alias 13/13、Std/RVV 各 32/32、板卡 public alias 13/13 通过；不新增性能 summary。 |

## 8. Closeout

- 输出文档：`doc-rvv/library-screening/sample_consensus/sample_consensus-function-evaluation-queue.zh.md`
- 三类队列数量如下：

| 队列 | 数量 |
| --- | ---: |
| 建议进行 RVV 优化的文件 | 6 |
| 保留实施的候选文件 | 6 |
| 暂缓或不推荐考虑 RVV 优化的文件 | 6 |

- 第一条未完成主题：`impl/sac_model_normal_plane.hpp` 的已有 RVV 收敛和边界确认。
- 当前未发现需要补充到 workflow、rvv-test、implementation 或 documentation skill 的通用新规则；后续 topic 应继续严格区分“已有 RVV count 覆盖”和“select/getDistances 新增覆盖”。
