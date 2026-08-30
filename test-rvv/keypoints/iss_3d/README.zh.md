# ISS 3D RVV Topic Navigation

## 当前结论

`ISSKeypoint3D::getScatterMatrix` 的 f64 vector reduction scatter（双精度向量规约散布矩阵）在 component ablation（组件消融）边界上有明确板卡收益：`iss_3d_phase000_scatter_f64_rerun1` 的 indexed 256 邻域 median speedup 为 1.662x，contiguous 256 为 1.859x，tail 73 为 1.213x。

真实 public compute（公开 `compute()` 入口）边界没有达到采纳条件。微调 production helper 后，`iss_3d_phase010_production_public_reduction_shape` 的 median speedup 为 1.014x，5/5 正向但 decision bucket（决策桶）仍是 `neutral`，Evidence Doctor（证据体检）报告 Errors=0、Warnings=0、Suggestions=3。这个结果只能说明 bounded production probe（有界生产探针）弱正向，不足以证明值得长期接入。

因此当前不创建 `doc-rvv/keypoints/iss_3d-RVV.zh.md`。`keypoints/include/pcl/keypoints/impl/iss_3d.hpp` 已回到原标量生产路径，production probe（生产探针）只作为历史证据保留在 topic-local 文档和 summary evidence（摘要证据）中。

## 推荐阅读顺序

1. `doc/iss_3d-evaluation.zh.md`：函数级评估、EvidenceDecision（证据决策）和 Traceability Map（可追踪性地图）。
2. `doc/phases/010-production-integration/result.zh.md`：production probe 的真实测试、板卡结果和不采纳理由。
3. `doc/benchmark-and-evidence.zh.md`：bench case、run label、Evidence Doctor 和 evidence registry（证据登记表）。
4. `doc/test-support-code-map.zh.md`：测试支撑代码、脚本和 production helper 的定位关系。
5. `doc/phases/optimization-matrix.zh.md` 与 `doc/optimization-roadmap.zh.md`：候选状态和恢复队列。

## 常用命令

| 目的 | 命令 |
| --- | --- |
| QEMU correctness（QEMU 正确性验证，不代表真实性能） | `make -C test-rvv/keypoints/iss_3d run_test_compare` |
| 诊断 helper 反汇编归属 | `make -C test-rvv/keypoints/iss_3d check_iss_3d_rvv_asm` |
| 证据新鲜度检查 | `make -C test-rvv/keypoints/iss_3d check_evidence_freshness` |

板卡命令需要使用本机配置和 `SSH_AUTH_SOCK`。文档只记录环境变量名，不记录私有地址或个人路径。

## 证据白名单

当前结论引用这些 summary / doctor 文件；raw run logs、build 输出和 board 私有路径不默认提交。production public run 是历史探针证据，当前源码不再包含该 production RVV path（RVV 生产路径）。

| run label | evidence role | summary | doctor |
| --- | --- | --- | --- |
| `iss_3d_phase000_scatter_f64_repeated` | diagnostic historical unstable | `log/board/repeated_phase000_scatter_f64/summary.md` | `log/board/repeated_phase000_scatter_f64/evidence_doctor.md` |
| `iss_3d_phase000_scatter_f64_rerun1` | diagnostic current positive | `log/board/repeated_phase000_scatter_f64_rerun1/summary.md` | `log/board/repeated_phase000_scatter_f64_rerun1/evidence_doctor.md` |
| `iss_3d_phase010_production_public_repeated` | production public historical neutral | `log/board/repeated_phase010_production_public/summary.md` | `log/board/repeated_phase010_production_public/evidence_doctor.md` |
| `iss_3d_phase010_production_public_reduction_shape` | production public current neutral | `log/board/repeated_phase010_production_public_reduction_shape/summary.md` | `log/board/repeated_phase010_production_public_reduction_shape/evidence_doctor.md` |

## 产物提交边界

可审查 topic 产物包括 `test-rvv/keypoints/iss_3d/` 下的源码、Makefile、topic-local 文档和 summary evidence。`log/qemu/`、`log/board/run_*`、`build/`、raw board 目录和个人配置不默认提交。当前未提交的 `sift_keypoint` 相关文件、recognition 筛选文档和其它 topic 修改不属于本 topic。
