# io 模块 RVV 第一轮文件级筛选报告

本文档记录 `io` 模块的第一轮 RVV 文件级粗筛结果。第一轮只回答“文件中是否存在值得第二轮下钻复核的可 SIMD/RVV 片段”，不直接决定最终 RVV 实施队列。

## 1. 输入依据与范围

- 覆盖范围：`io/**` 下源码后缀文件（`.h/.hpp/.c/.cc/.cpp/.cu`）。
- 覆盖结果：总文件 `151`，已判定 `151`（`151/151` 全覆盖）。
- 目录拆分：`include` `95`，`src` `56`。
- 第三方口径：`3rdparty/**` 文件 `0`，候选 `0`（仅登记，不纳入优化队列）。
- 路径显示：候选表和覆盖表中的 `include` 文件省略公共前缀 `io/include/pcl/io/`，`src` 文件以 `src/` 开头显示。

## 2. 第一轮筛选口径

- 第一轮是文件级粗筛，判断标准是文件内是否存在可向量化循环、数学密集片段、批量字段访问、规约、mask/压缩、图像式 organized 遍历或可诊断的局部 SIMD/RVV 点。
- `high/mid` 是第二轮必须复核并交代去向的初始候选基线，不是最终实施全集。
- `low` 表示本轮未发现足以进入二轮基线的证据，不是永久排除；如果第二轮源码下钻发现明显漏判，可以补入并说明证据。
- 第一轮不承诺 RVV 覆盖公开入口主成本；主成本覆盖、测试可行性、fallback 条件和维护风险由第二轮筛选继续判断。

优先级含义：

| 优先级 | 含义 |
| --- | --- |
| `high` | 文件内存在明显批量循环或数学密集片段，且粗看具备较强 SIMD/RVV 评估价值 |
| `mid` | 文件内存在可向量化片段，但主成本、数据布局、语义风险或测试入口需要第二轮继续确认 |
| `low` | 以声明、薄 wrapper、调度、类型、构建胶水、小规模固定计算或明显不规则状态路径为主 |

## 3. 第一轮筛选统计

| 项目 | 数量 | 说明 |
| --- | ---: | --- |
| 源码文件总数 | 151 | `io/**` 源码文件，第三方实现仅登记覆盖，不纳入候选主线。 |
| 已判定文件数 | 151 | `151/151` |
| high | 11 | 二轮必查 |
| mid | 48 | 二轮必查 |
| low | 92 | 已覆盖但不进入二轮初始基线 |
| high + mid | 59 | 第一轮候选基线 |
| 候选占比 | 59/151 = 39.1% | high + mid / 源码文件总数 |

## 4. high 候选（11）

| 文件 | 第一轮证据 |
| --- | --- |
| `src/obj_io.cpp` | 循环49处，编解码/IO转换计算密集，建议优先优化 |
| `include/pcl/compression/impl/entropy_range_coder.hpp` | 循环38处，编解码/IO转换计算密集，建议优先优化 |
| `src/ply_io.cpp` | 循环36处，编解码/IO转换计算密集，建议优先优化 |
| `src/pcd_io.cpp` | 循环31处，编解码/IO转换计算密集，建议优先优化 |
| `impl/vtk_lib_io.hpp` | 循环29处，编解码/IO转换计算密集，建议优先优化 |
| `src/debayer.cpp` | 循环12处，编解码/IO转换计算密集，建议优先优化 |
| `src/vtk_io.cpp` | 循环11处，编解码/IO转换计算密集，建议优先优化 |
| `src/openni_camera/openni_device.cpp` | 循环11处，编解码/IO转换计算密集，建议优先优化 |
| `src/openni2_grabber.cpp` | 循环10处，编解码/IO转换计算密集，建议优先优化 |
| `src/openni_grabber.cpp` | 循环10处，编解码/IO转换计算密集，建议优先优化 |
| `impl/point_cloud_image_extractors.hpp` | 循环10处，编解码/IO转换计算密集，建议优先优化 |

## 5. mid 候选（48）

| 文件 | 第一轮证据 |
| --- | --- |
| `src/openni_camera/openni_image_bayer_grbg.cpp` | 存在可向量化路径（循环19，数学项3），建议次优先 |
| `src/vtk_lib_io.cpp` | 存在可向量化路径（循环18，数学项108），建议次优先 |
| `impl/pcd_io.hpp` | 存在可向量化路径（循环17，数学项91），建议次优先 |
| `src/hdl_grabber.cpp` | 存在可向量化路径（循环16，数学项35），建议次优先 |
| `src/dinast_grabber.cpp` | 存在可向量化路径（循环12，数学项30），建议次优先 |
| `src/openni_camera/openni_driver.cpp` | 存在可向量化路径（循环12，数学项7），建议次优先 |
| `include/pcl/compression/organized_pointcloud_conversion.h` | 存在可向量化路径（循环10，数学项106），建议次优先 |
| `src/real_sense_grabber.cpp` | 存在可向量化路径（循环10，数学项36），建议次优先 |
| `src/openni2/openni2_device.cpp` | 存在可向量化路径（循环10，数学项8），建议次优先 |
| `impl/buffers.hpp` | 存在可向量化路径（循环10，数学项2），建议次优先 |
| `impl/lzf_image_io.hpp` | 存在可向量化路径（循环9，数学项78），建议次优先 |
| `src/image_grabber.cpp` | 存在可向量化路径（循环8，数学项107），建议次优先 |
| `src/oni_grabber.cpp` | 存在可向量化路径（循环8，数学项64），建议次优先 |
| `src/lzf.cpp` | 存在可向量化路径（循环8，数学项3），建议次优先 |
| `src/tim_grabber.cpp` | 存在可向量化路径（循环7，数学项8），建议次优先 |
| `src/ply/ply_parser.cpp` | 存在可向量化路径（循环7，数学项2），建议次优先 |
| `include/pcl/compression/impl/organized_pointcloud_compression.hpp` | 存在可向量化路径（循环6，数学项82），建议次优先 |
| `src/ensenso_grabber.cpp` | 存在可向量化路径（循环6，数学项65），建议次优先 |
| `src/image_depth.cpp` | 存在可向量化路径（循环6，数学项3），建议次优先 |
| `src/openni_camera/openni_depth_image.cpp` | 存在可向量化路径（循环6，数学项3），建议次优先 |
| `src/real_sense/real_sense_device_manager.cpp` | 存在可向量化路径（循环6，数学项2），建议次优先 |
| `src/image_yuv422.cpp` | 存在可向量化路径（循环6，数学项0），建议次优先 |
| `src/openni_camera/openni_image_yuv_422.cpp` | 存在可向量化路径（循环6，数学项0），建议次优先 |
| `include/pcl/compression/color_coding.h` | 存在可向量化路径（循环5，数学项46），建议次优先 |
| `src/image_rgb24.cpp` | 存在可向量化路径（循环5，数学项2），建议次优先 |
| `src/openni_camera/openni_image_rgb24.cpp` | 存在可向量化路径（循环5，数学项0），建议次优先 |
| `pcd_grabber.h` | 存在可向量化路径（循环4，数学项44），建议次优先 |
| `src/depth_sense/depth_sense_grabber_impl.cpp` | 存在可向量化路径（循环4，数学项28），建议次优先 |
| `src/ascii_io.cpp` | 存在可向量化路径（循环4，数学项27），建议次优先 |
| `src/vlp_grabber.cpp` | 存在可向量化路径（循环4，数学项5），建议次优先 |
| `src/openni2/openni2_device_manager.cpp` | 存在可向量化路径（循环4，数学项0），建议次优先 |
| `src/ifs_io.cpp` | 存在可向量化路径（循环3，数学项55），建议次优先 |
| `include/pcl/compression/impl/octree_pointcloud_compression.hpp` | 存在可向量化路径（循环2，数学项88），建议次优先 |
| `include/pcl/compression/point_coding.h` | 存在可向量化路径（循环2，数学项52），建议次优先 |
| `src/real_sense_2_grabber.cpp` | 存在可向量化路径（循环2，数学项25），建议次优先 |
| `src/robot_eye_grabber.cpp` | 存在可向量化路径（循环2，数学项18），建议次优先 |
| `src/lzf_image_io.cpp` | 存在可向量化路径（循环2，数学项17），建议次优先 |
| `src/pcd_grabber.cpp` | 存在可向量化路径（循环2，数学项11），建议次优先 |
| `pcd_io.h` | 存在可向量化路径（循环0，数学项307），建议次优先 |
| `ply_io.h` | 存在可向量化路径（循环0，数学项220），建议次优先 |
| `lzf_image_io.h` | 存在可向量化路径（循环0，数学项86），建议次优先 |
| `include/pcl/compression/octree_pointcloud_compression.h` | 存在可向量化路径（循环0，数学项42），建议次优先 |
| `vtk_lib_io.h` | 存在可向量化路径（循环0，数学项42），建议次优先 |
| `ascii_io.h` | 存在可向量化路径（循环0，数学项30），建议次优先 |
| `include/pcl/compression/organized_pointcloud_compression.h` | 存在可向量化路径（循环0，数学项29），建议次优先 |
| `point_cloud_image_extractors.h` | 存在可向量化路径（循环0，数学项25），建议次优先 |
| `auto_io.h` | 存在可向量化路径（循环0，数学项14），建议次优先 |
| `include/pcl/compression/entropy_range_coder.h` | 存在可向量化路径（循环0，数学项13），建议次优先 |

## 6. 全量文件覆盖表（151/151）

| 文件 | 优先级 | 是否候选 | 判断依据 | 关联实现 |
| --- | --- | --- | --- | --- |
| `include/pcl/compression/color_coding.h` | `mid` | 是 | 存在可向量化路径（循环5，数学项46），建议次优先 | `-` |
| `include/pcl/compression/compression_profiles.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `include/pcl/compression/entropy_range_coder.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项13），建议次优先 | `include/pcl/compression/impl/entropy_range_coder.hpp` |
| `include/pcl/compression/impl/entropy_range_coder.hpp` | `high` | 是 | 循环38处，编解码/IO转换计算密集，建议优先优化 | `-` |
| `include/pcl/compression/impl/octree_pointcloud_compression.hpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项88），建议次优先 | `-` |
| `include/pcl/compression/impl/organized_pointcloud_compression.hpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项82），建议次优先 | `-` |
| `include/pcl/compression/libpng_wrapper.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `include/pcl/compression/octree_pointcloud_compression.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项42），建议次优先 | `include/pcl/compression/impl/octree_pointcloud_compression.hpp` |
| `include/pcl/compression/organized_pointcloud_compression.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项29），建议次优先 | `include/pcl/compression/impl/organized_pointcloud_compression.hpp` |
| `include/pcl/compression/organized_pointcloud_conversion.h` | `mid` | 是 | 存在可向量化路径（循环10，数学项106），建议次优先 | `-` |
| `include/pcl/compression/point_coding.h` | `mid` | 是 | 存在可向量化路径（循环2，数学项52），建议次优先 | `-` |
| `ascii_io.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项30），建议次优先 | `impl/ascii_io.hpp` |
| `auto_io.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项14），建议次优先 | `impl/auto_io.hpp` |
| `buffers.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `impl/buffers.hpp` |
| `davidsdk_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `debayer.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `depth_sense/depth_sense_device_manager.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `depth_sense/depth_sense_grabber_impl.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `depth_sense_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `dinast_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `ensenso_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `file_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `file_io.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `fotonic_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `hdl_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `ifs_io.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `image.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `image_depth.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `image_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `image_ir.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `image_metadata_wrapper.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `image_rgb24.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `image_yuv422.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `impl/ascii_io.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `impl/auto_io.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `impl/buffers.hpp` | `mid` | 是 | 存在可向量化路径（循环10，数学项2），建议次优先 | `-` |
| `impl/lzf_image_io.hpp` | `mid` | 是 | 存在可向量化路径（循环9，数学项78），建议次优先 | `-` |
| `impl/pcd_io.hpp` | `mid` | 是 | 存在可向量化路径（循环17，数学项91），建议次优先 | `-` |
| `impl/point_cloud_image_extractors.hpp` | `high` | 是 | 循环10处，编解码/IO转换计算密集，建议优先优化 | `-` |
| `impl/synchronized_queue.hpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `impl/vtk_lib_io.hpp` | `high` | 是 | 循环29处，编解码/IO转换计算密集，建议优先优化 | `-` |
| `io_exception.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `low_level_io.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `lzf.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `lzf_image_io.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项86），建议次优先 | `impl/lzf_image_io.hpp` |
| `obj_io.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `oni_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni2/openni.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni2/openni2_convert.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni2/openni2_device.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni2/openni2_device_info.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni2/openni2_device_manager.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni2/openni2_frame_listener.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni2/openni2_metadata_wrapper.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni2/openni2_timer_filter.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni2/openni2_video_mode.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni2/openni_shift_to_depth_conversion.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni2_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni_camera/openni.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni_camera/openni_depth_image.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni_camera/openni_device.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni_camera/openni_device_kinect.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni_camera/openni_device_oni.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni_camera/openni_device_primesense.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni_camera/openni_device_xtion.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni_camera/openni_driver.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni_camera/openni_exception.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni_camera/openni_image.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni_camera/openni_image_bayer_grbg.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni_camera/openni_image_rgb24.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni_camera/openni_image_yuv_422.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni_camera/openni_ir_image.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni_camera/openni_shift_to_depth_conversion.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `openni_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `pcd_grabber.h` | `mid` | 是 | 存在可向量化路径（循环4，数学项44），建议次优先 | `-` |
| `pcd_io.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项307），建议次优先 | `impl/pcd_io.hpp` |
| `ply/byte_order.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `ply/io_operators.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `ply/ply.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `ply/ply_parser.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `ply_io.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项220），建议次优先 | `-` |
| `png_io.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `point_cloud_image_extractors.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项25），建议次优先 | `impl/point_cloud_image_extractors.hpp` |
| `real_sense/real_sense_device_manager.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `real_sense_2_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `real_sense_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `robot_eye_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `split.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `tar.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `tim_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `timestamp.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `vlp_grabber.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `vtk_io.h` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `vtk_lib_io.h` | `mid` | 是 | 存在可向量化路径（循环0，数学项42），建议次优先 | `impl/vtk_lib_io.hpp` |
| `src/ascii_io.cpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项27），建议次优先 | `-` |
| `src/auto_io.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/compression.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/davidsdk_grabber.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/debayer.cpp` | `high` | 是 | 循环12处，编解码/IO转换计算密集，建议优先优化 | `-` |
| `src/depth_sense/depth_sense_device_manager.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/depth_sense/depth_sense_grabber_impl.cpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项28），建议次优先 | `-` |
| `src/depth_sense_grabber.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/dinast_grabber.cpp` | `mid` | 是 | 存在可向量化路径（循环12，数学项30），建议次优先 | `-` |
| `src/ensenso_grabber.cpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项65），建议次优先 | `-` |
| `src/hdl_grabber.cpp` | `mid` | 是 | 存在可向量化路径（循环16，数学项35），建议次优先 | `-` |
| `src/ifs_io.cpp` | `mid` | 是 | 存在可向量化路径（循环3，数学项55），建议次优先 | `-` |
| `src/image_depth.cpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项3），建议次优先 | `-` |
| `src/image_grabber.cpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项107），建议次优先 | `-` |
| `src/image_ir.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/image_rgb24.cpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项2），建议次优先 | `-` |
| `src/image_yuv422.cpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项0），建议次优先 | `-` |
| `src/io_exception.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/libpng_wrapper.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/lzf.cpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项3），建议次优先 | `-` |
| `src/lzf_image_io.cpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项17），建议次优先 | `-` |
| `src/obj_io.cpp` | `high` | 是 | 循环49处，编解码/IO转换计算密集，建议优先优化 | `-` |
| `src/oni_grabber.cpp` | `mid` | 是 | 存在可向量化路径（循环8，数学项64），建议次优先 | `-` |
| `src/openni2/openni2_convert.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/openni2/openni2_device.cpp` | `mid` | 是 | 存在可向量化路径（循环10，数学项8），建议次优先 | `-` |
| `src/openni2/openni2_device_info.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/openni2/openni2_device_manager.cpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项0），建议次优先 | `-` |
| `src/openni2/openni2_timer_filter.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/openni2/openni2_video_mode.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/openni2_grabber.cpp` | `high` | 是 | 循环10处，编解码/IO转换计算密集，建议优先优化 | `-` |
| `src/openni_camera/openni_depth_image.cpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项3），建议次优先 | `-` |
| `src/openni_camera/openni_device.cpp` | `high` | 是 | 循环11处，编解码/IO转换计算密集，建议优先优化 | `-` |
| `src/openni_camera/openni_device_kinect.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/openni_camera/openni_device_oni.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/openni_camera/openni_device_primesense.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/openni_camera/openni_device_xtion.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/openni_camera/openni_driver.cpp` | `mid` | 是 | 存在可向量化路径（循环12，数学项7），建议次优先 | `-` |
| `src/openni_camera/openni_exception.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/openni_camera/openni_image_bayer_grbg.cpp` | `mid` | 是 | 存在可向量化路径（循环19，数学项3），建议次优先 | `-` |
| `src/openni_camera/openni_image_rgb24.cpp` | `mid` | 是 | 存在可向量化路径（循环5，数学项0），建议次优先 | `-` |
| `src/openni_camera/openni_image_yuv_422.cpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项0），建议次优先 | `-` |
| `src/openni_camera/openni_ir_image.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/openni_grabber.cpp` | `high` | 是 | 循环10处，编解码/IO转换计算密集，建议优先优化 | `-` |
| `src/pcd_grabber.cpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项11），建议次优先 | `-` |
| `src/pcd_io.cpp` | `high` | 是 | 循环31处，编解码/IO转换计算密集，建议优先优化 | `-` |
| `src/ply/ply_parser.cpp` | `mid` | 是 | 存在可向量化路径（循环7，数学项2），建议次优先 | `-` |
| `src/ply_io.cpp` | `high` | 是 | 循环36处，编解码/IO转换计算密集，建议优先优化 | `-` |
| `src/png_io.cpp` | `low` | 否 | 以声明/接口封装/构建胶水为主，暂不纳入本轮候选 | `-` |
| `src/real_sense/real_sense_device_manager.cpp` | `mid` | 是 | 存在可向量化路径（循环6，数学项2），建议次优先 | `-` |
| `src/real_sense_2_grabber.cpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项25），建议次优先 | `-` |
| `src/real_sense_grabber.cpp` | `mid` | 是 | 存在可向量化路径（循环10，数学项36），建议次优先 | `-` |
| `src/robot_eye_grabber.cpp` | `mid` | 是 | 存在可向量化路径（循环2，数学项18），建议次优先 | `-` |
| `src/tim_grabber.cpp` | `mid` | 是 | 存在可向量化路径（循环7，数学项8），建议次优先 | `-` |
| `src/vlp_grabber.cpp` | `mid` | 是 | 存在可向量化路径（循环4，数学项5），建议次优先 | `-` |
| `src/vtk_io.cpp` | `high` | 是 | 循环11处，编解码/IO转换计算密集，建议优先优化 | `-` |
| `src/vtk_lib_io.cpp` | `mid` | 是 | 存在可向量化路径（循环18，数学项108），建议次优先 | `-` |

## 7. 二轮交接说明

- 第二轮筛选应读取本文件，并把所有 `high/mid` 文件作为初始候选基线逐项交代去向。
- 第二轮可以将 `high/mid` 降级为暂缓、不推荐或不单独实施，但必须说明源码证据和主成本覆盖原因。
- 第二轮可以补入 `low` 或初筛遗漏文件，但必须说明补入来源、证据和为什么没有扩展成重新全模块/全库筛选。
- 第二轮输出应使用 `doc-rvv/library-screening/io/io-function-evaluation-queue.zh.md`，并按“建议进行 RVV 优化的文件 / 保留实施的候选文件 / 暂缓或不推荐考虑 RVV 优化的文件”三类组织。
