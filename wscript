def options(opt):
    opt.load('compiler_cxx waf_unit_test')

def configure(cnf):
    cnf.load('compiler_cxx waf_unit_test')
    cnf.env.CXXFLAGS += ['-O2']

def build(bld):
    for stress in ("zipper","lossy"):
        name = f'stress_{stress}'
        bld(features='c cxx cxxprogram',
            source=f'{name}.cpp',
            target=name)
    for test in ("zipper","lossy","cardinality","absent"):
        name = f'test_{test}'
        bld.program(features='test', source=f'{name}.cpp', target=name)
    from waflib.Tools import waf_unit_test
    bld.add_post_fun(waf_unit_test.summary)
