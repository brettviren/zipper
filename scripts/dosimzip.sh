#!/bin/bash

scenario=${1:-aparad}
outdir=build/out/${scenario}

dotify () {
    wirecell-pgraph dotify --npath nodes --epath edges $*
}

if [ -d $outdir ] ; then
   rm -rf $outdir
fi
mkdir -p $outdir

set -e -u -x -o pipefail

jsonnet -e "local l=import \"simzip/layers.jsonnet\"; l(\"${scenario}\") + {main:{run_time:0.1}}" > ${outdir}/in.json
dotify ${outdir}/in.json ${outdir}/in.pdf 
time ./build/simzip ${outdir}/in.json > ${outdir}/out.json 2> ${outdir}/out.log
dotify ${outdir}/out.json ${outdir}/out.pdf 
zipit  graph-plots -o ${outdir}/graph_plots.pdf -f svg ${outdir}/out.json



