auth = static_library(
  'auth',
  'Factory.cxx',
  'ClearPassword.cxx',
  'NativePassword.cxx',
  'CachingSha2Password.cxx',
  include_directories: inc,
  dependencies: [
    event_net_dep,
    util_dep,
    dependency('libmd'), # SHA-1 implementation for "mysql_native_password"
  ],
)

auth_dep = declare_dependency(
  link_with: auth,
)
