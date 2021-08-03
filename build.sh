#!/bin/bash

#source /opt/intel/oneapi/setvars.sh
icpc -fopenmp -fPIC -shared -o FEMbeCmm.o *.cpp *.h -I/home/pfaller/work/repos/FEBio_master -L/home/pfaller/work/repos/FEBio_master/build/lib -lfebiomech_lnx64 -lfecore_lnx64
