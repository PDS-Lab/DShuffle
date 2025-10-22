example_deps = [dpx_trans_dep, args_dep]


examples = [
    ['bench', [], example_deps],
    ['mmap_test', [], [dpx_common_dep]],
    # ['dpa_rdma_s', [], example_deps],
    # ['spill_test', [], [dpx_spdk_spill_dep]]
]

foreach example : examples

    executable(
        example[0],
        files([example[0] + '.cxx'] + example[1]),
        dependencies: example[2],
    )

endforeach

dpa_example_deps = [dpx_common_dep]

dpa_examples = [
    # ['dpa_msgq', [], dpa_example_deps],
]

foreach dpa_example : dpa_examples

    test_src = dpa_example[0] + '.cxx'
    test_dev_src = dpa_example[0] + '_dev.c'
    test_dev_out = dpa_example[0] + '.a'
    test_dev_app = custom_target(
        dpa_example[0],
        command: [
            dpacc,
            '@INPUT@',
            '-o', meson.current_build_dir() / test_dev_out,
            '--app-name', dpa_example[0],
        ] + dpacc_compile_command_options,
        output: [test_dev_out],
        input: [test_dev_src],
    )
    test_dev_app_dep = declare_dependency(link_whole: test_dev_app)
    executable(
        dpa_example[0],
        files(test_src),
        dependencies: [test_dev_app_dep] + dpa_example[2],
        link_depends: test_dev_app,
        install: false,
    )

endforeach
