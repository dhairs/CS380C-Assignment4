#/bin/sh
INPUT=test.ll
OUTPUT=output.txt

opt -load-pass-plugin build/libloop-analysis-pass.so -passes="DG43932-PD9592-loop-analysis-pass" < $INPUT >/dev/null 2> $OUTPUT
