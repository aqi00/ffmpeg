cmake_minimum_required(VERSION 3.18.1) # 指定CMake的最低要求版本号

set(target ffmpeg) # 设置环境变量的名称（target）及其取值（ffmpeg）
project(${target}) # 指定项目的名称

file(GLOB srcs *.cpp *.c) # 把所有cpp文件和c文件都放入名称为srcs的集合中
include_directories(include) # 添加头文件的所在目录
link_directories(../jniLibs/${ANDROID_ABI}) # 指定so文件的链接目录
add_library(${target} SHARED ${srcs}) # 生成扩展名为so的动态库文件

# 设置名叫ffmpeg-libs的库集合，指定它包括哪些so库文件
set(ffmpeg-libs avformat avcodec avfilter swresample swscale avutil)

# 设置名叫native-libs的库集合，指定它包括哪些so库文件
# 其中GLESv3表示第三代的OpenGL ES，用于渲染三维图形；
# EGL为OpenGL ES与系统底层之间的接口层，OpenSLES用于播放音频；
# log表示日志库，m表示数学函数库，z表示zlib库。
set(native-libs android EGL GLESv3 OpenSLES log m z)

# 指定要链接哪些库。其中log表示日志库，ffmpeg-libs和native-libs设置见上
target_link_libraries(${target} log ${ffmpeg-libs} ${native-libs})