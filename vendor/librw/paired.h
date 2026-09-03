// Macros to use the Espresso's Paired Singles in C
// based on https://github.com/dborth/wiimc/blob/master/source/mplayer/ffmpeg/libavutil/ppc/paired.h
// modified in 2021 by GaryOderNichts

#pragma once

enum {
    GQR_TYPE_FLOAT = 0,
    GQR_TYPE_U8 = 4,
    GQR_TYPE_U16,
    GQR_TYPE_S8,
    GQR_TYPE_S16,
};

enum {
    UGQR0 = 896, // The Espresso provides user accessible GQRs at 896-903
    UGQR1,
    UGQR2,
    UGQR3,
    UGQR4,
    UGQR5,
    UGQR6,
    UGQR7,
};

#define GQR_SET_EX(r, ld_power, ld_type, st_power, st_type) ({ \
    uint32_t gqr; \
    gqr = (((st_power) << 8) | (st_type)) << 16; \
    gqr |= ((ld_power) << 8) | (ld_type); \
    asm volatile("mtspr %0, %1" : : "i"(r), "r"(gqr)); })

#define GQR_SET(r, value) ({ \
    uint32_t gqr = value; \
    asm volatile("mtspr %0, %1" : : "i"(r), "r"(gqr)); })

#define GQR_GET(r) ({ \
    uint32_t gqr; \
    asm volatile("mfspr %0, %1" : : "r"(gqr), "i"(r)); \
    gqr; })

#define psq_l(d, rA, W, I) ({ \
    float frD; \
    asm volatile("psq_l %0, %1(%2), %3, %4" \
                 : "=f"(frD) : "i"(d), "b"(rA), "i"(W), "i"(I) : "memory"); \
    frD; })

#define psq_lx(rA, rB, W, I) ({ \
    float frD; \
    asm volatile("psq_lx %0, %1, %2, %3, %4" \
                 : "=f"(frD) : "b"(rA), "r"(rB), "i"(W), "i"(I) : "memory"); \
    frD; })

#define psq_lu(d, rA, W, I) ({ \
    float frD; \
    asm volatile("psq_lu %0, %2(%1), %3, %4" \
                 : "=f"(frD), "+b"(rA) : "i"(d), "i"(W), "i"(I) : "memory"); \
    frD; })

#define psq_lux(rA, rB, W, I) ({ \
    float frD; \
    asm volatile("psq_lux %0, %1, %2, %3, %4" \
                 : "=f"(frD), "+b"(rA) : "r"(rB), "i"(W), "i"(I) : "memory"); \
    frD; })

#define psq_st(frD, d, rA, W, I) \
    asm volatile("psq_st %0, %1(%2), %3, %4" \
                 :: "f"(frD), "i"(d), "b"(rA), "i"(W), "i"(I) : "memory")

#define psq_stx(frD, rA, rB, W, I) \
    asm volatile("psq_stx %0, %1, %2, %3, %4" \
                 :: "f"(frD), "b"(rA), "r"(rB), "i"(W), "i"(I) : "memory")

#define psq_stu(frD, d, rA, W, I) \
    asm volatile("psq_stu %1, %2(%0), %3, %4" \
                 : "+b"(rA) : "f"(frD), "i"(d), "i"(W), "i"(I) : "memory")

#define psq_stux(frD, rA, rB, W, I) \
    asm volatile("psq_stux %1, %0, %2, %3, %4" \
                 : "+b"(rA) : "f"(frD), "r"(rB), "i"(W), "i"(I) : "memory")

#define ps_neg(frB) ({ \
    float frD; \
    asm("ps_neg %0, %1" : "=f"(frD) : "f"(frB)); \
    frD; })

#define ps_add(frA, frB) ({ \
    float frD; \
    asm("ps_add %0, %1, %2" : "=f"(frD) : "f"(frA), "f"(frB)); \
    frD; })

#define ps_sub(frA, frB) ({ \
    float frD; \
    asm("ps_sub %0, %1, %2" : "=f"(frD) : "f"(frA), "f"(frB)); \
    frD; })

#define ps_mul(frA, frC) ({ \
    float frD; \
    asm("ps_mul %0, %1, %2" : "=f"(frD) : "f"(frA), "f"(frC)); \
    frD; })

#define ps_madd(frA, frC, frB) ({ \
    float frD; \
    asm("ps_madd %0, %1, %2, %3" : "=f"(frD) : "f"(frA), "f"(frC), "f"(frB)); \
    frD; })

#define ps_madds0(frA, frC, frB) ({ \
    float frD; \
    asm("ps_madds0 %0, %1, %2, %3" : "=f"(frD) : "f"(frA), "f"(frC), "f"(frB)); \
    frD; })

#define ps_madds1(frA, frC, frB) ({ \
    float frD; \
    asm("ps_madds1 %0,%1,%2,%3" : "=f"(frD) : "f"(frA), "f"(frC), "f"(frB)); \
    frD; })

#define ps_msub(frA, frC, frB) ({ \
    float frD; \
    asm("ps_msub %0, %1, %2, %3" : "=f"(frD) : "f"(frA), "f"(frC), "f"(frB)); \
    frD; })

#define ps_muls0(frA, frC) ({ \
    float frD; \
    asm("ps_muls0 %0, %1, %2" : "=f"(frD) : "f"(frA), "f"(frC)); \
    frD; })

#define ps_muls1(frA, frC) ({ \
    float frD; \
    asm("ps_muls1 %0, %1, %2" : "=f"(frD) : "f"(frA), "f"(frC)); \
    frD; })

#define ps_nmadd(frA, frC, frB) ({ \
    float frD; \
    asm("ps_nmadd %0, %1, %2, %3" : "=f"(frD) : "f"(frA), "f"(frC), "f"(frB)); \
    frD; })

#define ps_nmsub(frA, frC, frB) ({ \
    float frD; \
    asm("ps_nmsub %0, %1, %2, %3" : "=f"(frD) : "f"(frA), "f"(frC), "f"(frB)); \
    frD; })

#define ps_merge00(frA, frB) ({ \
    float frD; \
    asm("ps_merge00 %0, %1, %2" : "=f"(frD) : "f"(frA), "f"(frB)); \
    frD; })

#define ps_merge01(frA, frB) ({ \
    float frD; \
    asm("ps_merge01 %0, %1, %2" : "=f"(frD) : "f"(frA), "f"(frB)); \
    frD; })

#define ps_merge10(frA, frB) ({ \
    float frD; \
    asm("ps_merge10 %0, %1, %2" : "=f"(frD) : "f"(frA), "f"(frB)); \
    frD; })

#define ps_merge11(frA, frB) ({ \
    float frD; \
    asm("ps_merge11 %0, %1, %2" : "=f"(frD) : "f"(frA), "f"(frB)); \
    frD; })

#define ps_sel(frA, frC, frB) ({ \
    float frD; \
    asm("ps_sel %0, %1, %2, %3" : "=f"(frD) : "f"(frA), "f"(frC), "f"(frB)); \
    frD; })

#define ps_sum0(frA, frC, frB) ({ \
    float frD; \
    asm("ps_sum0 %0, %1, %2, %3" : "=f"(frD) : "f"(frA), "f"(frC), "f"(frB)); \
    frD; })

#define ps_sum1(frA, frC, frB) ({ \
    float frD; \
    asm("ps_sum1 %0, %1, %2, %3" : "=f"(frD) : "f"(frA), "f"(frC), "f"(frB)); \
    frD; })
