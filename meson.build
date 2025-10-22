project(
    'DataPathXpress',
    'c',
    'cpp',
    'java',
    version: '0.0.1',
    default_options: [
        'warning_level=3',
        'optimization=3',
        'cpp_std=c++20',
        'c_std=c2x',
        'buildtype=release',
    ],
)

# Compiler Arguments

c_cpp_extra_args = [
    '-Wno-c99-extensions',
    '-Wno-gnu-anonymous-struct',
    '-Wno-nested-anon-types',
    '-Wno-zero-length-array',
    '-Wno-deprecated-enum-enum-conversion',
    '-Wno-flexible-array-extensions',
    '-DDOCA_ALLOW_EXPERIMENTAL_API',
    '-DFLEXIO_ALLOW_EXPERIMENTAL_API',
]

cpp_extra_args = [
    '-Wno-c++23-extensions',
]

if get_option('buildtype') == 'release'
    c_cpp_extra_args += '-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO'
    c_cpp_extra_args += '-DNDEBUG'
elif get_option('buildtype') == 'plain'
    c_cpp_extra_args += '-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG'
    c_cpp_extra_args += '-DNDEBUG'
elif get_option('buildtype') == 'debugoptimized'
    c_cpp_extra_args += '-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE'
elif get_option('buildtype') == 'debug'
    c_cpp_extra_args += '-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE'
    c_cpp_extra_args += '-DENABLE_CHECK_FOOTPRINT'
else
    error('Unknown build type')
endif

add_project_arguments(cpp_extra_args, language: ['cpp'])
add_project_arguments(c_cpp_extra_args, language: ['c', 'cpp'])

# Dependencies

## JNI
jni_dep = dependency('jni', version: '= 1.8.0', modules: ['jvm'])
## pthread
pthread_dep = dependency('threads', required: true)
## glaze
cmake = import('cmake')
glaze_opt = cmake.subproject_options()
glaze_opt.add_cmake_defines(
    {
        'glaze_ENABLE_AVX2': 'ON',
        'glaze_DEVELOPER_MODE': 'OFF',
    },
)
glaze_dep = cmake.subproject('glaze', options: glaze_opt).dependency(
    'glaze_glaze',
)
## Boost.Fiber
boost_fiber_dep = dependency(
    'boost',
    modules: ['fiber', 'context'],
    required: true,
)
## dbg-macro
dbg_dep = subproject('dbg-macro').get_variable('dbg_macro_dep')
## zpp_bits
zpp_bits_dep = subproject('zpp_bits').get_variable('zpp_bits_dep')
## spdlog
spdlog_dep = subproject(
    'spdlog',
    default_options: ['tests=disabled', 'std_format=enabled'],
    required: true,
).get_variable('spdlog_dep')
## simdutf
simdutf_opt = cmake.subproject_options()
simdutf_opt.add_cmake_defines(
    {
        'SIMDUTF_TESTS': 'OFF',
        'SIMDUTF_TOOLS': 'OFF',
        'SIMDUTF_BENCHMARKS': 'OFF',
        'SIMDUTF_FUZZERS': 'OFF',
        'SIMDUTF_SANITIZE': 'OFF',
        'SIMDUTF_CXX_STANDARD': '20',
        'CMAKE_POSITION_INDEPENDENT_CODE': true,
    },
)
simdutf_dep = cmake.subproject('simdutf', options: simdutf_opt).dependency(
    'simdutf',
)
## librdmacm & libibverbs
rdma_deps = [
    dependency('librdmacm', required: true),
    dependency('libibverbs', required: true),
]
## liburing
uring_dep = dependency('liburing')
## args
args_dep = subproject('args').get_variable('args_dep')
## DOCA
doca_deps = [
    dependency('libmlx5', required: true),
    dependency('doca-common', required: true),
    dependency('doca-rdma', required: true),
    dependency('doca-comch', required: true),
    dependency('doca-dma', required: true),
    dependency('doca-dpa', required: true),
    dependency('libflexio', required: true),
]
## SPDK
spdk_dep = subproject('spdk').get_variable('spdk_dep')
# NOTICE: only build but not use currently

## SPSCQueue
SPSCQueue_dep = subproject('SPSCQueue').get_variable('SPSCQueue_dep')
MPMCQueue_dep = subproject('MPMCQueue').get_variable('MPMCQueue_dep')

# Common Include Directories

basic_incs = [
    include_directories('lib'),
]

# DPACC Configuration

dpacc = find_program('dpacc', required: true)

dpacc_devicecc_array = [
    '-DE_MODE_LE',
    '-Wall',
    '-Wextra',
    '-Wpedantic',
    '-Wdouble-promotion',
    '-Wno-empty-translation-unit',
    '-Wno-deprecated-declarations',
    '-Wno-gnu-zero-variadic-macro-arguments',
    '-Wmissing-prototypes',
    '-Wstrict-prototypes',
    '-ffreestanding',
    '-mcmodel=medany',
    '-nostdlib',
    '-mno-relax',
    '-mabi=lp64',
]

doca_dev_lib_path = ''
if (build_machine.cpu_family() == 'aarch64')
    doca_dev_lib_path = '/opt/mellanox/doca/lib/aarch64-linux-gnu/'
elif (build_machine.cpu_family() == 'x86_64')
    doca_dev_lib_path = '/opt/mellanox/doca/lib/x86_64-linux-gnu/'
else
    error('Unsupported cpu family')
endif

dpacc_compile_command_options = [
    '--mcpu',
    'nv-dpa-bf3',
    '--hostcc',
    'clang',
    '--hostcc-options',
    '-Werror,-Wall,-Wextra,-Wno-deprecated-declarations,-fPIC',
    '--devicecc-options',
    ','.join(dpacc_devicecc_array),
    '--device-libs',
    '-L' + doca_dev_lib_path + ' -ldoca_dpa_dev -ldoca_dpa_dev_comm',
    '-I' + meson.project_source_root() + '/lib',
    '-I/opt/mellanox/doca/include',
    '-flto',
]

subdir('hook')
subdir('lib')
subdir('example')
subdir('sample')
subdir('app')
subdir('tool')
