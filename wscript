#!/usr/bin/env waf
# -*- python -*-
'''
The main product of this package is zipper.hpp.  Copy it somewhere.

It provides some compiled tests and example applications.

For full build, 

 $ git clone https://github.com/fschuetz04/simcpp20.git
 $ git clone https://github.com/nlohmann/json.git
 $ waf configure --fschuetz04-simcpp20-include=simcpp20/include --nlohmann-json-include=json/include 
 $ waf 
 $ ./build/simzip
'''


from waflib.Utils import to_list

def options(opt):
    opt.load('compiler_cxx waf_unit_test')
    opt.add_option('--debug-flags', type=str, default="",
                   help="Use debug flags, disabling optimization")
    opt.add_option('--fschuetz04-simcpp20-include', type=str, default="",
                   help="Path holding fschuetz04/simcpp20 include dir, required for simzip")
    opt.add_option('--nlohmann-json-include', type=str, default="",
                   help="Path holding nlohmann/json include dir, required for simzip")

def configure(cfg):
    cfg.load('compiler_cxx waf_unit_test')

    cfg.env.CXXFLAGS += [ '-Wall','-Werror','-pedantic', '-I'+cfg.path.abspath() ]
    if cfg.options.debug_flags:
        cfg.env.CXXFLAGS += to_list(cfg.options.debug_flags)
    else:
        cfg.env.CXXFLAGS += ["-O2"]

    if cfg.options.fschuetz04_simcpp20_include:
        idir = cfg.path.find_dir(cfg.options.fschuetz04_simcpp20_include)
        cfg.env.CXXFLAGS_SIMCPP20 = [ '-fcoroutines', '-std=c++20',
                                      '-I' + idir.abspath()]
        cfg.check_cxx(header_name="fschuetz04/simcpp20.hpp",
                      use='SIMCPP20',
                      uselib_store='SIMCPP20',
                      define_name='HAVE_SIMCPP20')
    if cfg.options.nlohmann_json_include:
        idir = cfg.path.find_dir(cfg.options.nlohmann_json_include)

        cfg.env.CXXFLAGS_JSON = [ '-I' + idir.abspath() ]
        cfg.check_cxx(header_name="nlohmann/json.hpp",
                      use='JSON',
                      uselib_store='JSON',
                      define_name='HAVE_JSON')

    cfg.write_config_header("config.hpp")
    print (cfg.env)

def build(bld):
    if 'HAVE_SIMCPP20' in bld.env and 'HAVE_JSON' in bld.env:
        bld(features='c cxx cxxprogram', use='SIMCPP20 JSON',
            source="simzip.cpp", target="simzip")
        
    for stress in ("zipper","lossy"):
        name = f'stress_{stress}'
        bld(features='cxx cxxprogram',
            source=f'{name}.cpp', target=name)

    for tsrc in bld.path.ant_glob("test/test_*.cpp"):
        name = tsrc.name.replace(".cpp","")
        if "_simcpp20_" in name:
            if "HAVE_SIMCPP20" in bld.env:
                bld.program(features='test', source=[tsrc], target=name, use='SIMCPP20')
                continue
        elif '_simzip_' in name:
            if "HAVE_SIMCPP20" in bld.env and "HAVE_JSON" in bld.env:
                bld.program(features='test', source=[tsrc], target=name, use='SIMCPP20 JSON')
                continue


        bld.program(features='test', source=[tsrc], target=name)
        
    from waflib.Tools import waf_unit_test
    bld.add_post_fun(waf_unit_test.summary)
