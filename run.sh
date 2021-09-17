#!/bin/bash

# set intel environment (FEBio and plugin compile with icpc)
source /opt/intel/oneapi/setvars.sh

# run simulation
~/work/repos/FEBio_master/build/bin/febio3.lnx64 -i TAA-axi-4x200x1L-ht.feb -nosplash  -import FEMbeCmm.o
