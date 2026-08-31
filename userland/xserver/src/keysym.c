/*
 * SzpontOS - SzpontX11 Native X11 Server
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * Evdev Linux Scancode to X11 KeySym Translation
 */

#include "keysym_defs.h"
#include <linux/input-event-codes.h>

uint32_t evdev_to_keysym(uint16_t code, bool shift, bool caps) {
    bool upper = shift ^ caps;

    switch (code) {
    case KEY_ESC:        return XK_Escape;
    case KEY_1:          return shift ? XK_exclam : XK_1;
    case KEY_2:          return shift ? XK_at : XK_2;
    case KEY_3:          return shift ? XK_numbersign : XK_3;
    case KEY_4:          return shift ? XK_dollar : XK_4;
    case KEY_5:          return shift ? XK_percent : XK_5;
    case KEY_6:          return shift ? XK_asciicircum : XK_6;
    case KEY_7:          return shift ? XK_ampersand : XK_7;
    case KEY_8:          return shift ? XK_asterisk : XK_8;
    case KEY_9:          return shift ? XK_parenleft : XK_9;
    case KEY_0:          return shift ? XK_parenright : XK_0;
    case KEY_MINUS:      return shift ? XK_underscore : XK_minus;
    case KEY_EQUAL:      return shift ? XK_plus : XK_equal;
    case KEY_BACKSPACE:  return XK_BackSpace;
    case KEY_TAB:        return XK_Tab;
    case KEY_Q:          return upper ? XK_Q : XK_q;
    case KEY_W:          return upper ? XK_W : XK_w;
    case KEY_E:          return upper ? XK_E : XK_e;
    case KEY_R:          return upper ? XK_R : XK_r;
    case KEY_T:          return upper ? XK_T : XK_t;
    case KEY_Y:          return upper ? XK_Y : XK_y;
    case KEY_U:          return upper ? XK_U : XK_u;
    case KEY_I:          return upper ? XK_I : XK_i;
    case KEY_O:          return upper ? XK_O : XK_o;
    case KEY_P:          return upper ? XK_P : XK_p;
    case KEY_LEFTBRACE:  return shift ? XK_braceleft : XK_bracketleft;
    case KEY_RIGHTBRACE: return shift ? XK_braceright : XK_bracketright;
    case KEY_ENTER:      return XK_Return;
    case KEY_LEFTCTRL:   return XK_Control_L;
    case KEY_A:          return upper ? XK_A : XK_a;
    case KEY_S:          return upper ? XK_S : XK_s;
    case KEY_D:          return upper ? XK_D : XK_d;
    case KEY_F:          return upper ? XK_F : XK_f;
    case KEY_G:          return upper ? XK_G : XK_g;
    case KEY_H:          return upper ? XK_H : XK_h;
    case KEY_J:          return upper ? XK_J : XK_j;
    case KEY_K:          return upper ? XK_K : XK_k;
    case KEY_L:          return upper ? XK_L : XK_l;
    case KEY_SEMICOLON:  return shift ? XK_colon : XK_semicolon;
    case KEY_APOSTROPHE: return shift ? XK_quotedbl : XK_apostrophe;
    case KEY_GRAVE:      return shift ? XK_asciitilde : XK_grave;
    case KEY_LEFTSHIFT:  return XK_Shift_L;
    case KEY_BACKSLASH:  return shift ? XK_bar : XK_backslash;
    case KEY_Z:          return upper ? XK_Z : XK_z;
    case KEY_X:          return upper ? XK_X : XK_x;
    case KEY_C:          return upper ? XK_C : XK_c;
    case KEY_V:          return upper ? XK_V : XK_v;
    case KEY_B:          return upper ? XK_B : XK_b;
    case KEY_N:          return upper ? XK_N : XK_n;
    case KEY_M:          return upper ? XK_M : XK_m;
    case KEY_COMMA:      return shift ? XK_less : XK_comma;
    case KEY_DOT:        return shift ? XK_greater : XK_period;
    case KEY_SLASH:      return shift ? XK_question : XK_slash;
    case KEY_RIGHTSHIFT: return XK_Shift_R;
    case KEY_LEFTALT:    return XK_Alt_L;
    case KEY_SPACE:      return XK_space;
    case KEY_CAPSLOCK:   return XK_Caps_Lock;
    case KEY_F1:         return XK_F1;
    case KEY_F2:         return XK_F2;
    case KEY_F3:         return XK_F3;
    case KEY_F4:         return XK_F4;
    case KEY_F5:         return XK_F5;
    case KEY_F6:         return XK_F6;
    case KEY_F7:         return XK_F7;
    case KEY_F8:         return XK_F8;
    case KEY_F9:         return XK_F9;
    case KEY_F10:        return XK_F10;
    case KEY_F11:        return XK_F11;
    case KEY_F12:        return XK_F12;
    case KEY_RIGHTCTRL:  return XK_Control_R;
    case KEY_RIGHTALT:   return XK_Alt_R;
    case KEY_HOME:       return XK_Home;
    case KEY_UP:         return XK_Up;
    case KEY_PAGEUP:     return XK_Page_Up;
    case KEY_LEFT:       return XK_Left;
    case KEY_RIGHT:      return XK_Right;
    case KEY_END:        return XK_End;
    case KEY_DOWN:       return XK_Down;
    case KEY_PAGEDOWN:   return XK_Page_Down;
    case KEY_INSERT:     return XK_Insert;
    case KEY_DELETE:     return XK_Delete;
    default:             return 0;
    }
}
