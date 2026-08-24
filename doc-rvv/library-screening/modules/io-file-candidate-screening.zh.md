# io 模块 RVV 第一轮文件级筛选报告

本文档记录 `io` 模块按当前 RVV screening（RVV 筛选）规则重做后的第一轮文件级粗筛结果。第一轮只回答“文件中是否存在值得第二轮下钻复核的可 SIMD/RVV（单指令多数据 / RISC-V Vector）片段”，不直接决定最终 RVV 实施队列。

## 1. 输入依据与范围

- 源码范围：`io/**`
- 源码后缀：`.h`、`.hpp`、`.c`、`.cc`、`.cpp`、`.cu`
- 源码文件总数：`151`
- 已判定文件数：`151`
- 排除口径：第三方实现、生成文件、测试、文档和非主库路径只登记或排除，不纳入候选主线；本模块当前未发现 `io/3rdparty/**` 源码文件。
- 重做口径：既有同路径文档只作为 previous baseline（历史基线）和覆盖检查参考，不沿用循环数量或数学项数量作为充分判断依据。
- 路径显示：`io/include/pcl/io/` 下文件省略该公共前缀；`io/include/pcl/compression/` 下文件保留 `include/pcl/compression/` 前缀；`io/src/` 下文件以 `src/` 开头显示。

## 2. 第一轮筛选口径

- 第一轮是文件级粗筛，判断标准是文件内是否存在可向量化循环、数学密集片段、批量字段访问、规约、mask（掩码）/压缩、图像式 organized（有行列组织）遍历或可诊断的局部 SIMD/RVV 点。
- `high/mid` 是第二轮必须复核并交代去向的初始候选基线，不是最终实施全集。
- `low` 表示本轮未发现足以进入二轮基线的证据，不是永久排除；如果第二轮源码下钻发现明显漏判，可以补入并说明证据。
- 第一轮不承诺 RVV 覆盖公开入口主成本；主成本覆盖、测试可行性、fallback（回退路径）条件和维护风险由第二轮筛选继续判断。
- 循环数量、数学项数量或关键词命中只能作为扫描线索，不能单独支撑 `high` / `mid`。候选必须定位到可复核的入口、loop/helper（循环 / 辅助函数）、trip count（循环规模来源）、RVV 适配点和主要风险。

| 优先级 | 含义 |
| --- | --- |
| `high` | 文件内存在明显批量 loop 或函数族，满足生产价值、并行合法性、RVV 访存匹配中的两个以上强信号，且没有未解释的语义或测试硬伤 |
| `mid` | 存在可 SIMD/RVV 片段，但主成本、数据布局、indices/gather 成本、浮点语义、入口覆盖或测试可行性仍需函数评估队列判定 |
| `low` | 以声明、薄 wrapper（薄封装）、调度、类型胶水、小规模固定计算、不规则容器/search/solver/状态机或非热点路径为主 |

## 3. 第一轮筛选统计

| 项目 | 数量 | 说明 |
| --- | ---: | --- |
| 源码文件总数 | 151 | `io/**` 源码后缀文件 |
| 已判定文件数 | 151 | `151/151` 全覆盖 |
| high | 12 | 二轮必查 |
| mid | 26 | 二轮必查 |
| low | 113 | 已覆盖但不进入二轮初始基线 |
| high + mid | 38 | 第一轮候选基线 |
| 候选占比 | 38/151 = 25.2% | high + mid / 源码文件总数 |

相对旧文档的主要口径变化：

- 旧文档把大量“循环多 / 数学项多”的文件列为 high/mid；本次重做要求每个 high/mid 能落到具体入口、trip count、RVV 适配点和风险。
- 文本格式解析、range coder（区间编码器）和 LZF 压缩核心虽然循环较多，但跨迭代状态强、I/O 或熵编码状态主导，本轮降级为 `low` 或只保留局部 `mid`。
- 纯公开头、声明头和伴随实现头不再因为关联实现存在候选而单独进入 high/mid；真实循环在 `impl/*.hpp` 或 `src/*.cpp` 中承载。

## 4. high 候选

| 文件 | 关键入口 / loop | trip count 来源 | RVV 适配点 | 主要风险 | 第一轮判定理由 |
| --- | --- | --- | --- | --- | --- |
| `include/pcl/compression/impl/organized_pointcloud_compression.hpp` | `OrganizedPointCloudCompression::encodePointCloud` 的无效点颜色清零、灰度转换和 `analyzeOrganizedCloud` 深度 / 焦距扫描 | `width * height` / `cloud_arg->size()` | 连续 disparity/color buffer、逐点 finite（有限值）谓词、max depth 规约和 RGB 灰度乘加 | PNG/LZF 后续压缩可能稀释收益；max/focal 更新含条件规约 | 覆盖 organized 点云压缩前后处理的批量路径，和压缩格式入口直接相邻。 |
| `include/pcl/compression/organized_pointcloud_conversion.h` | `OrganizedConversion::convert` 多个 disparity/depth/RGB 与点云互转 loop | `cloud_arg.size()`、`width_arg * height_arg` | 深度反投影、disparity 量化、RGB/mono 转换、finite mask 和连续输出 | `push_back` 保序、点类型字段、NaN 和 0/0x7FF 语义 | 多个公开压缩转换入口都落在逐点同构公式上。 |
| `impl/lzf_image_io.hpp` | `LZFDepth16ImageReader::read/readOMP`、`LZFYUV422ImageReader::read/readOMP`、`LZFBayer8ImageReader::read/readOMP` | `width * height`、`cloud.size()` | 深度图反投影、YUV422->RGB、Bayer->RGB 后的 SoA/连续字段写回 | LZF 解压本身仍是串行状态机；OpenMP 和 RVV 分流边界需分开 | 解压后的像素到点云转换是大规模连续批量算术。 |
| `impl/point_cloud_image_extractors.hpp` | `extractImpl` normal/RGB/label/scaling 分支和 NaN 涂黑 pass | `cloud.size()` | AoS 字段 offset load、RGB 拆包、normal 映射、min/max 规约、缩放写 mono16 | `getFieldValue` 泛型字段访问、label colormap 的 map 路径、NaN 涂黑顺序 | organized 点云转图像是逐点字段转换。 |
| `src/debayer.cpp` | `DeBayer::debayerBilinear`、`debayerEdgeAware`、`debayerEdgeAwareWeighted` 主图像 loop | `width * height`，主体按 2 像素 / 2 行推进 | 固定邻域 byte load、AVG/AVG4/WAVG4、边缘分支 mask、RGB 连续写出 | 边界行列特殊处理、weighted 分支的 `abs` / 除法和饱和语义 | Bayer 解码主体是大图像同构 stencil（邻域模板）计算。 |
| `src/image_depth.cpp` | `DepthImage::fillDepthImageRaw`、`fillDepthImage`、`fillDisparityImage` | 输出 `width * height` | `uint16` 深度读取、无效值 mask、mm->m 乘法、`constant / pixel` 视差换算、padding tail | NaN 写入、downsample stride、line padding 和 exact invalid value 语义 | 深度帧填充是公开图像接口主循环。 |
| `src/image_yuv422.cpp` | `ImageYUV422::fillRGB`、`fillGrayscale` | `width * height`，RGB 路径每次处理 2 像素 | YUV422 固定布局 load、整数乘加、右移、clip 饱和、RGB 连续写出 | `CLIP_CHAR` 饱和、downsample stride、YUV 两像素共享 U/V | YUV422->RGB 是典型逐像素颜色转换。 |
| `src/openni2_grabber.cpp` | `convertToXYZPointCloud`、`convertToXYZRGBPointCloud`、`convertToXYZIPointCloud` | `depth_width_ * depth_height_`、`image_width_ * image_height_` | 深度反投影、invalid depth mask、RGB/IR 字段合并、organized stride 写点云 | depth/RGB 分辨率不一致、设备 buffer resize、点类型模板和 alpha 语义 | OpenNI2 grabber 的帧到点云转换是直接用户可见批量路径。 |
| `src/openni_camera/openni_depth_image.cpp` | `DepthImage::fillDepthImageRaw`、`fillDepthImage`、`fillDisparityImage` | 输出 `width * height` | 深度 mask、mm->m、视差除法和 padding | OpenNI metadata 访问、NaN/0 语义、line padding | OpenNI 旧接口仍有独立深度图转换实现。 |
| `src/openni_camera/openni_image_bayer_grbg.cpp` | `ImageBayerGRBG::fillRGB`、`fillGrayscale` 中 Bayer 行列 loop | `width * height`，按 2 像素块推进 | Bayer byte 邻域、RGB/gray 写出、downsample stride | 边界行列、OpenNI 条件编译和像素布局语义 | 文件承载 OpenNI Bayer 图像转换主循环。 |
| `src/openni_camera/openni_image_yuv_422.cpp` | `ImageYUV422::fillRGB`、`fillGrayscale` | `XRes * YRes`，RGB 路径每次处理 2 像素 | YUV422 固定布局、整数乘加、clip 饱和、连续 RGB/gray 写出 | OpenNI metadata、downsample stride、饱和一致性 | 与通用 YUV422 同构的 OpenNI 入口。 |
| `src/openni_grabber.cpp` | `convertToXYZPointCloud`、`convertToXYZRGBPointCloud`、`convertToXYZIPointCloud` | `depth_width_ * depth_height_`、`image_width_ * image_height_` | 深度反投影、invalid depth mask、RGB/IR 合并、organized stride 写点云 | 旧 OpenNI buffer、depth/RGB 对齐、点类型字段和 alpha 语义 | 旧 OpenNI grabber 的帧转点云主路径和 OpenNI2 同类。 |

## 5. mid 候选

| 文件 | 关键入口 / loop | trip count 来源 | RVV 适配点 | 主要风险 | 第一轮判定理由 |
| --- | --- | --- | --- | --- | --- |
| `include/pcl/compression/color_coding.h` | `ColorCoding::encodeAverageOfPoints`、`decodePoints`、`setDefaultColor` | leaf 内 `indexVector_arg.size()` 或点段长度 | RGB 分量提取、sum/average、xor diff、连续 output cloud 写回 | indices gather、短 leaf 尺寸、压缩器调用上下文 | 有明确逐点颜色编码片段，但是否覆盖压缩主成本需二轮判断。 |
| `include/pcl/compression/impl/octree_pointcloud_compression.hpp` | frame header 扫描和 `i < pointCount` 统计 / 输出段 | `pointCount` 和 frame header 长度 | 可复用 `ColorCoding` / `PointCoding` 的逐点编码结果，局部统计 | 主成本可能在 octree 编码结构和 entropy coder | 作为压缩入口伴随文件保留。 |
| `include/pcl/compression/point_coding.h` | `PointCoding::encodePoints`、`decodePoints` | leaf 内 indices 或输出点段长度 | x/y/z 差分、clamp（截断）、连续 diff vector 写入和点云写回 | indices gather、量化/截断语义、leaf 规模可能小 | 有清晰坐标差分批量公式，但受 octree leaf 分布限制。 |
| `impl/buffers.hpp` | `MedianBuffer::push`、`AverageBuffer::push` | buffer `size_`，每列 window `window_size_` | AverageBuffer 的逐元素 rolling sum；MedianBuffer 的局部排序维护可诊断 | median 路径跨迭代排序状态强、mutex 和 window 小 | Average path 有 SIMD 价值，median path 风险高。 |
| `impl/pcd_io.hpp` | 模板 `PCDWriter::writeBinaryCompressed`、`writeASCII` 字段循环 | `cloud.size()` * `fields.size()` * `count` | AoS->SoA 字段拆分、binary compressed 前置拷贝、finite 检查辅助 | ASCII 写流不可 RVV；字段 offset 和自定义点类型 | 模板写路径含批量字段搬运。 |
| `impl/vtk_lib_io.hpp` | `vtk2PointCloud`、`vtkStructuredGridToPointCloud` 模板字段写回 | `cloud.size()` 或 `cloud.width * cloud.height` | VTK tuple 到 PointT 字段写回、XYZ/normal/RGB 规则字段 | VTK API 调用开销、GetTupleValue 不连续、外部库主导 | 存在逐点字段转换，但可能被 VTK 调用稀释。 |
| `pcd_grabber.h` | `PCDGrabberBase` organized invalid point 处理 | `cloud->height * cloud->width` | organized 点云格点遍历、NaN 点填充 / 检查 | grabber 播放调度主导，循环可能不是主成本 | 头内有真实 organized loop，但更可能作为 PCD 播放上下文候选。 |
| `src/depth_sense/depth_sense_grabber_impl.cpp` | DepthSense frame 到 cloud 的固定 SIZE / WIDTH*HEIGHT loop | 相机固定 `SIZE` 或 `WIDTH * HEIGHT` | depth smoothing、vertices->PointXYZ、RGBA 行拷贝 | 外部 SDK、固定分辨率、设备状态和 copyPointCloud 稀释 | 有逐点帧转换片段，但依赖 DepthSense 入口热度。 |
| `src/dinast_grabber.cpp` | `readImage` buffer copy、`getXYZIPointCloud` 畸变校正 loop | `image_size_` / `cloud.width * cloud.height` | 每像素 sqrt、多项式、深度到 XYZI 转换 | USB 读取和 header 同步主导，双层 x/y 顺序和经验公式 | 数学密度高但设备路径专门。 |
| `src/ensenso_grabber.cpp` | pointMap 三元组到 cloud 的 mm->m 转换 | `pointMap.size() / 3` | 连续 x/y/z 三元组 load、scale 写 PointXYZ | Ensenso SDK 调用和数据获取可能主导 | 有规则逐点字段转换，但主成本需 profile。 |
| `src/hdl_grabber.cpp` | calibration load 和 firing packet 解析循环 | laser 数、packet firing 数 | 三角 LUT、packet 内固定 firing/laser 遍历 | packet 规模固定较小、网络/解析状态主导 | LiDAR packet 转换有重复数学片段。 |
| `src/image_grabber.cpp` | depth image 文件到 cloud / RGBD 合成 loop | 图像 `dims[0] * dims[1]` | depth pixel 到点 / color 填充的 organized loop | 文件枚举和加载主导；VTK/PNG 读入边界 | 有帧到点云转换片段。 |
| `src/image_rgb24.cpp` | `ImageRGB24::fillGrayscale`、downsample `fillRGB` | 输出 `width * height` | RGB->gray 乘加、RGB pixel copy/downsample | 完整 RGB copy 可由 `memcpy/std::copy` 覆盖，算术只在 gray 路径 | 灰度转换有明确批量公式，但主收益可能有限。 |
| `src/lzf_image_io.cpp` | `LZFRGB24ImageWriter::write`、`LZFYUV422ImageWriter::write` planar 重排 | `width * height` | interleaved->planar RGB/YUV 重排，连续写出 | 后续 LZF 压缩主导；重排是 partial-preprocess（局部前处理） | 写路径存在规则重排。 |
| `src/obj_io.cpp` | OBJ vertex/normal/face 解析后的 normal accumulation / normalize 写回 | 顶点数、face 字段数 | normal mapping 的向量累加和 normalize（归一化） | 文本解析、字符串分割和 material 状态主导；face 输出顺序语义 | 有几何局部片段，但 OBJ I/O 主成本不一定在该片段。 |
| `src/oni_grabber.cpp` | ONI reader 的 depth/color/IR organized cloud 构造 | `centerX * centerY` / `width * height` | depth+color 到点云、finite mask、organized stride | ONI 文件读取和 replay 状态主导；和 OpenNI conversion 重复 | 有帧转点云公式，但可能作为 OpenNI 同类入口保留。 |
| `src/openni_camera/openni_image_rgb24.cpp` | OpenNI `ImageRGB24::fillGrayscale`、downsample `fillRGB` | 输出 `width * height` | RGB->gray 乘加、downsample RGB copy | OpenNI 条件编译；full RGB copy 非 RVV 重点 | 与通用 RGB24 同构，保留供二轮统一评估。 |
| `src/pcd_io.cpp` | `readBodyBinary` compressed unpack、dense finite scan、writer ASCII/binary loops | `cloud.width * cloud.height * fields/count` | SoA->AoS unpack、field finite check、binary field copy | ASCII 流和 mmap I/O 主导；字段 datatype switch 复杂 | PCD 是常用入口，存在批量字段处理。 |
| `src/ply_io.cpp` | range_grid 重排、ASCII/binary write、finite rangegrid 生成 | `nr_points * fields/count`、`range_grid_.size()` | finite mask、rangegrid 压缩、binary field write | 文本/二进制 I/O、datatype switch、range_grid 输出顺序 | PLY 文件中有可批量 field 片段。 |
| `src/real_sense_2_grabber.cpp` | `RealSense2Grabber` vertices/texture 到 cloud 的 OpenMP loop | `cloud->size()` | vertices 指针、UV texture map、PointXYZRGBA 写回 | librealsense API、`mapColorFunc` 调用和 texture gather | 有逐点转换片段，但外部 SDK 成本需确认。 |
| `src/real_sense_grabber.cpp` | RealSense depth smoothing、vertices->XYZ、color mapped loop | 固定 `SIZE`、`WIDTH * HEIGHT` | depth buffer rolling copy、XYZ/XYZRGBA 点云填充 | 外部 SDK、固定分辨率、buffer / projection 调用主导 | 有批量帧转换片段。 |
| `src/robot_eye_grabber.cpp` | packet payload 到 `PointXYZI` 的 `computeXYZI` loop | packet `total_points` | 固定 bytes-per-point 解码、finite mask、append | packet 解析和 push_back 输出顺序；命中点数可变 | 有 LiDAR 点解码片段。 |
| `src/tim_grabber.cpp` | `buildLookupTables` sin/cos 表、distance->point loop | `amount_of_data_`、`distances_.size()` | sin/cos LUT 生成、距离批量转换 | LUT 生成一次性；grabber 状态和网络读取主导 | 有数学循环但主成本和热度不确定。 |
| `src/vlp_grabber.cpp` | VLP packet firing/laser loop | `HDL_FIRING_PER_PKT * HDL_LASER_PER_FIRING` | packet 内角度校正、三角 LUT、点构造 | 单 packet 固定小规模，分支和 packet 状态较多 | 与 HDL 同类，保留供二轮判断是否归并。 |
| `src/vtk_io.cpp` | VTK ASCII 写 point / RGB / polygon loop | `nr_points`、`triangles.polygons.size()` | XYZ/RGB 字段读取、颜色归一化、polygon 写出 | ASCII 输出流主导；字段查找和文件写稀释 | 有逐点字段转换，但可能 I/O 主导。 |
| `src/vtk_lib_io.cpp` | `vtk2mesh` / `mesh2vtk` 点、颜色、normal 转换 | `GetNumberOfPoints()`、`nr_points`、`nr_polygons` | VTK/PCL 点字段互转、normal/RGB 连续写入 | VTK API 调用、InsertNextTupleValue、polygon cell 状态 | 转换 loop 明确，但外部 VTK 调用成本需确认。 |

## 6. 全量文件覆盖表

| 文件 | 优先级 | 是否候选 | 判断依据 | 关联实现 |
| --- | --- | --- | --- | --- |
| `include/pcl/compression/color_coding.h` | `mid` | 是 | 有明确逐点颜色编码片段，但是否覆盖压缩主成本需二轮判断。 | `-` |
| `include/pcl/compression/compression_profiles.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `include/pcl/compression/entropy_range_coder.h` | `low` | 否 | range coder 公开声明，真实状态机在 impl；不单独进入二轮 | `-` |
| `include/pcl/compression/impl/entropy_range_coder.hpp` | `low` | 否 | 自适应频率表和 range coder 状态跨 byte 串行更新，难以作为 RVV 主候选 | `-` |
| `include/pcl/compression/impl/octree_pointcloud_compression.hpp` | `mid` | 是 | 作为压缩入口伴随文件保留。 | `-` |
| `include/pcl/compression/impl/organized_pointcloud_compression.hpp` | `high` | 是 | 覆盖 organized 点云压缩前后处理的批量路径，和压缩格式入口直接相邻。 | `-` |
| `include/pcl/compression/libpng_wrapper.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `include/pcl/compression/octree_pointcloud_compression.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `include/pcl/compression/organized_pointcloud_compression.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `include/pcl/compression/organized_pointcloud_conversion.h` | `high` | 是 | 多个公开压缩转换入口都落在逐点同构公式上。 | `-` |
| `include/pcl/compression/point_coding.h` | `mid` | 是 | 有清晰坐标差分批量公式，但受 octree leaf 分布限制。 | `-` |
| `ascii_io.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `impl/ascii_io.hpp` |
| `auto_io.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `impl/auto_io.hpp` |
| `buffers.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `impl/buffers.hpp` |
| `davidsdk_grabber.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `debayer.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `src/debayer.cpp` |
| `depth_sense/depth_sense_device_manager.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `depth_sense/depth_sense_grabber_impl.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `depth_sense_grabber.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `dinast_grabber.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `ensenso_grabber.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `file_grabber.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `file_io.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `fotonic_grabber.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `grabber.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `hdl_grabber.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `ifs_io.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `image.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `image_depth.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `src/image_depth.cpp` |
| `image_grabber.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `src/image_grabber.cpp` |
| `image_ir.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `src/image_ir.cpp` |
| `image_metadata_wrapper.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `image_rgb24.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `src/image_rgb24.cpp` |
| `image_yuv422.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `src/image_yuv422.cpp` |
| `impl/ascii_io.hpp` | `low` | 否 | 未发现足以进入二轮基线的可 SIMD/RVV 批量数值片段 | `-` |
| `impl/auto_io.hpp` | `low` | 否 | 未发现足以进入二轮基线的可 SIMD/RVV 批量数值片段 | `-` |
| `impl/buffers.hpp` | `mid` | 是 | Average path 有 SIMD 价值，median path 风险高。 | `-` |
| `impl/lzf_image_io.hpp` | `high` | 是 | 解压后的像素到点云转换是大规模连续批量算术。 | `-` |
| `impl/pcd_io.hpp` | `mid` | 是 | 模板写路径含批量字段搬运。 | `-` |
| `impl/point_cloud_image_extractors.hpp` | `high` | 是 | organized 点云转图像是逐点字段转换。 | `-` |
| `impl/synchronized_queue.hpp` | `low` | 否 | 未发现足以进入二轮基线的可 SIMD/RVV 批量数值片段 | `-` |
| `impl/vtk_lib_io.hpp` | `mid` | 是 | 存在逐点字段转换，但可能被 VTK 调用稀释。 | `-` |
| `io_exception.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `low_level_io.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `lzf.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `src/lzf.cpp` |
| `lzf_image_io.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `impl/lzf_image_io.hpp` |
| `obj_io.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `src/obj_io.cpp` |
| `oni_grabber.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni2/openni.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni2/openni2_convert.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni2/openni2_device.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni2/openni2_device_info.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni2/openni2_device_manager.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni2/openni2_frame_listener.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni2/openni2_metadata_wrapper.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni2/openni2_timer_filter.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni2/openni2_video_mode.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni2/openni_shift_to_depth_conversion.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni2_grabber.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `src/openni2_grabber.cpp` |
| `openni_camera/openni.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni_camera/openni_depth_image.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni_camera/openni_device.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni_camera/openni_device_kinect.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni_camera/openni_device_oni.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni_camera/openni_device_primesense.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni_camera/openni_device_xtion.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni_camera/openni_driver.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni_camera/openni_exception.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni_camera/openni_image.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni_camera/openni_image_bayer_grbg.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni_camera/openni_image_rgb24.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni_camera/openni_image_yuv_422.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni_camera/openni_ir_image.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni_camera/openni_shift_to_depth_conversion.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `openni_grabber.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `src/openni_grabber.cpp` |
| `pcd_grabber.h` | `mid` | 是 | 头内有真实 organized loop，但更可能作为 PCD 播放上下文候选。 | `-` |
| `pcd_io.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `impl/pcd_io.hpp, src/pcd_io.cpp` |
| `ply/byte_order.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `ply/io_operators.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `ply/ply.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `ply/ply_parser.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `ply_io.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `src/ply_io.cpp` |
| `png_io.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `point_cloud_image_extractors.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `impl/point_cloud_image_extractors.hpp` |
| `real_sense/real_sense_device_manager.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `real_sense_2_grabber.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `real_sense_grabber.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `robot_eye_grabber.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `split.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `tar.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `tim_grabber.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `timestamp.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `vlp_grabber.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `-` |
| `vtk_io.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `src/vtk_io.cpp` |
| `vtk_lib_io.h` | `low` | 否 | 公开接口、类型声明或薄封装为主，未承载可下钻批量 loop | `impl/vtk_lib_io.hpp, src/vtk_lib_io.cpp` |
| `src/ascii_io.cpp` | `low` | 否 | `getline` 文本解析和字段 reader 状态主导 | `-` |
| `src/auto_io.cpp` | `low` | 否 | 未发现足以进入二轮基线的可 SIMD/RVV 批量数值片段 | `-` |
| `src/compression.cpp` | `low` | 否 | 未发现足以进入二轮基线的可 SIMD/RVV 批量数值片段 | `-` |
| `src/davidsdk_grabber.cpp` | `low` | 否 | grabber 调度、设备状态或回调主导，未发现可独立下钻的批量数值 loop | `-` |
| `src/debayer.cpp` | `high` | 是 | Bayer 解码主体是大图像同构 stencil（邻域模板）计算。 | `-` |
| `src/depth_sense/depth_sense_device_manager.cpp` | `low` | 否 | 设备枚举、属性封装或管理逻辑为主 | `-` |
| `src/depth_sense/depth_sense_grabber_impl.cpp` | `mid` | 是 | 有逐点帧转换片段，但依赖 DepthSense 入口热度。 | `-` |
| `src/depth_sense_grabber.cpp` | `low` | 否 | grabber 调度、设备状态或回调主导，未发现可独立下钻的批量数值 loop | `-` |
| `src/dinast_grabber.cpp` | `mid` | 是 | 数学密度高但设备路径专门。 | `-` |
| `src/ensenso_grabber.cpp` | `mid` | 是 | 有规则逐点字段转换，但主成本需 profile。 | `-` |
| `src/hdl_grabber.cpp` | `mid` | 是 | LiDAR packet 转换有重复数学片段。 | `-` |
| `src/ifs_io.cpp` | `low` | 否 | IFS 文本/二进制解析和 facet 输出为主，三坐标固定小循环不足以入候选 | `-` |
| `src/image_depth.cpp` | `high` | 是 | 深度帧填充是公开图像接口主循环。 | `-` |
| `src/image_grabber.cpp` | `mid` | 是 | 有帧到点云转换片段。 | `-` |
| `src/image_ir.cpp` | `low` | 否 | IR raw/downsample 主要是简单字段 copy，算术密度不足 | `-` |
| `src/image_rgb24.cpp` | `mid` | 是 | 灰度转换有明确批量公式，但主收益可能有限。 | `-` |
| `src/image_yuv422.cpp` | `high` | 是 | YUV422->RGB 是典型逐像素颜色转换。 | `-` |
| `src/io_exception.cpp` | `low` | 否 | 异常类型实现，无批量路径 | `-` |
| `src/libpng_wrapper.cpp` | `low` | 否 | libpng 外部库调用和 row copy 主导 | `-` |
| `src/lzf.cpp` | `low` | 否 | LZF 压缩 / 解压是 byte 串行状态机和 back-reference 依赖 | `-` |
| `src/lzf_image_io.cpp` | `mid` | 是 | 写路径存在规则重排。 | `-` |
| `src/obj_io.cpp` | `mid` | 是 | 有几何局部片段，但 OBJ I/O 主成本不一定在该片段。 | `-` |
| `src/oni_grabber.cpp` | `mid` | 是 | 有帧转点云公式，但可能作为 OpenNI 同类入口保留。 | `-` |
| `src/openni2/openni2_convert.cpp` | `low` | 否 | 未发现足以进入二轮基线的可 SIMD/RVV 批量数值片段 | `-` |
| `src/openni2/openni2_device.cpp` | `low` | 否 | supported modes 遍历和设备状态管理为主 | `-` |
| `src/openni2/openni2_device_info.cpp` | `low` | 否 | 设备枚举、属性封装或管理逻辑为主 | `-` |
| `src/openni2/openni2_device_manager.cpp` | `low` | 否 | 设备枚举、属性封装或管理逻辑为主 | `-` |
| `src/openni2/openni2_timer_filter.cpp` | `low` | 否 | 未发现足以进入二轮基线的可 SIMD/RVV 批量数值片段 | `-` |
| `src/openni2/openni2_video_mode.cpp` | `low` | 否 | 设备枚举、属性封装或管理逻辑为主 | `-` |
| `src/openni2_grabber.cpp` | `high` | 是 | OpenNI2 grabber 的帧到点云转换是直接用户可见批量路径。 | `-` |
| `src/openni_camera/openni_depth_image.cpp` | `high` | 是 | OpenNI 旧接口仍有独立深度图转换实现。 | `-` |
| `src/openni_camera/openni_device.cpp` | `low` | 否 | shift lookup 生成固定小规模，其余是 callback / device 状态 | `-` |
| `src/openni_camera/openni_device_kinect.cpp` | `low` | 否 | 未发现足以进入二轮基线的可 SIMD/RVV 批量数值片段 | `-` |
| `src/openni_camera/openni_device_oni.cpp` | `low` | 否 | 未发现足以进入二轮基线的可 SIMD/RVV 批量数值片段 | `-` |
| `src/openni_camera/openni_device_primesense.cpp` | `low` | 否 | 未发现足以进入二轮基线的可 SIMD/RVV 批量数值片段 | `-` |
| `src/openni_camera/openni_device_xtion.cpp` | `low` | 否 | 未发现足以进入二轮基线的可 SIMD/RVV 批量数值片段 | `-` |
| `src/openni_camera/openni_driver.cpp` | `low` | 否 | node/device 枚举和 URI 解析状态主导 | `-` |
| `src/openni_camera/openni_exception.cpp` | `low` | 否 | 异常类型实现，无批量路径 | `-` |
| `src/openni_camera/openni_image_bayer_grbg.cpp` | `high` | 是 | 文件承载 OpenNI Bayer 图像转换主循环。 | `-` |
| `src/openni_camera/openni_image_rgb24.cpp` | `mid` | 是 | 与通用 RGB24 同构，保留供二轮统一评估。 | `-` |
| `src/openni_camera/openni_image_yuv_422.cpp` | `high` | 是 | 与通用 YUV422 同构的 OpenNI 入口。 | `-` |
| `src/openni_camera/openni_ir_image.cpp` | `low` | 否 | IR raw/downsample 主要是简单 copy，算术密度不足 | `-` |
| `src/openni_grabber.cpp` | `high` | 是 | 旧 OpenNI grabber 的帧转点云主路径和 OpenNI2 同类。 | `-` |
| `src/pcd_grabber.cpp` | `low` | 否 | PCD 文件列表 / TAR 读取和 reader 调度为主，头内 organized 片段另列 mid | `-` |
| `src/pcd_io.cpp` | `mid` | 是 | PCD 是常用入口，存在批量字段处理。 | `-` |
| `src/ply/ply_parser.cpp` | `low` | 否 | PLY parser 状态机和文本 / 属性事件处理主导 | `-` |
| `src/ply_io.cpp` | `mid` | 是 | PLY 文件中有可批量 field 片段。 | `-` |
| `src/png_io.cpp` | `low` | 否 | PNG 外部库 I/O 和 image wrapper 调度为主 | `-` |
| `src/real_sense/real_sense_device_manager.cpp` | `low` | 否 | 设备枚举、属性封装或管理逻辑为主 | `-` |
| `src/real_sense_2_grabber.cpp` | `mid` | 是 | 有逐点转换片段，但外部 SDK 成本需确认。 | `-` |
| `src/real_sense_grabber.cpp` | `mid` | 是 | 有批量帧转换片段。 | `-` |
| `src/robot_eye_grabber.cpp` | `mid` | 是 | 有 LiDAR 点解码片段。 | `-` |
| `src/tim_grabber.cpp` | `mid` | 是 | 有数学循环但主成本和热度不确定。 | `-` |
| `src/vlp_grabber.cpp` | `mid` | 是 | 与 HDL 同类，保留供二轮判断是否归并。 | `-` |
| `src/vtk_io.cpp` | `mid` | 是 | 有逐点字段转换，但可能 I/O 主导。 | `-` |
| `src/vtk_lib_io.cpp` | `mid` | 是 | 转换 loop 明确，但外部 VTK 调用成本需确认。 | `-` |

## 7. 二轮交接说明

- 第二轮筛选应读取本文件，并把所有 `high/mid` 文件作为初始候选基线逐项交代去向。
- 第二轮可以将 `high/mid` 降级为暂缓、不推荐或不单独实施，但必须说明源码证据和主成本覆盖原因。
- 第二轮可以补入 `low` 或初筛遗漏文件，但必须说明补入来源、证据和为什么没有扩展成重新全模块 / 全库筛选。
- 第二轮输出应使用 `doc-rvv/library-screening/io/io-function-evaluation-queue.zh.md`，并按“建议进行 RVV 优化的文件 / 保留实施的候选文件 / 暂缓或不推荐考虑 RVV 优化的文件”三类组织。
- `high` 中的图像转换、OpenNI/OpenNI2 帧转点云和 organized compression 转换可能存在同构实现；第二轮应优先判断是否按“共享转换族”归并评估，避免为同一公式建立重复 topic。
