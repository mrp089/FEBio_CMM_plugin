#!/bin/bash

#source /opt/intel/oneapi/setvars.sh
#g++ -fopenmp -fPIC -shared -o FEMbeCmm.o FEMbeCmm.cpp FEMbeCmm.h -I/home/pfaller/work/marcos/FEBio2.8.2/sdk/include/
g++ -fopenmp -fPIC -shared -o FEMbeCmm.o *.cpp *.h -I/home/pfaller/work/marcos/FEBio3 -L/home/pfaller/febio-2.8.0/sdk/lib/ -lfebiomech_gcc64 -lfecore_gcc64
#icpc -qopenmp -fPIC -shared -o FEMbeCmm.o FEMbeCmm.cpp FEMbeCmm.h -I/home/pfaller/work/marcos/FEBio3
