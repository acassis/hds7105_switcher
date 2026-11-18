/*
 * testhook.h - scripted UI input for automated tests (--script FILE).
 *
 * Script lines:
 *   wait N          idle N frames
 *   click X Y       press and release the left button at window X,Y
 *   drag X Y X2 Y2  press at X,Y, move to X2,Y2 in steps, release
 *   shot FILE       save a PNG of the window
 *   quit            leave the program
 * Mouse events are injected with raylib's PlayAutomationEvent(), so the
 * real pointer is never touched and the window can stay hidden.
 */

#ifndef SWITCHER_TESTHOOK_H
#define SWITCHER_TESTHOOK_H

#include <stdbool.h>

int  testhook_load(const char *path);
void testhook_step(void);        /* before drawing: inject input       */
void testhook_after_draw(void);  /* after drawing: screenshots         */
bool testhook_done(void);

#endif
