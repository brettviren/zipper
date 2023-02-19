#!/bin/bash

tstdir="$(dirname $(realpath $BASH_SOURCE))"

nsec=10000

$tstdir/dosimzip.sh ticktock     $nsec
$tstdir/dosimzip.sh ticktockbuff $nsec
