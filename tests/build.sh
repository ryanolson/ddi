#!/bin/csh
set GA_CONFIG = "$DDI_GA_CONFIG_PATH/ga-config"
set LDFLAGS = `$GA_CONFIG --ldflags`
set LIBS = `$GA_CONFIG --libs`
cc -I../include -I../src $LDFLAGS -o t.x t.c -L../ -lddi $LIBS
