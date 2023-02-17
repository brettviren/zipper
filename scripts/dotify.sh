#!/bin/bash
wp=wirecell-pgraph

#    --graph-options rankdir=TB \

$wp dotify \
    --npath nodes --epath edges \
    $*

