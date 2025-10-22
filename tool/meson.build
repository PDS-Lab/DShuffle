executable(
    'uring_probe',
    files('uring_probe.cxx'),
    dependencies: uring_dep,
)

executable(
    'doca_device_probe',
    files('doca_device_probe.cxx'),
    dependencies: dependency('doca-common', required: true),
    include_directories: basic_incs,
)
