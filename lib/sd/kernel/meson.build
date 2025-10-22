dpa_sd_src = [
    './sd.c',
    '../../doca/kernel/wakeup.c',
]

dpa_sd_name = 'sd'

dpa_sd_out = dpa_sd_name + '.a'

dpa_sd = custom_target(
    dpa_sd_name,
    command: [
        dpacc,
        '@INPUT@',
        '-o', meson.current_build_dir() + '/' + dpa_sd_out,
        '--app-name', dpa_sd_name,
    ] + dpacc_compile_command_options,
    output: [dpa_sd_out],
    input: dpa_sd_src,
)

dpa_sd_dep = declare_dependency(link_with: dpa_sd)
