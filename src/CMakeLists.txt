# 设置编译器
SET(CMAKE_C_COMPILER g++)
#ADD_DEFINITIONS(-std=c++11)
SET( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -march=native -O3 -pthread" )
#SET(CMAKE_EXE_LINKER_FLAGS "-Wl,--no-as-needed")

# 头文件列表
INCLUDE_DIRECTORIES(${VIDEO_WS_SOURCE_DIR}/include)
 
# 设置生成的可执行文件的路径
SET(EXECUTABLE_OUTPUT_PATH ${VIDEO_WS_SOURCE_DIR}/bin)

# 自动生成文件列表
AUX_SOURCE_DIRECTORY(. SRC_main)
 
# 生成的可执行文件
ADD_EXECUTABLE(video-ws ${SRC_main})
 
# 所需要的库文件的目录
LINK_DIRECTORIES(${VIDEO_WS_SOURCE_DIR}/lib)
 
# 需要链接的库文件
#TARGET_LINK_LIBRARIES(video-ws sum hello)
