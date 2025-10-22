project(
    'spdk',
    'c',
    version: '24.05',
)
fs = import('fs')

cc = meson.get_compiler('c')

math_dep = cc.find_library(
    'm',
    has_headers: ['math.h'],
)
fuse_dep = dependency(
    'fuse3',
)
bsd_dep = dependency(
    'libbsd',
)
ssl_dep = dependency(
    'openssl',
    version: '>=1.1.1',
)
dlfcn_dep = cc.find_library(
    'dl',
    has_headers: ['dlfcn.h'],
)
uuid_dep = cc.find_library(
    'uuid',
    has_headers: ['uuid/uuid.h'],
)
numa_dep = cc.find_library(
    'numa',
    has_headers: ['numaif.h'],
)
archive_dep = cc.find_library(
    'archive',
    has_headers: ['archive.h'],
)

if not fs.exists('build') and not fs.exists('install')
    message('Configuring ..')
    if get_option('buildtype') == 'debug'
        run_command('spdk_configure_debug.sh', capture: true, check: true)
    else
        run_command('spdk_configure.sh', capture: true, check: true)
    endif
endif
if not fs.exists('build' / 'lib' / 'src' / 'spdk_nvme.so')
    cpu_count = run_command(
        [
            import('python').find_installation('python3'),
            '-c',
            'from multiprocessing import cpu_count; print(cpu_count())',
        ],
        check: true,
    ).stdout().strip()
    message('Building ..')
    make = find_program('make')
    run_command(
        [
            make,
            '-j',
            cpu_count,
        ],
        capture: true,
        check: true,
        env: {},
    )
endif

message('Setting up dependency ..')
spdk_libnames = (
    [
        'spdk_nvme',
        'spdk_env_dpdk',
        'spdk_thread',
        'spdk_sock',
        'spdk_sock_posix',
        'spdk_rpc',
        'spdk_trace',
        'spdk_jsonrpc',
        'spdk_json',
        'spdk_util',
        'spdk_log',
        'spdk_dma',
        'spdk_keyring',
        'spdk_vfio_user',
        'spdk_rdma_provider',
        'spdk_rdma_utils',
    ]
)
dpdk_libnames = (
    [
        'rte_eal',
        'rte_log',
        'rte_telemetry',
        'rte_bus_pci',
        'rte_pci',
        'rte_ring',
        'rte_mempool',
        'rte_kvargs',
    ]
)

isal_libnames = (
    [
        'isal',
    ]
)

spdk_deps = [
    fuse_dep,
    dlfcn_dep,
    bsd_dep,
    math_dep,
    numa_dep,
    uuid_dep,
    ssl_dep,
    ssl_dep,
    archive_dep,
]

spdk = []
foreach libname : spdk_libnames + dpdk_libnames
    spdk += cc.find_library(
        libname,
        dirs: [
            meson.current_source_dir() / 'build' / 'lib',
            meson.current_source_dir() / 'dpdk' / 'build' / 'lib',
        ],
        static: false,
    )
endforeach

foreach libname : isal_libnames
    spdk += cc.find_library(
        libname,
        dirs: [
            meson.current_source_dir() / 'isa-l' / '.libs',
        ],
        static: true,
    )
endforeach

spdk_inc = (
    include_directories(
        'dpdk' / 'build' / 'include',
        'build' / 'include',
    )
)

# Construct link_args based on the above
spdk_dep = declare_dependency(
    dependencies: spdk_deps + spdk,
    link_args: ['-Wl,--no-as-needed'],
    include_directories: spdk_inc,
)
