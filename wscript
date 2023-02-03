from waflib.Utils import to_list

def options(opt):
    opt.load('compiler_cxx waf_unit_test')
    opt.add_option('--debug-flags', type=str, default="",
                   help="Use debug flags, disabling optimization")
    opt.add_option('--simulation', type=str, default="",
                   help="Path to full simcpp20 source tree, required for simzip")

def configure(cfg):
    cfg.load('compiler_cxx waf_unit_test')

    cfg.env.CXXFLAGS += [ '-Wall','-Werror','-pedantic', '-I'+cfg.path.abspath() ]
    if cfg.options.debug_flags:
        cfg.env.CXXFLAGS += to_list(cfg.options.debug_flags)
    else:
        cfg.env.CXXFLAGS += ["-O2"]

    if cfg.options.simulation:
        sdir = cfg.path.find_dir(cfg.options.simulation)
        idir = sdir.find_dir('include')
        edir = sdir.find_dir('examples')
        cfg.env.CXXFLAGS_SIMCPP20 = [ '-fcoroutines', '-std=c++20',
                                      '-I' + idir.abspath(), '-I' + edir.abspath() ]
        cfg.check_cxx(header_name="fschuetz04/simcpp20.hpp",
                      use='SIMCPP20',
                      uselib_store='SIMCPP20',
                      define_name='HAVE_SIMCPP20')
        cfg.check_cxx(header_name="resource.hpp",
                      use='SIMCPP20',
                      uselib_store='SIMCPP20',
                      define_name='HAVE_SIMCPP20')
    cfg.write_config_header("config.hpp")
    #print (cfg.env)

def build(bld):
    if 'HAVE_SIMCPP20' in bld.env:
        bld(features='c cxx cxxprogram', use='SIMCPP20',
            source="simzip.cpp", target="simzip")
        
    for stress in ("zipper","lossy"):
        name = f'stress_{stress}'
        bld(features='cxx cxxprogram',
            source=f'{name}.cpp', target=name)

    for tsrc in bld.path.ant_glob("test/test_*.cpp"):
        name = tsrc.name.replace(".cpp","")
        if "_simcpp20_" in name or "_simzip_" in name:
            if "HAVE_SIMCPP20" in bld.env:
                bld.program(features='test', source=[tsrc], target=name, use='SIMCPP20')
                continue
        bld.program(features='test', source=[tsrc], target=name)
        
    from waflib.Tools import waf_unit_test
    bld.add_post_fun(waf_unit_test.summary)
