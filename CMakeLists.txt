#需求的最低cmake程序版本
cmake_minimum_required(VERSION 3.12)
# 发布版
add_definitions (-DDEBUG)

# 设置 Release 模式
#set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)

#本工程的名字
project(OpenGL_Lecture)

#本工程支持的C++版本
set(CMAKE_CXX_STANDARD 17)


file(GLOB ASSETS "./assets" )

file(COPY ${ASSETS} DESTINATION ${CMAKE_BINARY_DIR})


include_directories(
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/include
)

link_directories(
	SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/lib
)

add_subdirectory(wrapper)
add_subdirectory(application)
add_subdirectory(glframework)
add_subdirectory(imgui)

#本工程所有cpp文件编译链接，生成exe
add_executable(openglStudy "main.cpp" "glad.c")

target_link_libraries(openglStudy glfw3.lib assimp-vc143-mtd.lib  poly2tri.lib  wrapper app fw imguilib)

# 添加自定义命令和目标，用于复制DLL
add_custom_command(
    TARGET openglStudy POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_CURRENT_SOURCE_DIR}/thirdParty/bin/assimp-vc143-mtd.dll"
        $<TARGET_FILE_DIR:openglStudy>
    COMMENT "复制Assimp DLL到可执行文件目录"
)