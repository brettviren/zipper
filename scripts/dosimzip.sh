#!/bin/bash

scenario=${1:-sourcesink}
outdir=build/out/${scenario}

if [ -d $outdir ] ; then
   rm -rf $outdir
fi
mkdir -p $outdir

set -e -u -x -o pipefail

jsonnet -e "local l=import \"simzip/layers.jsonnet\"; l(\"${scenario}\") + {main:{run_time:0.1}}" > ${outdir}/in.json
./dotify.sh ${outdir}/in.json ${outdir}/in.pdf 
time ./build/simzip ${outdir}/in.json > ${outdir}/out.json 2> ${outdir}/out.log
./dotify.sh ${outdir}/out.json ${outdir}/out.pdf 
zipit  graph-plots -o ${outdir}/graph_plots.pdf -f svg ${outdir}/out.json



