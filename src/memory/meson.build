memory = static_library(
  'memory',
  'fb_pool.cxx',
  include_directories: inc,
  dependencies: [
    memory_dep,
  ]
)

memory_dep = declare_dependency(
  link_with: memory,
)
