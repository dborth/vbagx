/****************************************************************************
 * Visual Boy Advance GX
 * Daryl Borth 2026
 * mappingui.h
 *
 * The button mapping screen. One screen replaces the two that came before it
 * (the hardware, the list of buttons): a chip in the header chooses the hardware
 * being mapped, the rows list what each Game Boy Advance button uses, and a drawing
 * of the hardware shows the control the highlighted row is mapped to.
 ***************************************************************************/

#pragma once

/**
 * Runs the mapping screen on the menu's main window until the user leaves it.
 * Returns the menu to go to next (a MENU_* value from menu.h).
 */
int MappingScreen();
