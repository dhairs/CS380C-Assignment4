#/bin/sh
INPUT=test3.ll
OUTPUT=output.txt

opt -load-pass-plugin build/libloop-analysis-pass.dylib -passes="DG43932-PD9592-loop-analysis-pass" < $INPUT >/dev/null 2> $OUTPUT
