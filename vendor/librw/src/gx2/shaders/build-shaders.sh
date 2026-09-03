#!/bin/bash

# to build shaders you need to place a copy of latte-assembler into the current directory
# latte-assembler is part of decaf-emu <https://github.com/decaf-emu/decaf-emu>

# default
./latte-assembler assemble --vsh=shader_source/default.vsh --psh=shader_source/simple.psh default.gsh
xxd -i default.gsh > default.h

# im2d
./latte-assembler assemble --vsh=shader_source/im2d.vsh --psh=shader_source/simple.psh im2d.gsh
xxd -i im2d.gsh > im2d.h

# im3d
./latte-assembler assemble --vsh=shader_source/im3d.vsh --psh=shader_source/simple.psh im3d.gsh
xxd -i im3d.gsh > im3d.h

# matfx_env
./latte-assembler assemble --vsh=shader_source/matfx_env.vsh --psh=shader_source/matfx_env.psh matfx_env.gsh
xxd -i matfx_env.gsh > matfx_env.h

# skin
./latte-assembler assemble --vsh=shader_source/skin.vsh --psh=shader_source/simple.psh skin.gsh
xxd -i skin.gsh > skin.h
