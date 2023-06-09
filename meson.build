project('CM4all MyProxy', 'cpp', version: '0.5',
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

  # disable boost specific code in libcommon
  '-DNO_BOOST',
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

libsystemd = dependency('libsystemd')

inc = include_directories(
  'src',
  'libcommon/src',
)

libcommon_enable_boost = false

subdir('libcommon/src/util')
subdir('libcommon/src/lib/fmt')
subdir('libcommon/src/lib/sodium')
subdir('libcommon/src/io')
subdir('libcommon/src/io/linux')
subdir('libcommon/src/system')
subdir('libcommon/src/net')
subdir('libcommon/src/event')
subdir('libcommon/src/event/net')
subdir('libcommon/src/co')
subdir('libcommon/src/lua')
subdir('libcommon/src/lua/net')
subdir('libcommon/src/lua/event')
subdir('libcommon/src/lua/sodium')

libcommon_require_pg = get_option('pg')
subdir('libcommon/src/stock')
subdir('libcommon/src/pg')
if pg_dep.found()
  subdir('libcommon/src/lua/pg')
  pg_dep = lua_pg_dep
endif

conf.set('HAVE_PG', pg_dep.found())
configure_file(output: 'config.h', configuration: conf)

executable(
  'cm4all-myproxy',
  'src/system/SetupProcess.cxx',
  'src/Cluster.cxx',
  'src/LResolver.cxx',
  'src/Policy.cxx',
  'src/MysqlReader.cxx',
  'src/MysqlParser.cxx',
  'src/MysqlMakePacket.cxx',
  'src/MysqlAuth.cxx',
  'src/Peer.cxx',
  'src/Connection.cxx',
  'src/LHandler.cxx',
  'src/LClient.cxx',
  'src/LAction.cxx',
  'src/Instance.cxx',
  'src/CommandLine.cxx',
  'src/Main.cxx',
  include_directories: inc,
  dependencies: [
    libsystemd,
    util_dep,
    io_linux_dep,
    net_dep,
    event_dep,
    event_net_dep,
    lua_dep,
    lua_net_dep,
    lua_event_dep,
    lua_sodium_dep,
    pg_dep,
    sodium_dep,
    dependency('libmd'), # SHA-1 implementation for "mysql_native_password"
  ],
  install: true,
  install_dir: 'sbin',
)

subdir('doc')
