# transformation_estimation_2D ASM Attribution Summary

## 结论

当前 RVV bench binary 中，fused candidate 的关键指令归属到 `runFusedCase` lambda 内联边界。该边界属于 test-support / bench binary，不代表 production dispatch 已经接入。

- attribution_decision：`candidate_inline_boundary_present_with_other_rvv_boundaries`
- std RVV lines：837
- rvv RVV lines：876
- delta：+39

## 关键指令归属

| category | RVV lines | key mnemonics | boundary |
| --- | ---: | --- | --- |
| `eigen_or_stdlib` | 651 | vfredosum=9, vfadd=8, vsetvli=35 | `Eigen::internal::gebp_kernel<float, float, long, Eigen::internal::blas_data_mapper<float, long, 0, 0, 1>, 24, 4, false, false>::operator()(Eigen::internal::blas_data_mapper<float, long, 0, 0, 1> const&, float const*, float const*, long, long, long, float, long, long, long, long) const [clone .constprop.0]` |
| `candidate_lambda_boundary` | 83 | vlsseg3e32.v=4, vfmacc=4, vfredosum=16, vfsub=8, vfadd=4, vsetvli=9 | `std::_Function_handler<unsigned long (), (anonymous namespace)::runFusedCase<pcl::PointCloud<pcl::PointXYZ> >(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, pcl::PointCloud<pcl::PointXYZ> const&, pcl::PointCloud<pcl::PointXYZ> const&, int, int)::{lambda()#1}>::_M_invoke(std::_Any_data const&)` |
| `production_scalar_boundary` | 76 | vfredosum=4, vsetvli=10 | `pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>::getTransformationFromCorrelation(Eigen::Matrix<float, -1, -1, 0, -1, -1> const&, Eigen::Matrix<float, 4, 1, 0, 4, 1> const&, Eigen::Matrix<float, -1, -1, 0, -1, -1> const&, Eigen::Matrix<float, 4, 1, 0, 4, 1> const&, Eigen::Matrix<float, 4, 4, 0, 4, 4>&) const [clone .isra.0]` |
| `bench_entry` | 59 | vsetvli=7 | `main` |
| `test_support_fixture` | 7 | vsetvli=1 | `pcl::PointCloud<pcl::PointXYZ> pcl::registration::rvv_te2d_support::transformCloud2D<pcl::PointXYZ>(pcl::PointCloud<pcl::PointXYZ> const&, Eigen::Matrix<float, 4, 4, 0, 4, 4> const&)` |

## RVV build top mnemonics

| mnemonic | count |
| --- | ---: |
| `vle32.v` | 237 |
| `vse8.v` | 135 |
| `vle8.v` | 130 |
| `vse32.v` | 74 |
| `vsetvli` | 62 |
| `vmv.v.` | 30 |
| `vfmv.v.f` | 30 |
| `vfmv.f.s` | 30 |
| `vfredosum` | 29 |
| `vfmv.s.f` | 21 |
| `vfmul` | 21 |
| `vse64.v` | 13 |
| `vfadd` | 12 |
| `vlse32.v` | 10 |
| `vfsub` | 8 |
| `vmul.vx` | 6 |
| `vadd.vx` | 6 |
| `vfirst` | 6 |
| `vlseg4e32.v` | 5 |
| `vlsseg3e32.v` | 4 |

## Top symbols

| category | symbol | RVV lines | sample |
| --- | --- | ---: | --- |
| `eigen_or_stdlib` | `Eigen::internal::gebp_kernel<float, float, long, Eigen::internal::blas_data_mapper<float, long, 0, 0, 1>, 24, 4, false, false>::operator()(Eigen::internal::blas_data_mapper<float, long, 0, 0, 1> const&, float const*, float const*, long, long, long, float, long, long, long, long) const [clone .constprop.0]` | 430 | `15e36:	5e003557          	vmv.v.i	v10,0; 15e3a:	5e0455d7          	vfmv.v.f	v11,fs0` |
| `eigen_or_stdlib` | `Eigen::internal::general_matrix_vector_product<long, float, Eigen::internal::const_blas_data_mapper<float, long, 0>, 0, false, float, Eigen::internal::const_blas_data_mapper<float, long, 1>, false, 0>::run(long, long, Eigen::internal::const_blas_data_mapper<float, long, 0> const&, Eigen::internal::const_blas_data_mapper<float, long, 1> const&, float*, long, float) [clone .isra.0]` | 84 | `17eae:	5e0451d7          	vfmv.v.f	v3,fs0; 17f9c:	5e0030d7          	vmv.v.i	v1,0` |
| `eigen_or_stdlib` | `void Eigen::internal::generic_product_impl<Eigen::Matrix<float, -1, -1, 0, -1, -1>, Eigen::Transpose<Eigen::Matrix<float, -1, -1, 0, -1, -1> const>, Eigen::DenseShape, Eigen::DenseShape, 8>::scaleAndAddTo<Eigen::Matrix<float, -1, -1, 0, -1, -1> >(Eigen::Matrix<float, -1, -1, 0, -1, -1>&, Eigen::Matrix<float, -1, -1, 0, -1, -1> const&, Eigen::Transpose<Eigen::Matrix<float, -1, -1, 0, -1, -1> const> const&, float const&)` | 56 | `197da:	5e0030d7          	vmv.v.i	v1,0; 197fc:	020ef0a7          	vse64.v	v1,(t4)` |
| `eigen_or_stdlib` | `(anonymous namespace)::caseEnabled(int, char**, std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&)` | 15 | `139f2:	0c007957          	vsetvli	s2,zero,e8,m1,ta,ma; 13a04:	4218a6d7          	vfirst.m	a3,v1` |
| `eigen_or_stdlib` | `Eigen::internal::gemm_pack_lhs<float, long, Eigen::internal::const_blas_data_mapper<float, long, 0>, 24, 8, Eigen::internal::eigen_packet_wrapper<__rvv_float32m1_t, 6>, 0, false, false>::operator()(float*, Eigen::internal::const_blas_data_mapper<float, long, 0> const&, long, long, long, long) const [clone .constprop.0]` | 15 | `158cc:	0208e187          	vle32.v	v3,(a7); 158de:	0208e107          	vle32.v	v2,(a7)` |
| `candidate_lambda_boundary` | `std::_Function_handler<unsigned long (), (anonymous namespace)::runFusedCase<pcl::PointCloud<pcl::PointXYZ> >(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, pcl::PointCloud<pcl::PointXYZ> const&, pcl::PointCloud<pcl::PointXYZ> const&, int, int)::{lambda()#1}>::_M_invoke(std::_Any_data const&)` | 83 | `1527a:	0d107357          	vsetvli	t1,zero,e32,m2,ta,ma; 1527e:	5e003157          	vmv.v.i	v2,0` |
| `production_scalar_boundary` | `pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>::getTransformationFromCorrelation(Eigen::Matrix<float, -1, -1, 0, -1, -1> const&, Eigen::Matrix<float, 4, 1, 0, 4, 1> const&, Eigen::Matrix<float, -1, -1, 0, -1, -1> const&, Eigen::Matrix<float, 4, 1, 0, 4, 1> const&, Eigen::Matrix<float, 4, 4, 0, 4, 4>&) const [clone .isra.0]` | 67 | `13ee2:	5e0030d7          	vmv.v.i	v1,0; 13eec:	0207f0a7          	vse64.v	v1,(a5)` |
| `production_scalar_boundary` | `pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>::estimateRigidTransformation(pcl::ConstCloudIterator<pcl::PointXYZ>&, pcl::ConstCloudIterator<pcl::PointXYZ>&, Eigen::Matrix<float, 4, 4, 0, 4, 4>&) const [clone .isra.0]` | 3 | `14a30:	5e0030d7          	vmv.v.i	v1,0; 14a38:	020400a7          	vse8.v	v1,(s0)` |
| `production_scalar_boundary` | `pcl::registration::TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>::estimateRigidTransformation(pcl::PointCloud<pcl::PointXYZ> const&, pcl::PointCloud<pcl::PointXYZ> const&, Eigen::Matrix<float, 4, 4, 0, 4, 4>&) const` | 3 | `19e3a:	0c077657          	vsetvli	a2,a4,e8,m1,ta,ma; 19e3e:	02068087          	vle8.v	v1,(a3)` |
| `production_scalar_boundary` | `pcl::ConstCloudIterator<pcl::PointXYZ>::ConstCloudIterator(pcl::PointCloud<pcl::PointXYZ> const&, std::vector<int, std::allocator<int> > const&)` | 3 | `19f98:	0c07f6d7          	vsetvli	a3,a5,e8,m1,ta,ma; 19f9c:	02070087          	vle8.v	v1,(a4)` |
| `bench_entry` | `main` | 59 | `12d5e:	02078087          	vle8.v	v1,(a5); 12d72:	020500a7          	vse8.v	v1,(a0)` |
| `test_support_fixture` | `pcl::PointCloud<pcl::PointXYZ> pcl::registration::rvv_te2d_support::transformCloud2D<pcl::PointXYZ>(pcl::PointCloud<pcl::PointXYZ> const&, Eigen::Matrix<float, 4, 4, 0, 4, 4> const&)` | 7 | `18c7e:	0c07f757          	vsetvli	a4,a5,e8,m1,ta,ma; 18c82:	02068087          	vle8.v	v1,(a3)` |

## 边界说明

- `runFusedCase` lambda 内联边界包含 `vlsseg3e32.v`、`vfmacc.vv` 和 `vfredosum.vs`，对应 test-only fused candidate。
- production 标量 helper 和 Eigen / iterator 路径也可能含 RVV 指令；这些指令不能归入 fused candidate 收益。
- 本摘要只支持 asm attribution（反汇编归属），不支持目标硬件性能结论。
