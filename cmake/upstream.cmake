# 上游接入：libwebp（CMake 原生）→ <build>/pkg/prefix，探针链接 libwebp 静态库。

set(PKG_ROOT "${CMAKE_BINARY_DIR}/pkg")
if(NOT DEFINED PKG_PREFIX)
  set(PKG_PREFIX "${PKG_ROOT}/prefix")
  add_custom_target(pkg ALL
    COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/cmake/pkg-build.sh" "${SRC_DIR}" "${PKG_ROOT}"
    COMMENT "构建上游 libwebp（${SRC_DIR}）"
    VERBATIM)
  set(HAVE_PKG_TARGET TRUE)
endif()

add_executable(webp_harness test/pkg_webp.c)
if(HAVE_PKG_TARGET)
  add_dependencies(webp_harness pkg)
endif()
target_include_directories(webp_harness PRIVATE "${PKG_PREFIX}/include")
target_link_directories(webp_harness PRIVATE "${PKG_PREFIX}/lib")
target_link_libraries(webp_harness PRIVATE webp sharpyuv m)
target_compile_options(webp_harness PRIVATE ${RVV_FLAGS})

add_test(NAME webp_harness COMMAND webp_harness)
