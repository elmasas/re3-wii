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
; C40
; $UNIFORM_VARS[10].name = "u_boneMatrices"
; $UNIFORM_VARS[10].type = "mat4"
; $UNIFORM_VARS[10].count = 64
; $UNIFORM_VARS[10].block = -1
; $UNIFORM_VARS[10].offset = 160

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
; R5
; $ATTRIB_VARS[4].name = "in_weights"
; $ATTRIB_VARS[4].type = "vec4"
; $ATTRIB_VARS[4].location = 4
; R6
; $ATTRIB_VARS[5].name = "in_indices"
; $ATTRIB_VARS[5].type = "vec4"
; $ATTRIB_VARS[5].location = 5

00 CALL_FS NO_BARRIER
01 ALU: ADDR(32) CNT(68)
    0  t: FLT_TO_INT ____,   R6.x
    1  x: LSHL_INT   R6.x,   PS0,   0x2
    2  t: INT_TO_FLT R6.x,   R6.x
    3  x: MOVA_FLOOR ____,   PS0
    4  t: FLT_TO_INT ____,   R6.y
    5  x: LSHL_INT   R6.y,   PS0,   0x2
    6  t: INT_TO_FLT R6.y,   R6.y
    7  y: MOVA_FLOOR ____,   PS0
    8  x: MUL        ____,   1.0f,  C43[AR.x].x
       y: MUL        ____,   1.0f,  C43[AR.x].y
       z: MUL        ____,   1.0f,  C43[AR.x].z
    9  x: MULADD     R127.x, R1.z,  C42[AR.x].x, PV0.x
       y: MULADD     R127.y, R1.z,  C42[AR.x].y, PV0.y
       z: MULADD     R127.z, R1.z,  C42[AR.x].z, PV0.z
    10 x: MULADD     R127.x, R1.y,  C41[AR.x].x, PV0.x
       y: MULADD     R127.y, R1.y,  C41[AR.x].y, PV0.y
       z: MULADD     R127.z, R1.y,  C41[AR.x].z, PV0.z
    11 x: MULADD     R127.x, R1.x,  C40[AR.x].x, PV0.x
       y: MULADD     R127.y, R1.x,  C40[AR.x].y, PV0.y
       z: MULADD     R127.z, R1.x,  C40[AR.x].z, PV0.z
    12 x: MUL        R12.x,  PV0.x, R5.x
       y: MUL        R12.y,  PV0.y, R5.x
       z: MUL        R12.z,  PV0.z, R5.x
    13 x: MUL        ____,   1.0f,  C43[AR.y].x
       y: MUL        ____,   1.0f,  C43[AR.y].y
       z: MUL        ____,   1.0f,  C43[AR.y].z
    14 x: MULADD     R127.x, R1.z,  C42[AR.y].x, PV0.x
       y: MULADD     R127.y, R1.z,  C42[AR.y].y, PV0.y
       z: MULADD     R127.z, R1.z,  C42[AR.y].z, PV0.z
    15 x: MULADD     R127.x, R1.y,  C41[AR.y].x, PV0.x
       y: MULADD     R127.y, R1.y,  C41[AR.y].y, PV0.y
       z: MULADD     R127.z, R1.y,  C41[AR.y].z, PV0.z
    16 x: MULADD     R127.x, R1.x,  C40[AR.y].x, PV0.x
       y: MULADD     R127.y, R1.x,  C40[AR.y].y, PV0.y
       z: MULADD     R127.z, R1.x,  C40[AR.y].z, PV0.z
    17 x: MULADD     R12.x,  PV0.x, R5.y,        R12.x
       y: MULADD     R12.y,  PV0.y, R5.y,        R12.y
       z: MULADD     R12.z,  PV0.z, R5.y,        R12.z
    18 x: MUL        ____,   R2.z,  C42[AR.x].x
       y: MUL        ____,   R2.z,  C42[AR.x].y
       z: MUL        ____,   R2.z,  C42[AR.x].z
    19 x: MULADD     R127.x, R2.y,  C41[AR.x].x, PV0.x
       y: MULADD     R127.y, R2.y,  C41[AR.x].y, PV0.y
       z: MULADD     R127.z, R2.y,  C41[AR.x].z, PV0.z
    20 x: MULADD     R127.x, R2.x,  C40[AR.x].x, PV0.x
       y: MULADD     R127.y, R2.x,  C40[AR.x].y, PV0.y
       z: MULADD     R127.z, R2.x,  C40[AR.x].z, PV0.z
    21 x: MUL        R11.x,  PV0.x, R5.x
       y: MUL        R11.y,  PV0.y, R5.x
       z: MUL        R11.z,  PV0.z, R5.x
    22 x: MUL        ____,   R2.z,  C42[AR.y].x
       y: MUL        ____,   R2.z,  C42[AR.y].y
       z: MUL        ____,   R2.z,  C42[AR.y].z
    23 x: MULADD     R127.x, R2.y,  C41[AR.y].x, PV0.x
       y: MULADD     R127.y, R2.y,  C41[AR.y].y, PV0.y
       z: MULADD     R127.z, R2.y,  C41[AR.y].z, PV0.z
    24 x: MULADD     R127.x, R2.x,  C40[AR.y].x, PV0.x
       y: MULADD     R127.y, R2.x,  C40[AR.y].y, PV0.y
       z: MULADD     R127.z, R2.x,  C40[AR.y].z, PV0.z
    25 x: MULADD     R11.x,  PV0.x, R5.y,        R11.x
       y: MULADD     R11.y,  PV0.y, R5.y,        R11.y
       z: MULADD     R11.z,  PV0.z, R5.y,        R11.z
02 ALU: ADDR(100) CNT(68)
    26 t: FLT_TO_INT ____,   R6.z
    27 x: LSHL_INT   R6.z,   PS0,   0x2
    28 t: INT_TO_FLT R6.z,   R6.z
    29 z: MOVA_FLOOR ____,   PS0
    30 t: FLT_TO_INT ____,   R6.w
    31 x: LSHL_INT   R6.w,   PS0,   0x2
    32 t: INT_TO_FLT R6.w,   R6.w
    33 w: MOVA_FLOOR ____,   PS0
    34 x: MUL        ____,   1.0f,  C43[AR.z].x
       y: MUL        ____,   1.0f,  C43[AR.z].y
       z: MUL        ____,   1.0f,  C43[AR.z].z
    35 x: MULADD     R127.x, R1.z,  C42[AR.z].x, PV0.x
       y: MULADD     R127.y, R1.z,  C42[AR.z].y, PV0.y
       z: MULADD     R127.z, R1.z,  C42[AR.z].z, PV0.z
    36 x: MULADD     R127.x, R1.y,  C41[AR.z].x, PV0.x
       y: MULADD     R127.y, R1.y,  C41[AR.z].y, PV0.y
       z: MULADD     R127.z, R1.y,  C41[AR.z].z, PV0.z
    37 x: MULADD     R127.x, R1.x,  C40[AR.z].x, PV0.x
       y: MULADD     R127.y, R1.x,  C40[AR.z].y, PV0.y
       z: MULADD     R127.z, R1.x,  C40[AR.z].z, PV0.z
    38 x: MULADD     R12.x,  PV0.x, R5.z,        R12.x
       y: MULADD     R12.y,  PV0.y, R5.z,        R12.y
       z: MULADD     R12.z,  PV0.z, R5.z,        R12.z
    39 x: MUL        ____,   1.0f,  C43[AR.w].x
       y: MUL        ____,   1.0f,  C43[AR.w].y
       z: MUL        ____,   1.0f,  C43[AR.w].z
    40 x: MULADD     R127.x, R1.z,  C42[AR.w].x, PV0.x
       y: MULADD     R127.y, R1.z,  C42[AR.w].y, PV0.y
       z: MULADD     R127.z, R1.z,  C42[AR.w].z, PV0.z
    41 x: MULADD     R127.x, R1.y,  C41[AR.w].x, PV0.x
       y: MULADD     R127.y, R1.y,  C41[AR.w].y, PV0.y
       z: MULADD     R127.z, R1.y,  C41[AR.w].z, PV0.z
    42 x: MULADD     R127.x, R1.x,  C40[AR.w].x, PV0.x
       y: MULADD     R127.y, R1.x,  C40[AR.w].y, PV0.y
       z: MULADD     R127.z, R1.x,  C40[AR.w].z, PV0.z
    43 x: MULADD     R1.x,   PV0.x, R5.w,        R12.x
       y: MULADD     R1.y,   PV0.y, R5.w,        R12.y
       z: MULADD     R1.z,   PV0.z, R5.w,        R12.z
    44 x: MUL        ____,   R2.z,  C42[AR.z].x
       y: MUL        ____,   R2.z,  C42[AR.z].y
       z: MUL        ____,   R2.z,  C42[AR.z].z
    45 x: MULADD     R127.x, R2.y,  C41[AR.z].x, PV0.x
       y: MULADD     R127.y, R2.y,  C41[AR.z].y, PV0.y
       z: MULADD     R127.z, R2.y,  C41[AR.z].z, PV0.z
    46 x: MULADD     R127.x, R2.x,  C40[AR.z].x, PV0.x
       y: MULADD     R127.y, R2.x,  C40[AR.z].y, PV0.y
       z: MULADD     R127.z, R2.x,  C40[AR.z].z, PV0.z
    47 x: MULADD     R11.x,  PV0.x, R5.z,        R11.x
       y: MULADD     R11.y,  PV0.y, R5.z,        R11.y
       z: MULADD     R11.z,  PV0.z, R5.z,        R11.z
    48 x: MUL        ____,   R2.z,  C42[AR.w].x
       y: MUL        ____,   R2.z,  C42[AR.w].y
       z: MUL        ____,   R2.z,  C42[AR.w].z
    49 x: MULADD     R127.x, R2.y,  C41[AR.w].x, PV0.x
       y: MULADD     R127.y, R2.y,  C41[AR.w].y, PV0.y
       z: MULADD     R127.z, R2.y,  C41[AR.w].z, PV0.z
    50 x: MULADD     R127.x, R2.x,  C40[AR.w].x, PV0.x
       y: MULADD     R127.y, R2.x,  C40[AR.w].y, PV0.y
       z: MULADD     R127.z, R2.x,  C40[AR.w].z, PV0.z
    51 x: MULADD     R2.x,   PV0.x, R5.w,        R11.x
       y: MULADD     R2.y,   PV0.y, R5.w,        R11.y
       z: MULADD     R2.z,   PV0.z, R5.w,        R11.z
03 ALU: ADDR(168) CNT(119)
    52 x: MUL    R0.x,   C4.w, C3.x
       y: MUL    R0.y,   C4.w, C3.y
    53 x: MUL    R0.z,   C4.w, C3.z
       y: MUL    R0.w,   C4.w, C3.w
    54 x: MULADD R0.x,   C4.z, C2.x,  R0.x
       y: MULADD R0.y,   C4.z, C2.y,  R0.y
    55 x: MULADD R0.z,   C4.z, C2.z,  R0.z
       y: MULADD R0.w,   C4.z, C2.w,  R0.w
    56 x: MULADD R0.x,   C4.y, C1.x,  R0.x
       y: MULADD R0.y,   C4.y, C1.y,  R0.y
    57 x: MULADD R0.z,   C4.y, C1.z,  R0.z
       y: MULADD R0.w,   C4.y, C1.w,  R0.w
    58 x: MULADD R7.x,   C4.x, C0.x,  R0.x
       y: MULADD R7.y,   C4.x, C0.y,  R0.y
    59 x: MULADD R7.z,   C4.x, C0.z,  R0.z
       y: MULADD R7.w,   C4.x, C0.w,  R0.w
    60 x: MUL    R0.x,   C5.w, C3.x
       y: MUL    R0.y,   C5.w, C3.y
    61 x: MUL    R0.z,   C5.w, C3.z
       y: MUL    R0.w,   C5.w, C3.w
    62 x: MULADD R0.x,   C5.z, C2.x,  R0.x
       y: MULADD R0.y,   C5.z, C2.y,  R0.y
    63 x: MULADD R0.z,   C5.z, C2.z,  R0.z
       y: MULADD R0.w,   C5.z, C2.w,  R0.w
    64 x: MULADD R0.x,   C5.y, C1.x,  R0.x
       y: MULADD R0.y,   C5.y, C1.y,  R0.y
    65 x: MULADD R0.z,   C5.y, C1.z,  R0.z
       y: MULADD R0.w,   C5.y, C1.w,  R0.w
    66 x: MULADD R8.x,   C5.x, C0.x,  R0.x
       y: MULADD R8.y,   C5.x, C0.y,  R0.y
    67 x: MULADD R8.z,   C5.x, C0.z,  R0.z
       y: MULADD R8.w,   C5.x, C0.w,  R0.w
    68 x: MUL    R0.x,   C6.w, C3.x
       y: MUL    R0.y,   C6.w, C3.y
    69 x: MUL    R0.z,   C6.w, C3.z
       y: MUL    R0.w,   C6.w, C3.w
    70 x: MULADD R0.x,   C6.z, C2.x,  R0.x
       y: MULADD R0.y,   C6.z, C2.y,  R0.y
    71 x: MULADD R0.z,   C6.z, C2.z,  R0.z
       y: MULADD R0.w,   C6.z, C2.w,  R0.w
    72 x: MULADD R0.x,   C6.y, C1.x,  R0.x
       y: MULADD R0.y,   C6.y, C1.y,  R0.y
    73 x: MULADD R0.z,   C6.y, C1.z,  R0.z
       y: MULADD R0.w,   C6.y, C1.w,  R0.w
    74 x: MULADD R9.x,   C6.x, C0.x,  R0.x
       y: MULADD R9.y,   C6.x, C0.y,  R0.y
    75 x: MULADD R9.z,   C6.x, C0.z,  R0.z
       y: MULADD R9.w,   C6.x, C0.w,  R0.w
    76 x: MUL    R0.x,   C7.w, C3.x
       y: MUL    R0.y,   C7.w, C3.y
    77 x: MUL    R0.z,   C7.w, C3.z
       y: MUL    R0.w,   C7.w, C3.w
    78 x: MULADD R0.x,   C7.z, C2.x,  R0.x
       y: MULADD R0.y,   C7.z, C2.y,  R0.y
    79 x: MULADD R0.z,   C7.z, C2.z,  R0.z
       y: MULADD R0.w,   C7.z, C2.w,  R0.w
    80 x: MULADD R0.x,   C7.y, C1.x,  R0.x
       y: MULADD R0.y,   C7.y, C1.y,  R0.y
    81 x: MULADD R0.z,   C7.y, C1.z,  R0.z
       y: MULADD R0.w,   C7.y, C1.w,  R0.w
    82 x: MULADD R10.x,  C7.x, C0.x,  R0.x
       y: MULADD R10.y,  C7.x, C0.y,  R0.y
    83 x: MULADD R10.z,  C7.x, C0.z,  R0.z
       y: MULADD R10.w,  C7.x, C0.w,  R0.w
    84 x: MUL    ____,   1.0f, C11.x
       y: MUL    ____,   1.0f, C11.y
       z: MUL    ____,   1.0f, C11.z
       w: MUL    ____,   1.0f, C11.w
    85 x: MULADD R127.x, R1.z, C10.x, PV0.x
       y: MULADD R127.y, R1.z, C10.y, PV0.y
       z: MULADD R127.z, R1.z, C10.z, PV0.z
       w: MULADD R127.w, R1.z, C10.w, PV0.w
    86 x: MULADD R127.x, R1.y, C9.x,  PV0.x
       y: MULADD R127.y, R1.y, C9.y,  PV0.y
       z: MULADD R127.z, R1.y, C9.z,  PV0.z
       w: MULADD R127.w, R1.y, C9.w,  PV0.w
    87 x: MULADD R6.x,   R1.x, C8.x,  PV0.x
       y: MULADD R6.y,   R1.x, C8.y,  PV0.y
       z: MULADD R6.z,   R1.x, C8.z,  PV0.z
       w: MULADD R6.w,   R1.x, C8.w,  PV0.w
    88 x: MUL    ____,   R6.x, R7.x
       y: MUL    ____,   R6.x, R7.y
       z: MUL    ____,   R6.x, R7.z
       w: MUL    ____,   R6.x, R7.w
    89 x: MULADD R127.x, R6.y, R8.x,  PV0.x
       y: MULADD R127.y, R6.y, R8.y,  PV0.y
       z: MULADD R127.z, R6.y, R8.z,  PV0.z
       w: MULADD R127.w, R6.y, R8.w,  PV0.w
    90 x: MULADD R127.x, R6.z, R9.x,  PV0.x
       y: MULADD R127.y, R6.z, R9.y,  PV0.y
       z: MULADD R127.z, R6.z, R9.z,  PV0.z
       w: MULADD R127.w, R6.z, R9.w,  PV0.w
    91 x: MULADD R11.x,  R6.w, R10.x, PV0.x
       y: MULADD R11.y,  R6.w, R10.y, PV0.y
       z: MULADD R11.z,  R6.w, R10.z, PV0.z
       w: MULADD R11.w,  R6.w, R10.w, PV0.w
    92 x: ADD            ____, R11.w, -C15.y
    93 x: MUL            ____, PV0.x, C15.z
    94 x: MAX            ____, PV0.x, C15.w
    95 x: MIN            R8.x, PV0.x, 1.0f
    96 x: MULADD         R3.x,  C12.x, C14.x, R3.x
       y: MULADD         R3.y,  C12.y, C14.x, R3.y
       z: MULADD         R3.z,  C12.z, C14.x, R3.z
    97 x: MUL    ____,   R2.z,  C10.x
       y: MUL    ____,   R2.z,  C10.y
       z: MUL    ____,   R2.z,  C10.z
    98 x: MULADD R127.x, R2.y,  C9.x,  PV0.x
       y: MULADD R127.y, R2.y,  C9.y,  PV0.y
       z: MULADD R127.z, R2.y,  C9.z,  PV0.z
    99 x: MULADD R6.x,   R2.x,  C8.x,  PV0.x
       y: MULADD R6.y,   R2.x,  C8.y,  PV0.y
       z: MULADD R6.z,   R2.x,  C8.z,  PV0.z
   100 x: MOV            R7.x,  0.0f
       y: MOV            R7.y,  0.0f
       z: MOV            R7.z,  0.0f
       w: MOV            R6.w,  0.0f
04 LOOP_START_DX10 FAIL_JUMP_ADDR(9)
    05 ALU_BREAK: ADDR(287) CNT(3)
       101 y: SETGT_INT       R0.y, 0x8, R6.w
       102 x: PRED_SETNE_INT  ____,  R0.y, 0.0f UPDATE_EXEC_MASK UPDATE_PRED
    06 ALU_BREAK: ADDR(290) CNT(4)
       103 t: INT_TO_FLT      R0.x, R6.w
       104 x: MOVA_FLOOR      ____, PS0
       105 w: SETE_DX10       R0.w, C16[AR.x].x, 0.0f
       106 x: PRED_SETE_INT   ____, R0.w, 0.0f UPDATE_EXEC_MASK UPDATE_PRED 
    07 ALU: ADDR(294) CNT(23)
       107 x: MOVA_FLOOR  ____, R0.x
           w: MOV         R0.w, 0.0f
           t: ADD_INT     R6.w, R6.w, 0x1
       108 x: MOV         ____, C24[AR.x].x
           y: MOV         ____, C24[AR.x].y
           z: MOV         ____, C24[AR.x].z
       109 x: DOT4        ____, R6.x, -PV0.x
           y: DOT4        ____, R6.y, -PV0.y
           z: DOT4        ____, R6.z, -PV0.z
           w: DOT4        ____, R6.z, -R0.w
           t: SETE_DX10   R7.w, C16[AR.x].x, 1.0f
       110 w: MAX         ____, PV0.x, 0.0f
       111 x: MUL         ____, PV0.w, C32[AR.x].x
           y: MUL         ____, PV0.w, C32[AR.x].y
           z: MUL         ____, PV0.w, C32[AR.x].z
       112 x: ADD         ____, PV0.x, R7.x
           y: ADD         ____, PV0.y, R7.y
           z: ADD         ____, PV0.z, R7.z
       113 x: CNDE_INT    R7.x, R7.w, R7.x, PV0.x
           y: CNDE_INT    R7.y, R7.w, R7.y, PV0.y
           z: CNDE_INT    R7.z, R7.w, R7.z, PV0.z
08 LOOP_END CF_CONST(0) PASS_JUMP_ADDR(5)
09 ALU: ADDR(317) CNT(7)
   114 x: MULADD    R3.x,   R7.x,  C14.z, R3.x CLAMP
       y: MULADD    R3.y,   R7.y,  C14.z, R3.y CLAMP
       z: MULADD    R3.z,   R7.z,  C14.z, R3.z CLAMP
   115 x: MUL       R3.x,   R3.x,  C13.x
       y: MUL       R3.y,   R3.y,  C13.y
       z: MUL       R3.z,   R3.z,  C13.z
       w: MUL       R3.w,   R3.w,  C13.w
10 EXP_DONE: POS0, R11
11 EXP: PARAM0, R3 NO_BARRIER
12 EXP: PARAM1, R4 NO_BARRIER
13 EXP_DONE: PARAM2, R8 NO_BARRIER
END_OF_PROGRAM
