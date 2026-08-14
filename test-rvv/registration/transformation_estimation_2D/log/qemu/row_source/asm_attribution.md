# transformation_estimation_2D ASM Attribution Summary

## 结论

当前 RVV bench binary 中，production public case 的关键指令优先归属到 `runPublicCase` lambda 内联边界，或 exact `PointXYZ` ordered-cloud-pair 公开 overload 的 production symbol；test-only fused candidate 仍归属到 `runFusedCase` lambda 内联边界；row-source candidate 归属到 `runRowSourceCase` 或具体 row-source wrapper lambda 内联边界。需要结合 case-filter 和 production direct tests 判断证据角色。

- attribution_decision：`row_source_lambda_boundary_present_with_candidate_boundary_with_other_rvv_boundaries`
- std RVV lines：1003
- rvv RVV lines：1078
- delta：+75

## 关键指令归属

| category | RVV lines | key mnemonics | boundary |
| --- | ---: | --- | --- |
| `eigen_or_stdlib` | 659 | vfredosum=9, vfadd=8, vsetvli=35 | `Eigen::internal::gebp_kernel<float, float, long, Eigen::internal::blas_data_mapper<float, long, 0, 0, 1>, 24, 4, false, false>::operator()(Eigen::internal::blas_data_mapper<float, long, 0, 0, 1> const&, float const*, float const*, long, long, long, float, long, long, long, long) const [clone .constprop.0]` |
| `bench_entry` | 157 | vsetvli=26 | `main` |
| `test_support_fixture` | 128 | vlsseg3e32.v=4, vfmacc=4, vfredosum=24, vfsub=12, vfadd=4, vsetvli=14 | `Eigen::Matrix<float, 4, 4, 0, 4, 4> pcl::registration::rvv_te2d_support::estimateFused2DCandidate<pcl::PointXYZ, pcl::PointXYZ>(pcl::PointCloud<pcl::PointXYZ> const&, pcl::PointCloud<pcl::PointXYZ> const&, pcl::registration::rvv_te2d_support::CandidateStats*)` |
| `production_scalar_boundary` | 73 | vfredosum=4, vsetvli=9 | `pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>::getTransformationFromCorrelation(Eigen::Matrix<float, -1, -1, 0, -1, -1> const&, Eigen::Matrix<float, 4, 1, 0, 4, 1> const&, Eigen::Matrix<float, -1, -1, 0, -1, -1> const&, Eigen::Matrix<float, 4, 1, 0, 4, 1> const&, Eigen::Matrix<float, 4, 4, 0, 4, 4>&) const [clone .isra.0]` |
| `row_source_lambda_boundary` | 51 | vsetvli=5 | `std::_Function_handler<unsigned long (), (anonymous namespace)::runSourceIndexedCase<pcl::PointCloud<pcl::PointXYZ> >(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, pcl::PointCloud<pcl::PointXYZ> const&, std::vector<int, std::allocator<int> > const&, pcl::PointCloud<pcl::PointXYZ> const&, int, int)::{lambda()#1}>::_M_invoke(std::_Any_data const&)` |
| `candidate_lambda_boundary` | 4 | vsetvli=1 | `std::_Function_handler<unsigned long (), (anonymous namespace)::runFusedCase<pcl::PointCloud<pcl::PointXYZ> >(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, pcl::PointCloud<pcl::PointXYZ> const&, pcl::PointCloud<pcl::PointXYZ> const&, int, int)::{lambda()#1}>::_M_invoke(std::_Any_data const&)` |
| `production_public_lambda_boundary` | 3 | vsetvli=1 | `std::_Function_handler<unsigned long (), (anonymous namespace)::runPublicCase<pcl::PointCloud<pcl::PointXYZ> >(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, pcl::PointCloud<pcl::PointXYZ> const&, pcl::PointCloud<pcl::PointXYZ> const&, int, int)::{lambda()#1}>::_M_invoke(std::_Any_data const&)` |
| `production_public_boundary` | 3 | vsetvli=1 | `pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>::estimateRigidTransformation(pcl::PointCloud<pcl::PointXYZ> const&, pcl::PointCloud<pcl::PointXYZ> const&, Eigen::Matrix<float, 4, 4, 0, 4, 4>&) const` |

## RVV build top mnemonics

| mnemonic | count |
| --- | ---: |
| `vle32.v` | 245 |
| `vse8.v` | 163 |
| `vle8.v` | 142 |
| `vse32.v` | 101 |
| `vsetvli` | 92 |
| `vmv.v.` | 68 |
| `vfmv.f.s` | 38 |
| `vfredosum` | 37 |
| `vfmv.v.f` | 30 |
| `vse64.v` | 29 |
| `vfmul` | 25 |
| `vfmv.s.f` | 21 |
| `vmul.vx` | 12 |
| `vadd.vx` | 12 |
| `vfadd` | 12 |
| `vfsub` | 12 |
| `vlse32.v` | 10 |
| `vlseg4e32.v` | 9 |
| `vfirst` | 7 |
| `vlsseg3e32.v` | 4 |

## Top symbols

| category | symbol | RVV lines | sample |
| --- | --- | ---: | --- |
| `eigen_or_stdlib` | `Eigen::internal::gebp_kernel<float, float, long, Eigen::internal::blas_data_mapper<float, long, 0, 0, 1>, 24, 4, false, false>::operator()(Eigen::internal::blas_data_mapper<float, long, 0, 0, 1> const&, float const*, float const*, long, long, long, float, long, long, long, long) const [clone .constprop.0]` | 430 | `17d48:	5e003557          	vmv.v.i	v10,0; 17d4c:	5e0455d7          	vfmv.v.f	v11,fs0` |
| `eigen_or_stdlib` | `Eigen::internal::general_matrix_vector_product<long, float, Eigen::internal::const_blas_data_mapper<float, long, 0>, 0, false, float, Eigen::internal::const_blas_data_mapper<float, long, 1>, false, 0>::run(long, long, Eigen::internal::const_blas_data_mapper<float, long, 0> const&, Eigen::internal::const_blas_data_mapper<float, long, 1> const&, float*, long, float) [clone .isra.0]` | 84 | `19dc0:	5e0451d7          	vfmv.v.f	v3,fs0; 19eae:	5e0030d7          	vmv.v.i	v1,0` |
| `eigen_or_stdlib` | `void Eigen::internal::generic_product_impl<Eigen::Matrix<float, -1, -1, 0, -1, -1>, Eigen::Transpose<Eigen::Matrix<float, -1, -1, 0, -1, -1> const>, Eigen::DenseShape, Eigen::DenseShape, 8>::scaleAndAddTo<Eigen::Matrix<float, -1, -1, 0, -1, -1> >(Eigen::Matrix<float, -1, -1, 0, -1, -1>&, Eigen::Matrix<float, -1, -1, 0, -1, -1> const&, Eigen::Transpose<Eigen::Matrix<float, -1, -1, 0, -1, -1> const> const&, float const&)` | 56 | `1b952:	5e0030d7          	vmv.v.i	v1,0; 1b974:	020ef0a7          	vse64.v	v1,(t4)` |
| `eigen_or_stdlib` | `(anonymous namespace)::caseEnabled(int, char**, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&)` | 15 | `15104:	0c007957          	vsetvli	s2,zero,e8,m1,ta,ma; 15116:	4218a6d7          	vfirst.m	a3,v1` |
| `eigen_or_stdlib` | `Eigen::internal::gemm_pack_lhs<float, long, Eigen::internal::const_blas_data_mapper<float, long, 0>, 24, 8, Eigen::internal::eigen_packet_wrapper<__rvv_float32m1_t, 6>, 0, false, false>::operator()(float*, Eigen::internal::const_blas_data_mapper<float, long, 0> const&, long, long, long, long) const [clone .constprop.0]` | 15 | `177de:	0208e187          	vle32.v	v3,(a7); 177f0:	0208e107          	vle32.v	v2,(a7)` |
| `bench_entry` | `main` | 157 | `12ecc:	02078087          	vle8.v	v1,(a5); 12eee:	020500a7          	vse8.v	v1,(a0)` |
| `test_support_fixture` | `Eigen::Matrix<float, 4, 4, 0, 4, 4> pcl::registration::rvv_te2d_support::estimateFused2DCandidate<pcl::PointXYZ, pcl::PointXYZ>(pcl::PointCloud<pcl::PointXYZ> const&, pcl::PointCloud<pcl::PointXYZ> const&, pcl::registration::rvv_te2d_support::CandidateStats*)` | 121 | `1c11c:	5e0030d7          	vmv.v.i	v1,0; 1c126:	020680a7          	vse8.v	v1,(a3)` |
| `test_support_fixture` | `pcl::PointCloud<pcl::PointXYZ> pcl::registration::rvv_te2d_support::transformCloud2D<pcl::PointXYZ>(pcl::PointCloud<pcl::PointXYZ> const&, Eigen::Matrix<float, 4, 4, 0, 4, 4> const&)` | 7 | `1ac82:	0c07f757          	vsetvli	a4,a5,e8,m1,ta,ma; 1ac86:	02068087          	vle8.v	v1,(a3)` |
| `production_scalar_boundary` | `pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>::getTransformationFromCorrelation(Eigen::Matrix<float, -1, -1, 0, -1, -1> const&, Eigen::Matrix<float, 4, 1, 0, 4, 1> const&, Eigen::Matrix<float, -1, -1, 0, -1, -1> const&, Eigen::Matrix<float, 4, 1, 0, 4, 1> const&, Eigen::Matrix<float, 4, 4, 0, 4, 4>&) const [clone .isra.0]` | 67 | `15508:	5e0030d7          	vmv.v.i	v1,0; 15512:	0207f0a7          	vse64.v	v1,(a5)` |
| `production_scalar_boundary` | `pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>::estimateRigidTransformation(pcl::ConstCloudIterator<pcl::PointXYZ>&, pcl::ConstCloudIterator<pcl::PointXYZ>&, Eigen::Matrix<float, 4, 4, 0, 4, 4>&) const [clone .isra.0]` | 3 | `160a6:	5e0030d7          	vmv.v.i	v1,0; 160ae:	020400a7          	vse8.v	v1,(s0)` |
| `production_scalar_boundary` | `pcl::ConstCloudIterator<pcl::PointXYZ>::ConstCloudIterator(pcl::PointCloud<pcl::PointXYZ> const&, std::vector<int, std::allocator<int> > const&)` | 3 | `1c9bc:	0c07f6d7          	vsetvli	a3,a5,e8,m1,ta,ma; 1c9c0:	02070087          	vle8.v	v1,(a4)` |
| `row_source_lambda_boundary` | `std::_Function_handler<unsigned long (), (anonymous namespace)::runSourceIndexedCase<pcl::PointCloud<pcl::PointXYZ> >(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, pcl::PointCloud<pcl::PointXYZ> const&, std::vector<int, std::allocator<int> > const&, pcl::PointCloud<pcl::PointXYZ> const&, int, int)::{lambda()#1}>::_M_invoke(std::_Any_data const&)` | 18 | `165be:	5e0031d7          	vmv.v.i	v3,0; 165ce:	5e0030d7          	vmv.v.i	v1,0` |
| `row_source_lambda_boundary` | `std::_Function_handler<unsigned long (), (anonymous namespace)::runDualIndexedCase<pcl::PointCloud<pcl::PointXYZ> >(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, pcl::PointCloud<pcl::PointXYZ> const&, std::vector<int, std::allocator<int> > const&, pcl::PointCloud<pcl::PointXYZ> const&, std::vector<int, std::allocator<int> > const&, int, int)::{lambda()#1}>::_M_invoke(std::_Any_data const&)` | 17 | `16b32:	5e0030d7          	vmv.v.i	v1,0; 16b4c:	02076187          	vle32.v	v3,(a4)` |
| `row_source_lambda_boundary` | `std::_Function_handler<unsigned long (), (anonymous namespace)::runCorrespondenceCase<pcl::PointCloud<pcl::PointXYZ> >(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, pcl::PointCloud<pcl::PointXYZ> const&, pcl::PointCloud<pcl::PointXYZ> const&, std::vector<pcl::Correspondence, Eigen::aligned_allocator<pcl::Correspondence> > const&, int, int)::{lambda()#1}>::_M_invoke(std::_Any_data const&)` | 16 | `16fbc:	5e0030d7          	vmv.v.i	v1,0; 16fda:	020680a7          	vse8.v	v1,(a3)` |
| `candidate_lambda_boundary` | `std::_Function_handler<unsigned long (), (anonymous namespace)::runFusedCase<pcl::PointCloud<pcl::PointXYZ> >(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, pcl::PointCloud<pcl::PointXYZ> const&, pcl::PointCloud<pcl::PointXYZ> const&, int, int)::{lambda()#1}>::_M_invoke(std::_Any_data const&)` | 4 | `17490:	5e003157          	vmv.v.i	v2,0; 17494:	0d807057          	vsetvli	zero,zero,e64,m1,ta,ma` |
| `production_public_lambda_boundary` | `std::_Function_handler<unsigned long (), (anonymous namespace)::runPublicCase<pcl::PointCloud<pcl::PointXYZ> >(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, pcl::PointCloud<pcl::PointXYZ> const&, pcl::PointCloud<pcl::PointXYZ> const&, int, int)::{lambda()#1}>::_M_invoke(std::_Any_data const&)` | 3 | `16360:	0c077657          	vsetvli	a2,a4,e8,m1,ta,ma; 16364:	02068087          	vle8.v	v1,(a3)` |
| `production_public_boundary` | `pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>::estimateRigidTransformation(pcl::PointCloud<pcl::PointXYZ> const&, pcl::PointCloud<pcl::PointXYZ> const&, Eigen::Matrix<float, 4, 4, 0, 4, 4>&) const` | 3 | `1bff6:	0c077657          	vsetvli	a2,a4,e8,m1,ta,ma; 1bffa:	02068087          	vle8.v	v1,(a3)` |

## 边界说明

- `runPublicCase` lambda 或 exact `PointXYZ` ordered-cloud-pair 公开 overload 边界用于 production public smoke / board bench，PI2 后可作为 production helper 命中证据的一部分。
- `runFusedCase` lambda 内联边界对应 test-only fused candidate，不能替代 production public evidence。
- `runRowSourceCase` 或 source-indexed、dual-indexed、correspondence wrapper lambda 边界对应 materialize-to-ordered row-source candidate，不能替代 production public evidence。
- production fallback、Eigen / iterator 路径也可能含 RVV 指令；这些指令不能归入当前 production helper 收益。
- 本摘要只支持 asm attribution（反汇编归属），不支持目标硬件性能结论。
