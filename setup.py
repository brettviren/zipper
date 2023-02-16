#!/usr/bin/env python

from setuptools import setup, find_packages
setup(
    name = 'zipper',
    version = '0.0',
    packages = find_packages(),
    install_requires = [
        'Click',
        'numpy',
        'matplotlib',
#        'networkx',
#        'mayavi',
#        'vtk',
    ],
    # extras_require = {
    #     # parse TbbFlow logs and make anigif showing graph states
    #     'anidfg':  ["GraphvizAnim"] 
    # },
    entry_points = dict(
        console_scripts = [
            'zipit = zipper.__main__:main',
        ]
    )
)
