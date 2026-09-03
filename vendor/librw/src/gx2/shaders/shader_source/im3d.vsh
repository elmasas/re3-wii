; $MODE = "UniformRegister"

; $SPI_VS_OUT_CONFIG.VS_EXPORT_COUNT = 2
; $NUM_SPI_VS_OUT_ID = 1
; color
; $SPI_VS_OUT_ID[0].SEMANTIC_0 = 0
; tex_coords
; $SPI_VS_OUT_ID[0].SEMANTIC_1 = 1
; fog
; $SPI_VS_OUT_ID[0].SEMANTIC_2 = 2

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
; $UNIFORM_VARS[3].name = "u_fogData"
; $UNIFORM_VARS[3].type = "vec4"
; $UNIFORM_VARS[3].count = 1
; $UNIFORM_VARS[3].block = -1
; $UNIFORM_VARS[3].offset = 48

; R1
; $ATTRIB_VARS[0].name = "in_pos"
; $ATTRIB_VARS[0].type = "vec3"
; $ATTRIB_VARS[0].location = 0
; R2
; $ATTRIB_VARS[1].name = "in_color"
; $ATTRIB_VARS[1].type = "vec4"
; $ATTRIB_VARS[1].location = 1
; R3
; $ATTRIB_VARS[2].name = "in_tex0"
; $ATTRIB_VARS[2].type = "vec2"
; $ATTRIB_VARS[2].location = 2

00 CALL_FS NO_BARRIER 
01 ALU: ADDR(32) CNT(55) 
    0  x: MUL    ____,   1.0f, C11.x
       y: MUL    ____,   1.0f, C11.y
       z: MUL    ____,   1.0f, C11.z
       w: MUL    ____,   1.0f, C11.w
    1  x: MULADD R127.x, R1.z, C10.x, PV0.x
       y: MULADD R127.y, R1.z, C10.y, PV0.y
       z: MULADD R127.z, R1.z, C10.z, PV0.z
       w: MULADD R127.w, R1.z, C10.w, PV0.w
    2  x: MULADD R127.x, R1.y, C9.x,  PV0.x
       y: MULADD R127.y, R1.y, C9.y,  PV0.y
       z: MULADD R127.z, R1.y, C9.z,  PV0.z
       w: MULADD R127.w, R1.y, C9.w,  PV0.w
    3  x: MULADD R1.x,   R1.x, C8.x,  PV0.x
       y: MULADD R1.y,   R1.x, C8.y,  PV0.y
       z: MULADD R1.z,   R1.x, C8.z,  PV0.z
       w: MULADD R1.w,   R1.x, C8.w,  PV0.w
    4  x: MUL    ____,   R1.w, C7.x
       y: MUL    ____,   R1.w, C7.y
       z: MUL    ____,   R1.w, C7.z
       w: MUL    ____,   R1.w, C7.w
    5  x: MULADD R127.x, R1.z, C6.x,  PV0.x
       y: MULADD R127.y, R1.z, C6.y,  PV0.y
       z: MULADD R127.z, R1.z, C6.z,  PV0.z
       w: MULADD R127.w, R1.z, C6.w,  PV0.w
    6  x: MULADD R127.x, R1.y, C5.x,  PV0.x
       y: MULADD R127.y, R1.y, C5.y,  PV0.y
       z: MULADD R127.z, R1.y, C5.z,  PV0.z
       w: MULADD R127.w, R1.y, C5.w,  PV0.w
    7  x: MULADD R1.x,   R1.x, C4.x,  PV0.x
       y: MULADD R1.y,   R1.x, C4.y,  PV0.y
       z: MULADD R1.z,   R1.x, C4.z,  PV0.z
       w: MULADD R1.w,   R1.x, C4.w,  PV0.w
    8  x: MUL    ____,   R1.w, C3.x
       y: MUL    ____,   R1.w, C3.y
       z: MUL    ____,   R1.w, C3.z
       w: MUL    ____,   R1.w, C3.w
    9  x: MULADD R127.x, R1.z, C2.x,  PV0.x
       y: MULADD R127.y, R1.z, C2.y,  PV0.y
       z: MULADD R127.z, R1.z, C2.z,  PV0.z
       w: MULADD R127.w, R1.z, C2.w,  PV0.w
    10 x: MULADD R127.x, R1.y, C1.x,  PV0.x
       y: MULADD R127.y, R1.y, C1.y,  PV0.y
       z: MULADD R127.z, R1.y, C1.z,  PV0.z
       w: MULADD R127.w, R1.y, C1.w,  PV0.w
    11 x: MULADD R1.x,   R1.x, C0.x,  PV0.x
       y: MULADD R1.y,   R1.x, C0.y,  PV0.y
       z: MULADD R1.z,   R1.x, C0.z,  PV0.z
       w: MULADD R1.w,   R1.x, C0.w,  PV0.w
    12 x: ADD    ____,   R1.w, -C12.y
    13 x: MUL    ____,   PV0.x, C12.z
    14 x: MAX    ____,   PV0.x, C12.w
    15 x: MIN    R0.x,   PV0.x, 1.0f
02 EXP_DONE: POS0, R1
03 EXP: PARAM0, R2 NO_BARRIER
04 EXP: PARAM1, R3 NO_BARRIER
05 EXP_DONE: PARAM2, R0 NO_BARRIER 
END_OF_PROGRAM
