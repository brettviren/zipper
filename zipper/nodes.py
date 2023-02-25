import json
import numpy

def typename(node):
    return node['type'] + ':' + node['name']

def msg_arrays(node):
    marrs = dict()
    for name, marr in node['msgs'].items():
        if marr is None: continue
        one = dict()
        one['pays'] = numpy.array(marr['pays'], dtype=float)
        one['ords'] = numpy.array(marr['ords'], dtype=int)
        one['ids'] = numpy.array(marr['ids'], dtype=int)
        one['ts'] = numpy.array(marr['ts'], dtype=int)
        marrs[name] = one;
    return marrs

def load(data):
    if isinstance(data, str):
        data = json.load(open(data))
    if isinstance(data, dict):
        if 'nodes' in data:
            return {typename(n):msg_arrays(n) for n in data['nodes'] if n['type'] != 'random'}
    raise ValueError(f'can not load data of type {type(data)}')
