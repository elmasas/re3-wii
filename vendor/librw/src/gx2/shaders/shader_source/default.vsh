; $MODE = "UniformRegister"

; $SPI_VS_OUT_CONFIG.VS_EXPORT_COUNT = 2
; $NUM_SPI_VS_OUT_ID = 1
; color
; $SPI_VS_OUT_ID[0].SEMANTIC_0 = 0
; tex
; $SPI_VS_OUT_ID[0].SEMANTIC_1 = 1
; fog
; $SPI_VS_OUT_ID[0].SEMANTIC_2 = 2

; $LOOP_VARS[0].offset = 0
; $LOOP_VARS[0].value = 0xFFFFFFFF

; C0
; $UNIFORM_VARS[0].name = "u_proj"
; $UNIFORM_VARS[0].type = "mat4"
; $UNIFORM_VARS[0].count = 1
; $UNIFORM_VARS[0].block = -1
; $UNIFORM_VARS[0].offset = 0
; C4
; $UNIFORM_VARS[1].name = "u_view"
; $UNIFORM_VARS[1].type = "mat4"
; $UNIFORM_VARS[1].count = 1
; $UNIFORM_VARS[1].block = -1
; $UNIFORM_VARS[1].offset = 16
; C8
; $UNIFORM_VARS[2].name = "u_world"
; $UNIFORM_VARS[2].type = "mat4"
; $UNIFORM_VARS[2].count = 1
; $UNIFORM_VARS[2].block = -1
; $UNIFORM_VARS[2].offset = 32
; C12
; $UNIFORM_VARS[3].name = "u_ambLight"
; $UNIFORM_VARS[3].type = "vec4"
; $UNIFORM_VARS[3].count = 1
; $UNIFORM_VARS[3].block = -1
; $UNIFORM_VARS[3].offset = 48
; C13
; $UNIFORM_VARS[4].name = "u_matColor"
; $UNIFORM_VARS[4].type = "vec4"
; $UNIFORM_VARS[4].count = 1
; $UNIFORM_VARS[4].block = -1
; $UNIFORM_VARS[4].offset = 52
; C14
; $UNIFORM_VARS[5].name = "u_surfProps"
; $UNIFORM_VARS[5].type = "vec4"
; $UNIFORM_VARS[5].count = 1
; $UNIFORM_VARS[5].block = -1
; $UNIFORM_VARS[5].offset = 56
; C15
; $UNIFORM_VARS[6].name = "u_fogData"
; $UNIFORM_VARS[6].type = "vec4"
; $UNIFORM_VARS[6].count = 1
; $UNIFORM_VARS[6].block = -1
; $UNIFORM_VARS[6].offset = 60
; C16
; $UNIFORM_VARS[7].name = "u_lightParams"
; $UNIFORM_VARS[7].type = "vec4"
; $UNIFORM_VARS[7].count = 8
; $UNIFORM_VARS[7].block = -1
; $UNIFORM_VARS[7].offset = 64
; C24
; $UNIFORM_VARS[8].name = "u_lightDirection"
; $UNIFORM_VARS[8].type = "vec4"
; $UNIFORM_VARS[8].count = 8
; $UNIFORM_VARS[8].block = -1
; $UNIFORM_VARS[8].offset = 96
; C32
; $UNIFORM_VARS[9].name = "u_lightColor"
; $UNIFORM_VARS[9].type = "vec4"
; $UNIFORM_VARS[9].count = 8
; $UNIFORM_VARS[9].block = -1
; $UNIFORM_VARS[9].offset = 128

; R1
; $ATTRIB_VARS[0].name = "in_pos"
; $ATTRIB_VARS[0].type = "vec3"
; $ATTRIB_VARS[0].location = 0
; R2
; $ATTRIB_VARS[1].name = "in_normal"
; $ATTRIB_VARS[1].type = "vec3"
; $ATTRIB_VARS[1].location = 1
; R3
; $ATTRIB_VARS[2].name = "in_color"
; $ATTRIB_VARS[2].type = "vec4"
; $ATTRIB_VARS[2].location = 2
; R4
; $ATTRIB_VARS[3].name = "in_tex0"
; $ATTRIB_VARS[3].type = "vec2"
; $ATTRIB_VARS[3].location = 3

00 CALL_FS NO_BARRIER
01 ALU: ADDR(32) CNT(103)
    0  x: MUL    R0.x,   C4.w, C3.x
       y: MUL    R0.y,   C4.w, C3.y
    1  x: MUL    R0.z,   C4.w, C3.z
       y: MUL    R0.w,   C4.w, C3.w
    2  x: MULADD R0.x,   C4.z, C2.x,  R0.x
       y: MULADD R0.y,   C4.z, C2.y,  R0.y
    3  x: MULADD R0.z,   C4.z, C2.z,  R0.z
       y: MULADD R0.w,   C4.z, C2.w,  R0.w
    4  x: MULADD R0.x,   C4.y, C1.x,  R0.x
       y: MULADD R0.y,   C4.y, C1.y,  R0.y
    5  x: MULADD R0.z,   C4.y, C1.z,  R0.z
       y: MULADD R0.w,   C4.y, C1.w,  R0.w
    6  x: MULADD R7.x,   C4.x, C0.x,  R0.x
       y: MULADD R7.y,   C4.x, C0.y,  R0.y
    7  x: MULADD R7.z,   C4.x, C0.z,  R0.z
       y: MULADD R7.w,   C4.x, C0.w,  R0.w
    8  x: MUL    R0.x,   C5.w, C3.x
       y: MUL    R0.y,   C5.w, C3.y
    9  x: MUL    R0.z,   C5.w, C3.z
       y: MUL    R0.w,   C5.w, C3.w
    10 x: MULADD R0.x,   C5.z, C2.x,  R0.x
       y: MULADD R0.y,   C5.z, C2.y,  R0.y
    11 x: MULADD R0.z,   C5.z, C2.z,  R0.z
       y: MULADD R0.w,   C5.z, C2.w,  R0.w
    12 x: MULADD R0.x,   C5.y, C1.x,  R0.x
       y: MULADD R0.y,   C5.y, C1.y,  R0.y
    13 x: MULADD R0.z,   C5.y, C1.z,  R0.z
       y: MULADD R0.w,   C5.y, C1.w,  R0.w
    14 x: MULADD R8.x,   C5.x, C0.x,  R0.x
       y: MULADD R8.y,   C5.x, C0.y,  R0.y
    15 x: MULADD R8.z,   C5.x, C0.z,  R0.z
       y: MULADD R8.w,   C5.x, C0.w,  R0.w
    16 x: MUL    R0.x,   C6.w, C3.x
       y: MUL    R0.y,   C6.w, C3.y
    17 x: MUL    R0.z,   C6.w, C3.z
       y: MUL    R0.w,   C6.w, C3.w
    18 x: MULADD R0.x,   C6.z, C2.x,  R0.x
       y: MULADD R0.y,   C6.z, C2.y,  R0.y
    19 x: MULADD R0.z,   C6.z, C2.z,  R0.z
       y: MULADD R0.w,   C6.z, C2.w,  R0.w
    20 x: MULADD R0.x,   C6.y, C1.x,  R0.x
       y: MULADD R0.y,   C6.y, C1.y,  R0.y
    21 x: MULADD R0.z,   C6.y, C1.z,  R0.z
       y: MULADD R0.w,   C6.y, C1.w,  R0.w
    22 x: MULADD R9.x,   C6.x, C0.x,  R0.x
       y: MULADD R9.y,   C6.x, C0.y,  R0.y
    23 x: MULADD R9.z,   C6.x, C0.z,  R0.z
       y: MULADD R9.w,   C6.x, C0.w,  R0.w
    24 x: MUL    R0.x,   C7.w, C3.x
       y: MUL    R0.y,   C7.w, C3.y
    25 x: MUL    R0.z,   C7.w, C3.z
       y: MUL    R0.w,   C7.w, C3.w
    26 x: MULADD R0.x,   C7.z, C2.x,  R0.x
       y: MULADD R0.y,   C7.z, C2.y,  R0.y
    27 x: MULADD R0.z,   C7.z, C2.z,  R0.z
       y: MULADD R0.w,   C7.z, C2.w,  R0.w
    28 x: MULADD R0.x,   C7.y, C1.x,  R0.x
       y: MULADD R0.y,   C7.y, C1.y,  R0.y
    29 x: MULADD R0.z,   C7.y, C1.z,  R0.z
       y: MULADD R0.w,   C7.y, C1.w,  R0.w
    30 x: MULADD R10.x,  C7.x, C0.x,  R0.x
       y: MULADD R10.y,  C7.x, C0.y,  R0.y
    31 x: MULADD R10.z,  C7.x, C0.z,  R0.z
       y: MULADD R10.w,  C7.x, C0.w,  R0.w
    32 x: MUL    ____,   1.0f, C11.x
       y: MUL    ____,   1.0f, C11.y
       z: MUL    ____,   1.0f, C11.z
       w: MUL    ____,   1.0f, C11.w
    33 x: MULADD R127.x, R1.z, C10.x, PV0.x
       y: MULADD R127.y, R1.z, C10.y, PV0.y
       z: MULADD R127.z, R1.z, C10.z, PV0.z
       w: MULADD R127.w, R1.z, C10.w, PV0.w
    34 x: MULADD R127.x, R1.y, C9.x,  PV0.x
       y: MULADD R127.y, R1.y, C9.y,  PV0.y
       z: MULADD R127.z, R1.y, C9.z,  PV0.z
       w: MULADD R127.w, R1.y, C9.w,  PV0.w
    35 x: MULADD R6.x,   R1.x, C8.x,  PV0.x
       y: MULADD R6.y,   R1.x, C8.y,  PV0.y
       z: MULADD R6.z,   R1.x, C8.z,  PV0.z
       w: MULADD R6.w,   R1.x, C8.w,  PV0.w
    36 x: MUL    ____,   R6.x, R7.x
       y: MUL    ____,   R6.x, R7.y
       z: MUL    ____,   R6.x, R7.z
       w: MUL    ____,   R6.x, R7.w
    37 x: MULADD R127.x, R6.y, R8.x,  PV0.x
       y: MULADD R127.y, R6.y, R8.y,  PV0.y
       z: MULADD R127.z, R6.y, R8.z,  PV0.z
       w: MULADD R127.w, R6.y, R8.w,  PV0.w
    38 x: MULADD R127.x, R6.z, R9.x,  PV0.x
       y: MULADD R127.y, R6.z, R9.y,  PV0.y
       z: MULADD R127.z, R6.z, R9.z,  PV0.z
       w: MULADD R127.w, R6.z, R9.w,  PV0.w
    39 x: MULADD R11.x,  R6.w, R10.x, PV0.x
       y: MULADD R11.y,  R6.w, R10.y, PV0.y
       z: MULADD R11.z,  R6.w, R10.z, PV0.z
       w: MULADD R11.w,  R6.w, R10.w, PV0.w
    40 x: ADD    ____,   R11.w, -C15.y
    41 x: MUL    ____,   PV0.x, C15.z
    42 x: MAX    ____,   PV0.x, C15.w
    43 x: MIN    R8.x,   PV0.x, 1.0f
02 ALU: ADDR(135) CNT(16)
    44 x: MULADD         R3.x,  C12.x, C14.x, R3.x
       y: MULADD         R3.y,  C12.y, C14.x, R3.y
       z: MULADD         R3.z,  C12.z, C14.x, R3.z
    45 x: MUL    ____,   R2.z,  C10.x
       y: MUL    ____,   R2.z,  C10.y
       z: MUL    ____,   R2.z,  C10.z
    46 x: MULADD R127.x, R2.y,  C9.x,  PV0.x
       y: MULADD R127.y, R2.y,  C9.y,  PV0.y
       z: MULADD R127.z, R2.y,  C9.z,  PV0.z
    47 x: MULADD R6.x,   R2.x,  C8.x,  PV0.x
       y: MULADD R6.y,   R2.x,  C8.y,  PV0.y
       z: MULADD R6.z,   R2.x,  C8.z,  PV0.z
    48 x: MOV            R7.x,  0.0f
       y: MOV            R7.y,  0.0f
       z: MOV            R7.z,  0.0f
       w: MOV            R6.w,  0.0f
03 LOOP_START_DX10 FAIL_JUMP_ADDR(8)
    04 ALU_BREAK: ADDR(151) CNT(3)
        49 y: SETGT_INT       R0.y, 0x8, R6.w
        50 x: PRED_SETNE_INT  ____,  R0.y, 0.0f UPDATE_EXEC_MASK UPDATE_PRED
    05 ALU_BREAK: ADDR(154) CNT(4)
        51 t: INT_TO_FLT      R0.x, R6.w
        52 x: MOVA_FLOOR      ____, PS0
        53 w: SETE_DX10       R0.w, C16[AR.x].x, 0.0f
        54 x: PRED_SETE_INT   ____, R0.w, 0.0f UPDATE_EXEC_MASK UPDATE_PRED 
    06 ALU: ADDR(158) CNT(23)
        55 x: MOVA_FLOOR  ____, R0.x
           w: MOV         R0.w, 0.0f
           t: ADD_INT     R6.w, R6.w, 0x1
        56 x: MOV         ____, C24[AR.x].x
           y: MOV         ____, C24[AR.x].y
           z: MOV         ____, C24[AR.x].z
        57 x: DOT4        ____, R6.x, -PV0.x
           y: DOT4        ____, R6.y, -PV0.y
           z: DOT4        ____, R6.z, -PV0.z
           w: DOT4        ____, R6.z, -R0.w
           t: SETE_DX10   R7.w, C16[AR.x].x, 1.0f
        58 w: MAX         ____, PV0.x, 0.0f
        59 x: MUL         ____, PV0.w, C32[AR.x].x
           y: MUL         ____, PV0.w, C32[AR.x].y
           z: MUL         ____, PV0.w, C32[AR.x].z
        60 x: ADD         ____, PV0.x, R7.x
           y: ADD         ____, PV0.y, R7.y
           z: ADD         ____, PV0.z, R7.z
        61 x: CNDE_INT    R7.x, R7.w, R7.x, PV0.x
           y: CNDE_INT    R7.y, R7.w, R7.y, PV0.y
           z: CNDE_INT    R7.z, R7.w, R7.z, PV0.z
07 LOOP_END CF_CONST(0) PASS_JUMP_ADDR(4)
08 ALU: ADDR(181) CNT(7)
    62 x: MULADD    R3.x,   R7.x,  C14.z, R3.x CLAMP
       y: MULADD    R3.y,   R7.y,  C14.z, R3.y CLAMP
       z: MULADD    R3.z,   R7.z,  C14.z, R3.z CLAMP
    63 x: MUL       R3.x,   R3.x,  C13.x
       y: MUL       R3.y,   R3.y,  C13.y
       z: MUL       R3.z,   R3.z,  C13.z
       w: MUL       R3.w,   R3.w,  C13.w
09 EXP_DONE: POS0, R11
10 EXP: PARAM0, R3 NO_BARRIER
11 EXP: PARAM1, R4 NO_BARRIER
12 EXP_DONE: PARAM2, R8 NO_BARRIER
END_OF_PROGRAM
