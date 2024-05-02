file(GLOB LIB_SOURCES "*.cc")

add_library(comp_net_debug STATIC ${LIB_SOURCES})

add_library(comp_net_sanitized EXCLUDE_FROM_ALL STATIC ${LIB_SOURCES})
target_compile_options(comp_net_sanitized PUBLIC ${SANITIZING_FLAGS})

add_library(comp_net_optimized EXCLUDE_FROM_ALL STATIC ${LIB_SOURCES})
target_compile_options(comp_net_optimized PUBLIC "-O2")

macro(add_app exec_name)
  add_executable("${exec_name}" "${exec_name}.cc")
  target_link_libraries("${exec_name}" comp_net_debug)
  target_link_libraries("${exec_name}" util_debug)
endmacro(add_app)

add_app(webget)
