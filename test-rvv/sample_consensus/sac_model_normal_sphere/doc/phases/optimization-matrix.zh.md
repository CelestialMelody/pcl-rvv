# sac_model_normal_sphere 优化矩阵

## 当前矩阵结论

当前 EvidenceDecision（证据决策）是 `production-adopted`。Phase060 在接入后的 production-public（真实公开入口）
边界下完成四种 source 点型 × 三入口 5-run board repeated（重复板卡测试），12/12 comparison 全部 positive，
Evidence Doctor（证据体检）为 `Errors=0`、`Warnings=0`、`Suggestions=0`。Phase000-030 的 diagnostic（诊断）
矩阵行保留为历史输入，不作为当前生产性能 truth。

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production RVV count | indexed source + indexed normal | `PointXYZ/PointXYZI/PointXYZRGB/PointXYZRGBA + pcl::Normal` / float xyz+normal / AoS | `countWithinDistance` public entry and RVV detail helper | `run_test_compare` passed; `ProductionRVVDetailHelpersMatchPublicReference`; fallback tests | Phase060 production-public count label | medians `3.332x` / `3.358x` / `3.308x` / `3.411x` | `computeNormalSphereDistanceRVV` and count helper contain `vfsqrt.v` / `vcpop.m` | Phase060 0/0/0 | adopted | none in current scope |
| production RVV select | indexed source + indexed normal | same | `selectWithinDistance` public entry and RVV detail helper | `run_test_compare` passed; select order and error output covered | Phase060 production-public select label | medians `3.039x` / `2.900x` / `2.973x` / `2.906x` | select helper contains `vcompress.vm`, `vfwcvt.f.f.v`, `vse64.v` | Phase060 0/0/0 | adopted | none in current scope |
| production RVV dense distances | indexed source + indexed normal | same | `getDistancesToModel` public entry and RVV detail helper | `run_test_compare` passed; dense output size/order covered | Phase060 production-public getDistances label | medians `4.456x` / `4.157x` / `4.290x` / `4.161x` | getDistances helper contains `vfwcvt.f.f.v` / `vse64.v` | Phase060 0/0/0 | adopted | none in current scope |
| Phase000 RVV count early-mask | indexed source + indexed normal | `PointXYZ/PointXYZI + Normal` / float fields / AoS | `countWithinDistance` production-shaped diagnostic（生产形态诊断） | historical `run_test_compare` passed | `bench_sac_model_normal_sphere PointXYZ/PointXYZI` | single board smoke `6.177x` / `5.126x` | test-only candidate asm | `low_run_count` Warning | historical positive diagnostic | closed by Phase060 production evidence |
| Phase000 RVV select scalar writeback | indexed source + indexed normal | `PointXYZ/PointXYZI + Normal` / float fields / AoS | `selectWithinDistance` production-shaped diagnostic | historical `run_test_compare` passed | `bench_sac_model_normal_sphere PointXYZ/PointXYZI` | single board smoke `2.856x` / `2.375x` | test-only candidate asm | `low_run_count` Warning | historical positive diagnostic | superseded by `vcompress` production select |
| Phase010 `vcompress` select writeback | indexed source + indexed normal | `PointXYZ/PointXYZI/PointXYZRGB/PointXYZRGBA + Normal` / float fields / AoS | `selectWithinDistance` implementation-family ablation（实现族消融） | `VCompressSelectCandidatePreservesOrderAndErrors` | diagnostic vcompress label | PointXYZ B/A `1.256x`; PointXYZI B/A `1.180x`; RGB/RGBA positive smoke | `vcompress.vm` observed | `low_run_count` Warning | historical PI1 input | adopted through Phase060 production select |
| Phase020 getDistances dense output | indexed source + indexed normal | `PointXYZ/PointXYZI + Normal` / float input / double output / AoS | `getDistancesToModel` production-shaped diagnostic | dense output same-chain covered | diagnostic getDistances label | single board smoke `5.156x` / `4.526x` | `vfwcvt.f.f.v` / `vse64.v` in test-only candidate | `low_run_count` Warning | historical PI1 input | adopted through Phase060 production getDistances |
| Phase030 point type expansion | indexed source + indexed normal | `PointXYZRGB/PointXYZRGBA + Normal` / float input / AoS | RGB/RGBA source layout diagnostic | `PointXYZRGBAndRGBALayoutsMatchReference` | Phase030 all candidate labels | RGB/RGBA all positive smoke | same RVV helper instantiation boundary | `low_run_count` Warning | historical point-type input | adopted through Phase060 RGB/RGBA production-public evidence |
| structure-parity-doc-suite | not_applicable | current topic docs | topic-local docs and long-term `doc-rvv` | no new correctness target | no new bench target | reused Phase060 summary for closeout | no new asm | evidence_status fresh | adopted | none |
| other normal point types | indexed source + indexed normal | non-`pcl::Normal` normal-like types | not in current production gate | not run | not run | not run | not run | not run | deferred outside current scope | user-specified expansion phase |
| custom source point types | indexed source + indexed normal | custom registered PointXYZ-like source | not in current production evidence | not run | not run | not run | not run | not run | deferred outside current scope | user-specified workload or point type |
| `Scalar=double` / non-float field strategy | not_applicable | double coefficients or non-float point fields | outside current PCL entry shape | not run | not run | not run | not run | not run | not_applicable now | re-evaluate if upstream signatures or fields change |

## Ready For Review Validity Check

`ready_for_review_validity_check` 结果为 pass。当前 matrix、Phase060 result、roadmap、README、evaluation、
topic-local doc suite、Evidence Doctor 和 registry 在已授权 scope 内没有 `phase_deferred + unblocked` 项。
继续到其它 normal 点型、自定义点型、`Scalar=double`、identity-index 专门路径、真实 workload/profile 或其它硬件
都会改变 scope，需要新的 phase 或用户点名范围。
