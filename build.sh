#!/bin/bash

g++ -fPIC -shared FEMbeCmm.cpp dllmain.cpp -o FEMbeCmm.o -std=c++11 -I../FEBio/ -L../FEBio/build/lib -lfebiomech -lfecore
