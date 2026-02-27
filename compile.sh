#!/bin/bash

gcc src/main.c src/parsePGM.c -o computeHistogram -pthread
#./computeHistogram ../Data/heart.pgm ../Data/histogram_heart.txt 4
#python3 ../showHistogram.py ../Data/histogram_heart.txt

#gcc P3_sequential.c parsePGM.c -o computeHistogramSequential
#./computeHistogramSequential ../Data/heart.pgm ../Data/histogram_heart.txt
#python3 ../showHistogram.py ../Data/histogram_heart.txt
