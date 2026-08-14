# transformation_estimation_2D ASM Attribution Summary

## 结论

当前 RVV bench binary 中，production public case 的关键指令优先归属到 `runPublicCase` lambda 内联边界，或 exact `PointXYZ` ordered-cloud-pair 公开 overload 的 production symbol；test-only fused candidate 仍归属到 `runFusedCase` lambda 内联边界；row-source candidate 归属到 `runRowSourceCase` 或具体 row-source wrapper lambda 内联边界。需要结合 case-filter 和 production direct tests 判断证据角色。

- attribution_decision：`production_public_inline_boundary_present_with_candidate_boundary_with_other_rvv_boundaries`
- std RVV lines：1003
- rvv RVV lines：1114
- delta：+111

## 关键指令归属

| category | RVV lines | key mnemonics | boundary |
| --- | ---: | --- | --- |
| `eigen_or_stdlib` | 659 | vfredosum=9, vfadd=8, vsetvli=35 | `Eigen::internal::gebp_kernel<float, float, long, Eigen::internal::blas_data_mapper<float, long, 0, 0, 1>, 24, 4, false, false>::operator()(Eigen::internal::blas_data_mapper<float, long, 0, 0, 1> const&, float const*, float const*, long, long, long, float, long, long, long, long) const [clone .constprop.0]` |
| `bench_entry` | 157 | vsetvli=26 | `main` |
| `test_support_fixture` | 128 | vlsseg3e32.v=4, vfmacc=4, vfredosum=24, vfsub=12, vfadd=4, vsetvli=14 | `Eigen::Matrix<float, 4, 4, 0, 4, 4> pcl::registration::rvv_te2d_support::estimateFused2DCandidate<pcl::PointXYZ, pcl::PointXYZ>(pcl::PointCloud<pcl::PointXYZ> const&, pcl::PointCloud<pcl::PointXYZ> const&, pcl::registration::rvv_te2d_support::CandidateStats*)` |
| `production_scalar_boundary` | 73 | vfredosum=4, vsetvli=9 | `pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>::getTransformationFromCorrelation(Eigen::Matrix<float, -1, -1, 0, -1, -1> const&, Eigen::Matrix<float, 4, 1, 0, 4, 1> const&, Eigen::Matrix<float, -1, -1, 0, -1, -1> const&, Eigen::Matrix<float, 4, 1, 0, 4, 1> const&, Eigen::Matrix<float, 4, 4, 0, 4, 4>&) const [clone .isra.0]` |
| `row_source_lambda_boundary` | 51 | vsetvli=5 | `std::_Function_handler<unsigned long (), (anonymous namespace)::runSourceIndexedCase<pcl::PointCloud<pcl::PointXYZ> >(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, pcl::PointCloud<pcl::PointXYZ> const&, std::vector<int, std::allocator<int> > const&, pcl::PointCloud<pcl::PointXYZ> const&, int, int)::{lambda()#1}>::_M_invoke(std::_Any_data const&)` |
| `production_public_boundary` | 42 | vlsseg3e32.v=4, vfmacc=4, vfredosum=8, vfsub=4, vfadd=4, vsetvli=6 | `pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>::estimateRigidTransformation(pcl::PointCloud<pcl::PointXYZ> const&, pcl::PointCloud<pcl::PointXYZ> const&, Eigen::Matrix<float, 4, 4, 0, 4, 4>&) const` |
| `candidate_lambda_boundary` | 4 | vsetvli=1 | `std::_Function_handler<unsigned long (), (anonymous namespace)::runFusedCase<pcl::PointCloud<pcl::PointXYZ> >(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, pcl::PointCloud<pcl::PointXYZ> const&, pcl::PointCloud<pcl::PointXYZ> const&, int, int)::{lambda()#1}>::_M_invoke(std::_Any_data const&)` |

## RVV build top mnemonics

| mnemonic | count |
| --- | ---: |
| `vle32.v` | 245 |
| `vse8.v` | 162 |
| `vle8.v` | 141 |
| `vse32.v` | 101 |
| `vsetvli` | 96 |
| `vmv.v.` | 69 |
| `vfmv.f.s` | 46 |
| `vfredosum` | 45 |
| `vfmv.v.f` | 30 |
| `vse64.v` | 29 |
| `vfmul` | 25 |
| `vfmv.s.f` | 21 |
| `vfadd` | 16 |
| `vfsub` | 16 |
| `vmul.vx` | 12 |
| `vadd.vx` | 12 |
| `vlse32.v` | 10 |
| `vlseg4e32.v` | 9 |
| `vlsseg3e32.v` | 8 |
| `vfmacc` | 8 |

## Top symbols

| category | symbol | RVV lines | sample |
| --- | --- | ---: | --- |
| `eigen_or_stdlib` | `Eigen::internal::gebp_kernel<float, float, long, Eigen::internal::blas_data_mapper<float, long, 0, 0, 1>, 24, 4, false, false>::operator()(Eigen::internal::blas_data_mapper<float, long, 0, 0, 1> const&, float const*, float const*, long, long, long, float, long, long, long, long) const [clone .constprop.0]` | 430 | `17d6c:	5e003557          	vmv.v.i	v10,0; 17d70:	5e0455d7          	vfmv.v.f	v11,fs0` |
| `eigen_or_stdlib` | `Eigen::internal::general_matrix_vector_product<long, float, Eigen::internal::const_blas_data_mapper<float, long, 0>, 0, false, float, Eigen::internal::const_blas_data_mapper<float, long, 1>, false, 0>::run(long, long, Eigen::internal::const_blas_data_mapper<float, long, 0> const&, Eigen::internal::const_blas_data_mapper<float, long, 1> const&, float*, long, float) [clone .isra.0]` | 84 | `19de4:	5e0451d7          	vfmv.v.f	v3,fs0; 19ed2:	5e0030d7          	vmv.v.i	v1,0` |
| `eigen_or_stdlib` | `void Eigen::internal::generic_product_impl<Eigen::Matrix<float, -1, -1, 0, -1, -1>, Eigen::Transpose<Eigen::Matrix<float, -1, -1, 0, -1, -1> const>, Eigen::DenseShape, Eigen::DenseShape, 8>::scaleAndAddTo<Eigen::Matrix<float, -1, -1, 0, -1, -1> >(Eigen::Matrix<float, -1, -1, 0, -1, -1>&, Eigen::Matrix<float, -1, -1, 0, -1, -1> const&, Eigen::Transpose<Eigen::Matrix<float, -1, -1, 0, -1, -1> const> const&, float const&)` | 56 | `1b976:	5e0030d7          	vmv.v.i	v1,0; 1b998:	020ef0a7          	vse64.v	v1,(t4)` |
| `eigen_or_stdlib` | `(anonymous namespace)::caseEnabled(int, char**, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&)` | 15 | `1524c:	0c007957          	vsetvli	s2,zero,e8,m1,ta,ma; 1525e:	4218a6d7          	vfirst.m	a3,v1` |
| `eigen_or_stdlib` | `Eigen::internal::gemm_pack_lhs<float, long, Eigen::internal::const_blas_data_mapper<float, long, 0>, 24, 8, Eigen::internal::eigen_packet_wrapper<__rvv_float32m1_t, 6>, 0, false, false>::operator()(float*, Eigen::internal::const_blas_data_mapper<float, long, 0> const&, long, long, long, long) const [clone .constprop.0]` | 15 | `17802:	0208e187          	vle32.v	v3,(a7); 17814:	0208e107          	vle32.v	v2,(a7)` |
| `bench_entry` | `main` | 157 | `1300c:	02078087          	vle8.v	v1,(a5); 1302e:	020500a7          	vse8.v	v1,(a0)` |
| `test_support_fixture` | `Eigen::Matrix<float, 4, 4, 0, 4, 4> pcl::registration::rvv_te2d_support::estimateFused2DCandidate<pcl::PointXYZ, pcl::PointXYZ>(pcl::PointCloud<pcl::PointXYZ> const&, pcl::PointCloud<pcl::PointXYZ> const&, pcl::registration::rvv_te2d_support::CandidateStats*)` | 121 | `1c39a:	5e0030d7          	vmv.v.i	v1,0; 1c3a4:	020680a7          	vse8.v	v1,(a3)` |
| `test_support_fixture` | `pcl::PointCloud<pcl::PointXYZ> pcl::registration::rvv_te2d_support::transformCloud2D<pcl::PointXYZ>(pcl::PointCloud<pcl::PointXYZ> const&, Eigen::Matrix<float, 4, 4, 0, 4, 4> const&)` | 7 | `1aca6:	0c07f757          	vsetvli	a4,a5,e8,m1,ta,ma; 1acaa:	02068087          	vle8.v	v1,(a3)` |
| `production_scalar_boundary` | `pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>::getTransformationFromCorrelation(Eigen::Matrix<float, -1, -1, 0, -1, -1> const&, Eigen::Matrix<float, 4, 1, 0, 4, 1> const&, Eigen::Matrix<float, -1, -1, 0, -1, -1> const&, Eigen::Matrix<float, 4, 1, 0, 4, 1> const&, Eigen::Matrix<float, 4, 4, 0, 4, 4>&) const [clone .isra.0]` | 67 | `15650:	5e0030d7          	vmv.v.i	v1,0; 1565a:	0207f0a7          	vse64.v	v1,(a5)` |
| `production_scalar_boundary` | `pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>::estimateRigidTransformation(pcl::ConstCloudIterator<pcl::PointXYZ>&, pcl::ConstCloudIterator<pcl::PointXYZ>&, Eigen::Matrix<float, 4, 4, 0, 4, 4>&) const [clone .isra.0]` | 3 | `161f0:	5e0030d7          	vmv.v.i	v1,0; 161f8:	020400a7          	vse8.v	v1,(s0)` |
| `production_scalar_boundary` | `pcl::ConstCloudIterator<pcl::PointXYZ>::ConstCloudIterator(pcl::PointCloud<pcl::PointXYZ> const&, std::vector<int, std::allocator<int> > const&)` | 3 | `1cb92:	0c07f6d7          	vsetvli	a3,a5,e8,m1,ta,ma; 1cb96:	02070087          	vle8.v	v1,(a4)` |
| `row_source_lambda_boundary` | `std::_Function_handler<unsigned long (), (anonymous namespace)::runSourceIndexedCase<pcl::PointCloud<pcl::PointXYZ> >(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, pcl::PointCloud<pcl::PointXYZ> const&, std::vector<int, std::allocator<int> > const&, pcl::PointCloud<pcl::PointXYZ> const&, int, int)::{lambda()#1}>::_M_invoke(std::_Any_data const&)` | 18 | `16430:	5e0031d7          	vmv.v.i	v3,0; 16440:	5e0030d7          	vmv.v.i	v1,0` |
| `row_source_lambda_boundary` | `std::_Function_handler<unsigned long (), (anonymous namespace)::runDualIndexedCase<pcl::PointCloud<pcl::PointXYZ> >(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, pcl::PointCloud<pcl::PointXYZ> const&, std::vector<int, std::allocator<int> > const&, pcl::PointCloud<pcl::PointXYZ> const&, std::vector<int, std::allocator<int> > const&, int, int)::{lambda()#1}>::_M_invoke(std::_Any_data const&)` | 17 | `169a4:	5e0030d7          	vmv.v.i	v1,0; 169be:	02076187          	vle32.v	v3,(a4)` |
| `row_source_lambda_boundary` | `std::_Function_handler<unsigned long (), (anonymous namespace)::runCorrespondenceCase<pcl::PointCloud<pcl::PointXYZ> >(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, pcl::PointCloud<pcl::PointXYZ> const&, pcl::PointCloud<pcl::PointXYZ> const&, std::vector<pcl::Correspondence, Eigen::aligned_allocator<pcl::Correspondence> > const&, int, int)::{lambda()#1}>::_M_invoke(std::_Any_data const&)` | 16 | `16e2e:	5e0030d7          	vmv.v.i	v1,0; 16e4c:	020680a7          	vse8.v	v1,(a3)` |
| `production_public_boundary` | `pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>::estimateRigidTransformation(pcl::PointCloud<pcl::PointXYZ> const&, pcl::PointCloud<pcl::PointXYZ> const&, Eigen::Matrix<float, 4, 4, 0, 4, 4>&) const` | 42 | `1c020:	0c07f6d7          	vsetvli	a3,a5,e8,m1,ta,ma; 1c024:	02070087          	vle8.v	v1,(a4)` |
| `candidate_lambda_boundary` | `std::_Function_handler<unsigned long (), (anonymous namespace)::runFusedCase<pcl::PointCloud<pcl::PointXYZ> >(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, pcl::PointCloud<pcl::PointXYZ> const&, pcl::PointCloud<pcl::PointXYZ> const&, int, int)::{lambda()#1}>::_M_invoke(std::_Any_data const&)` | 4 | `17302:	5e003157          	vmv.v.i	v2,0; 17306:	0d807057          	vsetvli	zero,zero,e64,m1,ta,ma` |

## 边界说明

- `runPublicCase` lambda 或 exact `PointXYZ` ordered-cloud-pair 公开 overload 边界用于 production public smoke / board bench，PI2 后可作为 production helper 命中证据的一部分。
- `runFusedCase` lambda 内联边界对应 test-only fused candidate，不能替代 production public evidence。
- `runRowSourceCase` 或 source-indexed、dual-indexed、correspondence wrapper lambda 边界对应 materialize-to-ordered row-source candidate，不能替代 production public evidence。
- production fallback、Eigen / iterator 路径也可能含 RVV 指令；这些指令不能归入当前 production helper 收益。
- 本摘要只支持 asm attribution（反汇编归属），不支持目标硬件性能结论。
