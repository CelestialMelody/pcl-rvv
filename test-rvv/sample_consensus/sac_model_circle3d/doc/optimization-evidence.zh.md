# sac_model_circle3d Optimization Evidence

| 优化方式 | 代码位置 | correctness | bench / board | asm | decision | 边界 |
| --- | --- | --- | --- | --- | --- | --- |
| projection count candidate | `include/impl/sac_model_circle3d_candidates.hpp` | `run_test_compare` passed Std/RVV 2/2 | `projection-repeated-evidence-manifest.json`: B/A mean 0.5713，5/5 退化 | `check_projection_asm` passed | rejected with evidence | test-only component ablation；不接 production。 |
| projection select candidate | `include/impl/sac_model_circle3d_candidates.hpp` | `run_test_compare` passed Std/RVV 2/2 | `projection-repeated-evidence-manifest.json`: B/A mean 1.1294，5/5 正向 | `check_projection_asm` passed | partial-production-candidate gate | 只支持请求 select-only production probe；不能替代 production direct。 |
| select production RVV patch | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_circle3d.hpp` | `run_test_compare` passed Std/RVV 5/5；未采纳点型 fallback 测试通过 | `select-production-repeated-evidence-manifest.json`: B/A mean 0.9748，median 0.9991，5/10 退化 | `check_select_production_asm` passed | rollback/no-production | 生产补丁已回滚；该行只保留历史证据，不进入正式 `doc-rvv`。 |
| select point-type expansion | historical production public entry | `run_test_compare` passed Std/RVV 5/5；helper gate 对三点型返回 false | `PointXYZI` mean 0.9117；`PointXYZRGB` mean 0.8883；`PointXYZRGBA` mean 0.8828 | historical asm attribution only；production helper 已回滚 | rejected with evidence | 三点型保留标量 fallback；当前只保留历史证据。 |
| getDistances full-RVV sqrt/double store | not_yet_implemented | not_yet_covered | not_yet_covered | not_yet_covered | deferred | 需先审计 `getDistancesToModel` lambda 符号和独立数值预算；不能从 Phase 000 count 负向外推。 |

Evidence Doctor 主路径：

- `test-rvv/sample_consensus/sac_model_circle3d/doc/phases/000-circle3d-projection-component-ablation/projection-repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_circle3d/doc/phases/010-selectwithin-production-probe/select-production-repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_circle3d/doc/phases/020-select-point-type-expansion/select-xyzi-repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_circle3d/doc/phases/020-select-point-type-expansion/select-xyzrgb-repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_circle3d/doc/phases/020-select-point-type-expansion/select-xyzrgba-repeated-evidence-doctor.md`
