#/bin/sh
INPUT=looptest1.ll
OUTPUT=output.ll

make -C ./build

opt -load-pass-plugin ./build/libloop-opt-pass.so -passes="DG43932-PD9592-loop-opt-pass" -S <$INPUT -o $OUTPUT