project('CM4all MyProxy', 'cpp', version: '0.13',
  meson_version: '>= 0.56',
  default_options: [
    'cpp_std=c++2a',
    'warning_level=3',
  ],
)

debug = get_option('b_ndebug') != 'true'

compiler = meson.get_compiler('cpp')

conf = configuration_data()

common_flags = [
  '-D_GNU_SOURCE',
]

test_common_flags = [
  '-Wcast-align',
  '-Wcast-qual',
  '-Wdouble-promotion',
  '-Wfloat-equal',
  '-Wmissing-declarations',
  '-Wmissing-noreturn',
  '-Wmissing-format-attribute',
  '-Wredundant-decls',
  '-Wshadow',
  '-Wundef',
  '-Wunused',
  '-Wvla',
  '-Wwrite-strings',

  '-Wno-missing-field-initializers',

  # clang specific warning options:
  '-Wunreachable-code-aggressive',
  '-Wused-but-marked-unused',
]

test_global_common_flags = [
  '-fvisibility=hidden',
]

add_global_link_arguments(
  compiler.get_supported_link_arguments(
    # make relocations read-only (hardening)
    '-Wl,-z,relro',

    # no lazy binding, please - not worth it for a daemon
    '-Wl,-z,now',
  ),
  language: 'cpp'
)

if not debug
  test_global_common_flags += [
    '-ffunction-sections',
    '-fdata-sections',
  ]

  add_global_link_arguments(
    compiler.get_supported_link_arguments(
      '-Wl,--gc-sections',
      '-Wl,--icf=all',
    ),
    language: 'cpp'
  )
endif

test_global_cxxflags = test_global_common_flags + [
]

test_cxxflags = test_common_flags + [
  '-fno-threadsafe-statics',
  '-fmerge-all-constants',

  '-Wcomma-subscript',
  '-Wextra-semi',
  '-Wmismatched-tags',
  '-Woverloaded-virtual',
  '-Wsign-promo',
  '-Wvolatile',
  '-Wvirtual-inheritance',

  # a vtable without a dtor is just fine
  '-Wno-non-virtual-dtor',

  # clang specific warning options:
  '-Wcomma',
  '-Wheader-hygiene',
  '-Winconsistent-missing-destructor-override',
]

add_global_arguments(common_flags, language: 'cpp')
add_global_arguments(compiler.get_supported_arguments(test_global_cxxflags), language: 'cpp')
add_project_arguments(compiler.get_supported_arguments(test_cxxflags), language: 'cpp')

libsystemd = dependency('libsystemd', required: get_option('systemd'))

inc = include_directories(
  'src',
  'libcommon/src',
)

libcommon_enable_json = false

sources = []

subdir('libcommon/src/util')
subdir('libcommon/src/lib/fmt')
subdir('libcommon/src/lib/nlohmann_json')
subdir('libcommon/src/lib/sodium')
subdir('libcommon/src/io')
subdir('libcommon/src/io/linux')
subdir('libcommon/src/system')
subdir('libcommon/src/net')
subdir('libcommon/src/event')
subdir('libcommon/src/event/net')

subdir('libcommon/src/co')
subdir('libcommon/src/lua')
subdir('libcommon/src/lua/io')
subdir('libcommon/src/lua/net')
subdir('libcommon/src/lua/event')
subdir('libcommon/src/lua/sodium')

if libsystemd.found()
  # systemd support also enables the systemd-resolved client which
  # uses a protocol with JSON payloads
  sources += 'src/AsyncResolver.cxx'

  subdir('libcommon/src/event/systemd')
  libsystemd = event_systemd_dep
endif

libcommon_require_pg = get_option('pg')
subdir('libcommon/src/pg')
if pg_dep.found()
  subdir('libcommon/src/lua/pg')
  pg_dep = lua_pg_dep
endif

conf.set('HAVE_LIBSYSTEMD', libsystemd.found())
conf.set('HAVE_PG', pg_dep.found())
configure_file(output: 'config.h', configuration: conf)

my = static_library(
  'my',
  'src/MysqlReader.cxx',
  'src/MysqlParser.cxx',
  'src/MysqlMakePacket.cxx',
  'src/MysqlForwardPacket.cxx',
  'src/MysqlAuth.cxx',
  'src/MysqlTextResultsetParser.cxx',
  include_directories: inc,
  dependencies: [
    event_net_dep,
    util_dep,
    dependency('libmd'), # SHA-1 implementation for "mysql_native_password"
  ],
)

my_dep = declare_dependency(
  link_with: my,
)

executable(
  'cm4all-myproxy',
  sources,
  'src/system/SetupProcess.cxx',
  'src/Options.cxx',
  'src/Cluster.cxx',
  'src/Check.cxx',
  'src/LResolver.cxx',
  'src/Policy.cxx',
  'src/Peer.cxx',
  'src/Connection.cxx',
  'src/LHandler.cxx',
  'src/LClient.cxx',
  'src/LAction.cxx',
  'src/Instance.cxx',
  'src/Reload.cxx',
  'src/CommandLine.cxx',
  'src/Main.cxx',
  include_directories: inc,
  dependencies: [
    libsystemd,
    my_dep,
    util_dep,
    io_linux_dep,
    net_dep,
    event_dep,
    lua_dep,
    lua_io_dep,
    lua_net_dep,
    lua_event_dep,
    lua_sodium_dep,
    pg_dep,
    sodium_dep,
  ],
  install: true,
  install_dir: 'sbin',
)

subdir('test')
subdir('doc')
