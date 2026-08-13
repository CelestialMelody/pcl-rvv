# TEPTPLW RVV-vs-RVV B/A Summary

Input logs:
- /home/zoomin/codes/pcl/test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/production_source_indices_detail_ba/run01_rvv.log (iterations=20, warmup=5)
- /home/zoomin/codes/pcl/test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/production_source_indices_detail_ba/run02_rvv.log (iterations=20, warmup=5)
- /home/zoomin/codes/pcl/test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/production_source_indices_detail_ba/run03_rvv.log (iterations=20, warmup=5)
- /home/zoomin/codes/pcl/test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/production_source_indices_detail_ba/run04_rvv.log (iterations=20, warmup=5)
- /home/zoomin/codes/pcl/test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/production_source_indices_detail_ba/run05_rvv.log (iterations=20, warmup=5)

| layer | point type | size | candidate | runs | avg B/A median | avg B/A min | avg B/A max | avg B/A p10 | avg B/A p90 | values | iter-median B/A median |
| --- | --- | ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | ---: |
| production-source-indices-detail component | `pointnormal` | 65536 | `abcd-ilp` | 5 | 0.957x | 0.952x | 0.960x | 0.954x | 0.960x | 0.959x, 0.957x, 0.960x, 0.952x, 0.956x | 0.950x |
| production-source-indices-detail component | `pointnormal` | 262144 | `abcd-ilp` | 5 | 0.881x | 0.620x | 0.996x | 0.711x | 0.994x | 0.996x, 0.991x, 0.848x, 0.881x, 0.620x | 1.005x |
| production-source-indices-detail component | `pointxyz-to-pointnormal` | 65536 | `abcd-ilp` | 5 | 1.003x | 0.995x | 1.037x | 0.995x | 1.034x | 0.995x, 0.995x, 1.037x, 1.029x, 1.003x | 1.027x |
| production-source-indices-detail component | `pointxyz-to-pointnormal` | 262144 | `abcd-ilp` | 5 | 1.115x | 0.817x | 1.593x | 0.874x | 1.413x | 1.115x, 1.593x, 1.141x, 0.960x, 0.817x | 1.116x |
| production-source-indices-detail component | `pointxyz-to-pointxyzinormal` | 65536 | `abcd-ilp` | 5 | 0.989x | 0.960x | 0.997x | 0.961x | 0.995x | 0.963x, 0.989x, 0.960x, 0.992x, 0.997x | 0.978x |
| production-source-indices-detail component | `pointxyz-to-pointxyzinormal` | 262144 | `abcd-ilp` | 5 | 1.009x | 0.782x | 1.058x | 0.802x | 1.049x | 1.009x, 0.782x, 1.035x, 1.058x, 0.831x | 1.031x |
| production-source-indices-detail full | `pointnormal` | 65536 | `abcd-ilp` | 5 | 0.968x | 0.957x | 0.996x | 0.957x | 0.991x | 0.957x, 0.984x, 0.957x, 0.996x, 0.968x | 0.968x |
| production-source-indices-detail full | `pointnormal` | 262144 | `abcd-ilp` | 5 | 1.382x | 1.008x | 1.677x | 1.032x | 1.671x | 1.008x, 1.663x, 1.382x, 1.677x, 1.069x | 1.037x |
| production-source-indices-detail full | `pointxyz-to-pointnormal` | 65536 | `abcd-ilp` | 5 | 1.031x | 1.013x | 1.040x | 1.020x | 1.039x | 1.036x, 1.040x, 1.013x, 1.029x, 1.031x | 1.030x |
| production-source-indices-detail full | `pointxyz-to-pointnormal` | 262144 | `abcd-ilp` | 5 | 0.851x | 0.617x | 1.192x | 0.700x | 1.145x | 1.075x, 0.826x, 1.192x, 0.851x, 0.617x | 1.008x |
| production-source-indices-detail full | `pointxyz-to-pointxyzinormal` | 65536 | `abcd-ilp` | 5 | 1.018x | 1.006x | 1.188x | 1.009x | 1.126x | 1.006x, 1.018x, 1.188x, 1.033x, 1.013x | 1.019x |
| production-source-indices-detail full | `pointxyz-to-pointxyzinormal` | 262144 | `abcd-ilp` | 5 | 0.721x | 0.694x | 0.949x | 0.698x | 0.923x | 0.883x, 0.721x, 0.705x, 0.694x, 0.949x | 0.905x |
