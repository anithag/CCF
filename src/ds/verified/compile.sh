#!/bin/bash

if [[ -z "${KREMLIN_HOME}" ]]; then
    echo "Please set KREMLIN_HOME"
    exit 1
fi

# just extraction
#krml -tmpdir extracted -verify -skip-compilation -bundle WasmSupport  -bundle Ring+Reader+Writer+Misc= Ring.fst Reader.fst Writer.fst Misc.fst

# change directory
#cd extracted

#compile the extracted files
gcc-8 -I ${KREMLIN_HOME}/kremlib/dist/minimal -I  ${KREMLIN_HOME}/kremlib -I  ${KREMLIN_HOME}/include -I extracted -Wall -Werror -Wno-unused-variable -Wno-unknown-warning-option -Wno-unused-but-set-variable -g -fwrapv -D_BSD_SOURCE -D_DEFAULT_SOURCE -Wno-parentheses -std=c11 Ring_Reader_Writer_Misc.c  -c

# create an archive
ar rcs libvering.a Ring_Reader_writer_Misc.o 

#come back
#cd ..
