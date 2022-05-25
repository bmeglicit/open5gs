
libmetrics_sources = files('''
    ogs-metrics.c

    context.c
'''.split())

libmicrohttpd_dep = dependency('libmicrohttpd', version: '>=0.9.40')
libmetrics_inc = include_directories('.')

metrics_cc_flags = ['-DOGS_METRICS_COMPILATION']

libmetrics = library('ogsmetrics',
    sources: libmetrics_sources,
    version : libogslib_version,
    c_args : metrics_cc_flags,
    cpp_args : metrics_cc_flags,
    include_directories : [libmetrics_inc],
    dependencies : [libmicrohttpd_dep,
                    libapp_dep],
    install : true)

libmetrics_dep = declare_dependency(
    link_with : libmetrics,
    include_directories : [libmetrics_inc],
    dependencies : [libmicrohttpd_dep,
                    libapp_dep]
)
