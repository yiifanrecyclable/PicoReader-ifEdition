idf_component_register(SRCS "md4c.c" INCLUDE_DIRS ".")
# 解析器优先节约程序空间；硬件/显示组件沿用原有优化。/ Optimize parser size; hardware/display flags stay unchanged.
target_compile_options(${COMPONENT_LIB} PRIVATE -Os)
