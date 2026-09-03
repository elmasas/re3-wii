; $MODE = "UniformRegister"

; $SPI_VS_OUT_CONFIG.VS_EXPORT_COUNT = 2
; $NUM_SPI_VS_OUT_ID = 1
; color
; $SPI_VS_OUT_ID[0].SEMANTIC_0 = 0
; tex coords
; $SPI_VS_OUT_ID[0].SEMANTIC_1 = 1
; fog
; $SPI_VS_OUT_ID[0].SEMANTIC_2 = 2

; C0
; $UNIFORM_VARS[0].name= "u_xform"
; $UNIFORM_VARS[0].type= "vec4"
; $UNIFORM_VARS[0].count = 1
; $UNIFORM_VARS[0].block = -1
; $UNIFORM_VARS[0].offset = 0
; C1
; $UNIFORM_VARS[1].name= "u_fogData"
; $UNIFORM_VARS[1].type= "vec4"
; $UNIFORM_VARS[1].count = 1
; $UNIFORM_VARS[1].block = -1
; $UNIFORM_VARS[1].offset = 4

; R1
; $ATTRIB_VARS[0].name = "in_pos"
; $ATTRIB_VARS[0].type = "vec4"
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
01 ALU: ADDR(32) CNT(12)
    0  x: MUL ____, R1.x,  C0.x
       y: MUL ____, R1.y,  C0.y
    1  x: ADD R1.x, PV0.x, C0.z
       y: ADD R1.y, PV0.y, C0.w
    2  x: MUL R1.x, R1.x,  R1.w
       y: MUL R1.y, R1.y,  R1.w
       z: MUL R1.z, R1.z,  R1.w
    3  x: ADD ____, R1.w, -C1.y
    4  x: MUL ____, PV0.x, C1.z
    5  x: MAX ____, PV0.x, C1.w
    6  x: MIN R0.x, PV0.x, 1.0f
02 EXP_DONE: POS0, R1
03 EXP: PARAM0, R2 NO_BARRIER 
04 EXP: PARAM1, R3 NO_BARRIER 
05 EXP_DONE: PARAM2, R0 NO_BARRIER
END_OF_PROGRAM
