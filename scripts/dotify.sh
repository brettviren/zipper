#!/bin/bash
wp=/home/bv/wrk/wct/point-cloud/.direnv/python-3.9.10/bin/wirecell-pgraph

#    --graph-options rankdir=TB \

$wp dotify \
    --npath nodes --epath edges \
    $*

