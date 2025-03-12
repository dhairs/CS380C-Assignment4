#/bin/sh
INPUT=test3.ll
OUTPUT=output.ll

make -C ./build

opt -load-pass-plugin ./build/libloop-opt-pass.dylib -passes="DG43932-PD9592-loop-opt-pass" -S <$INPUT -o $OUTPUT