#!/usr/bin/env waf
# -*- python -*-
'''
The main product of this package is zipper.hpp.  Copy it somewhere.

It provides some compiled tests and example applications.

For full build with tests, 

 $ waf configure 
 $ waf 

'''

from waflib.Utils import to_list

def options(opt):
    opt.load('compiler_cxx waf_unit_test')
    opt.add_option('--debug-flags', type=str, default="",
                   help="Use debug flags, disabling optimization")
    # opt.add_option('--nlohmann-json-include', type=str, default="",
    #                help="Path holding nlohmann/json include dir, required for simzip")

def configure(cfg):
    cfg.load('compiler_cxx waf_unit_test')
    cfg.env.CXXFLAGS += [ '-Wall','-Werror','-pedantic', '-I'+cfg.path.abspath() ]
    if cfg.options.debug_flags:
        cfg.env.CXXFLAGS += to_list(cfg.options.debug_flags)
    else:
        cfg.env.CXXFLAGS += ["-O2"]

    # if cfg.options.nlohmann_json_include:
    #     idir = cfg.path.find_dir(cfg.options.nlohmann_json_include)
    #     cfg.env.CXXFLAGS_JSON = [ '-I' + idir.abspath() ]
    #     cfg.check_cxx(header_name="nlohmann/json.hpp",
    #                   use='JSON',
    #                   uselib_store='JSON',
    #                   define_name='HAVE_JSON')

    cfg.write_config_header("config.hpp")

def build(bld):
    for ssrc in bld.path.ant_glob("stress/stress_*.cpp"):
        name = ssrc.name.replace(".cpp","")
        bld(features='cxx cxxprogram',
            source=[ssrc], target=name)

    for tsrc in bld.path.ant_glob("test/test_*.cpp"):
        name = tsrc.name.replace(".cpp","")
        bld.program(features='test', source=[tsrc], target=name)

    from waflib.Tools import waf_unit_test
    bld.add_post_fun(waf_unit_test.summary)
