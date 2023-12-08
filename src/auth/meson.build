auth_sources = []

if crypto_dep.found()
  auth_sources += 'CachingSha2Password.cxx'
endif

auth = static_library(
  'auth',
  'Factory.cxx',
  'ClearPassword.cxx',
  'NativePassword.cxx',
  auth_sources,
  include_directories: inc,
  dependencies: [
    event_net_dep,
    util_dep,
    dependency('libmd'), # SHA-1 implementation for "mysql_native_password"
    crypto_dep,
  ],
)

auth_dep = declare_dependency(
  link_with: auth,
)
