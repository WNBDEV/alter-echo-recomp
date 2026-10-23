/*
 * Alter Echo's time dilation picks its targets with the D-pad alone -- the
 * left stick does nothing there -- so on the keyboard only the arrow keys
 * worked while the left hand sat on WASD. While it runs, the movement keys
 * press the D-pad too. Only then: in a fight the D-pad picks the target
 * ("Use the D-pad to target specific enemies"), and walking would keep
 * changing it.
 *
 * The time dilation manager is the object at 0x5416B0 (every call into it
 * passes that as this). Its state, at +0x9C, is 5 in play (0 before the
 * first time), and goes 0, 1, 2 -- targets are picked in 2 -- then 3, 4 and
 * back to 5 when time dilation ends. The game lets it start only from 0 or 5
 * (sub_0012B4E0), so anything else is time dilation running. Read off guest
 * memory in L09_A5: right stick click, D-pad, B.
 */
#include <xbox/xboxrecomp.h>
#include <stdint.h>

#define TD_STATE 0x54174Cu      /* 0x5416B0 + 0x9C */

static int time_dilation_running(void)
{
    uint32_t s = *(volatile uint32_t *)((uintptr_t)TD_STATE
                                        + (uintptr_t)xbox_GetMemoryOffset());
    return s != 0 && s != 5;
}

void title_input_init(void)
{
    xbox_InputSetMoveKeysAsDpad(time_dilation_running);
}
