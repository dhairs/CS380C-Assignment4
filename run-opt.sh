#/bin/sh
INPUT=simple_loop_hoist.ll
OUTPUT=output.txt

opt -load-pass-plugin ./build/libloop-opt-pass.so -passes="DG43932-PD9592-loop-opt-pass" <$INPUT >/dev/null 2>$OUTPUT