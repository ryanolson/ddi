#!/bin/csh
set ONESIDED=libonesided
cc -I../include -I../src -o t.x t.c -L../ -lddi 
