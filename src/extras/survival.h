/*
 * Survival mode (survival.c): its hooks in translated functions that New
 * Game+ already wraps. A function has one wrap, so those wraps, in
 * new_game_plus.c, call these.
 */
#ifndef AE_EXTRAS_SURVIVAL_H
#define AE_EXTRAS_SURVIVAL_H

#include <stdint.h>

void survival_menu_enter(void);         /* sub_000B5CA0: the main menu is entered */
int survival_menu_pressed(void);        /* sub_000B5750, before the menu's own input */
void survival_start(void);              /* ... after it, when that returned 1 */
void survival_stop(void);               /* Start Game, or a save loaded */
void survival_area_loaded(uint32_t name);   /* sub_001419F0: name = the area */
void survival_frame(void);              /* sub_000E6760: every frame */

#endif
