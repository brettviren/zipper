#!/usr/bin/env python
'''
Process simzip output.
'''
import os
import json
import click
import subprocess
import numpy
import matplotlib.pyplot as plt

cmddef = dict(context_settings = dict(help_option_names=['-h', '--help']))
@click.group(**cmddef)
@click.pass_context
def cli(ctx):
    ctx.obj = dict()

def time_unit(tval, past=1):
    '''
    Return (mult, unit) for tval*mult [unit] to be > past.
    '''
    tunits = ["s", "ms", "us"]
    mult = 1.0
    for tunit in tunits:
        if tval*mult > past:
            break
        mult *= 1000
    return (mult, tunit)

def plot_node_timing(node, all_nodes):

    def make_title():
        type=node['type']
        name=node['name']
        data=node['data']
        extra=""
        if type == 'zipit':
            maxlat = data['max_latency']
            mult,tunit = time_unit(maxlat)
            holding = data['zipsize']
            lost = len(data['Rsamples']) - len(data['Ssamples']) - holding
            extra = f'maxlat:{maxlat*mult:.1f} {tunit}, holding:{holding}, lost:{lost}'
        elif type == 'source':
            delay = all_nodes[data['delay']]
            rate = 1.0/delay['data'][lifetime];
            extra = 'rate:{rate:.1f} Hz'
        return f'{type}:{name} {extra}'

    plt.clf()
    dat = node["data"]
    maxval=0
    for which in ('send','recv'):
        W = which[0].upper()
        maxval = max([maxval] + dat[f'{W}samples'])

    mult, tunit = time_unit(maxval)

    for which in ('send','recv'):
        w = which[0]
        W = w.upper()
        samps = numpy.array(dat[f'{W}samples'])*mult
        n = dat[f'{W}n']
        mu = dat[f'{W}mu']*mult
        rms = dat[f'{W}rms']*mult

        if len(samps) == 0:
            continue
        c,h = numpy.histogram(samps)
        label = f'{which}: {n} {mu:.1f}+/-{rms:.1f} {tunit}'
        plt.stairs(c,h, label=label)
    
    plt.title(make_title())
    plt.legend()
    plt.xlabel(f'time [{tunit}]')


def type_name(*args):
    if type(args[0]) == dict:
        node = args[0]
        tn = node['type']
        if 'name' in node:
            tn += ':' + node['name']
        return tn
    if len(args) > 0:
        tn = args[0]
    if len(args) > 1 and args[1]:
        tn += ":" + args[1]
    return tn

def index_nodes(nodes):
    ret = dict()
    for node in nodes:
        ret[type_name(node)] = node
    return ret

@cli.command("plot-node")
@click.option("-k", "--kind", type=str, help="node kind name")
@click.option("-i", "--inst", default="", type=str, help="node instance name")
@click.option("-o", "--outfile", type=str, help="output file")
@click.argument('infile')
def cmd_plot_one(kind, inst, outfile, infile):
    nodes = json.load(open(infile))["nodes"]
    all_nodes = index_nodes(nodes)
    node = all_nodes[type_name(kind, inst)]
    plot_node_timing(node, all_nodes)
    plt.savefig(outfile)


@cli.command("graph-plots")
@click.option("-f", "--figext", type=str, default=None, help="figure extensions")
@click.option("-o", "--outfile", type=str, help="output file")
@click.argument('infile')
def cmd_graph_plots(figext, outfile, infile):
    nodes = json.load(open(infile))["nodes"]
    all_nodes = index_nodes(nodes)

    base, ext = os.path.splitext(outfile)
    dirname = os.path.dirname(base)
    base = os.path.basename(base)

    if figext is None:
        figext = ext[1:]

    def tname(t, n=None):
        if n:
            return base + "_" + t + "_" + n
        return base + "_" + t
    def tnname(tn):
        tn = tn.split(":", 1)
        if len(tn) == 1:
            tn.append("")
        return tname(*tn)
    def oname(node):
        return tname(node['type'], node['name'])


    node_lines = ["node[shape=plain]"]
    fp = json.load(open(infile))

    # make individual node images
    for node in nodes:
        if node['type'] in ('random',):
            continue
        plot_node_timing(node, all_nodes)
        name = oname(node)
        fn = name + "." + figext
        if dirname:
            fn = dirname + "/" + fn
        plt.savefig(fn)
        print(fn)

        nl = f'{name}[image="{fn}", label=""]'
        node_lines.append(nl)

    edge_lines = list()
    for edge in fp["edges"]:
        tail = tnname(edge['tail']['node'])
        head = tnname(edge['head']['node'])
        # fixme: could indicate ports...
        el = f'{tail} -> {head}'
        edge_lines.append(el)

    lines = ["digraph base {"] + node_lines + edge_lines + ["}"]
    dtext = '\n'.join(lines)

    if outfile.endswith(".dot"):
        open(outfile, "w").write(dtext)
        return

    dotcmd = f'dot -T {ext[1:]} -o {outfile}'
    proc = subprocess.Popen(dotcmd, shell=True, stdin = subprocess.PIPE)
    proc.communicate(input=dtext.encode("utf-8"))

@cli.command("ls")
@click.argument('infile')
def cmd_ls(infile):
    for node in json.load(open(infile))["nodes"]:
        if node['type'] in ('random',):
            continue
        tn = node['type'] + ":" + node['name']
        d = node['data']
        r = 'R:{Rn} {Rmu:.3f}+/-{Rrms:.3f}'.format(**d)
        s = 'S:{Sn} {Smu:.3f}+/-{Srms:.3f}'.format(**d)
        print(f'{tn:16}{r:32}{s:32}')


def main():
    cli(obj=None)


if '__main__' == __name__:
    main()

