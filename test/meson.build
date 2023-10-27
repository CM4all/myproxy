executable(
  'RunCheck',
  'RunCheck.cxx',
  '../src/Check.cxx',
  '../src/Peer.cxx',
  include_directories: inc,
  dependencies: [
    my_dep,
    event_net_dep,
  ],
)

if resolved_dep.found()
  executable(
    'RunResolvedClient',
    'RunResolvedClient.cxx',
    include_directories: inc,
    dependencies: [
      resolved_dep,
      event_net_dep,
    ],
  )
endif
