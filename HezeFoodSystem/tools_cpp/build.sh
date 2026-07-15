#!/bin/bash
g++ -std=c++17 -O2 pipeline.cpp -o pipeline
if [ $? -eq 0 ]; then
    echo "Build successful: pipeline"
else
    echo "Build FAILED!"
    exit 1
fi
