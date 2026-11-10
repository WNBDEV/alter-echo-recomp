/*
 * The game's C library routines that cost the most when translated, written
 * natively. Each one does what the original does to the registers and the
 * x87 stack the generated code works on; the translator skips the original
 * (tools/setup_game.py's MANUAL_WRAPS lists this file).
 *
 * _ftol2 (sub_00151D3C), the compiler's float-to-integer conversion: every
 * `(int)f` in the game calls it, and it was about 8% of the game thread in a
 * survival fight. Translated, it is an x87 FIST under the guest rounding mode
 * (floor() and friends, not inlined) followed by a correction toward zero,
 * all on the emulated stack.
 */
#include <xbox/xboxrecomp.h>
#include <stdint.h>

extern RECOMP_TLS uint32_t g_eax, g_edx, g_esp;
extern RECOMP_TLS double g_fp_stack[8];
extern RECOMP_TLS int g_fp_top;

/* _ftol2: ST(0) truncated toward zero, as a 64-bit integer in EDX:EAX, and
 * popped. The original rounds with FISTP and then steps back toward zero by
 * the sign of what was left over, so the rounding mode never shows; a NaN or
 * a value outside the 64-bit range gives the integer indefinite
 * (0x8000000000000000), as FISTP does. */
void sub_00151D3C(void)
{
    double v = g_fp_stack[g_fp_top];
    int64_t r;

    g_fp_top = (g_fp_top + 1) & 7;
    if (v != v || v >= 9223372036854775808.0 || v < -9223372036854775808.0)
        r = INT64_MIN;
    else
        r = (int64_t)v;
    g_eax = (uint32_t)r;
    g_edx = (uint32_t)((uint64_t)r >> 32);
    g_esp += 4;                         /* ret */
}
