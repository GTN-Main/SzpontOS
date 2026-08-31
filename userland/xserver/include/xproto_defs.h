/*
 * SzpontOS - SzpontX11 Native X11 Server
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * X11 Wire Protocol Definitions & Constants (X11 Core Protocol 11.0)
 */

#ifndef SZPONT_XPROTO_DEFS_H
#define SZPONT_XPROTO_DEFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define X11_PROTOCOL_VERSION_MAJOR 11
#define X11_PROTOCOL_VERSION_MINOR 0

/* Core Request Opcodes */
#define X_CreateWindow              1
#define X_ChangeWindowAttributes    2
#define X_GetWindowAttributes       3
#define X_DestroyWindow             4
#define X_DestroySubwindows         5
#define X_ChangeSaveSet             6
#define X_ReparentWindow            7
#define X_MapWindow                 8
#define X_MapSubwindows             9
#define X_UnmapWindow               10
#define X_UnmapSubwindows           11
#define X_ConfigureWindow           12
#define X_CirculateWindow           13
#define X_GetGeometry               14
#define X_QueryTree                 15
#define X_InternAtom                16
#define X_GetAtomName               17
#define X_ChangeProperty            18
#define X_DeleteProperty            19
#define X_GetProperty               20
#define X_ListProperties            21
#define X_SetSelectionOwner         22
#define X_GetSelectionOwner         23
#define X_ConvertSelection          24
#define X_SendEvent                 25
#define X_GrabPointer               26
#define X_UngrabPointer             27
#define X_GrabButton                28
#define X_UngrabButton              29
#define X_ChangeActivePointerGrab   30
#define X_GrabKeyboard              31
#define X_UngrabKeyboard            32
#define X_GrabKey                   33
#define X_UngrabKey                 34
#define X_AllowEvents               35
#define X_GrabServer                36
#define X_UngrabServer              37
#define X_QueryPointer              38
#define X_GetMotionEvents           39
#define X_TranslateCoords           40
#define X_WarpPointer               41
#define X_SetInputFocus             42
#define X_GetInputFocus             43
#define X_QueryKeymap               44
#define X_OpenFont                  45
#define X_CloseFont                 46
#define X_QueryFont                 47
#define X_QueryTextExtents          48
#define X_ListFonts                 49
#define X_ListFontsWithInfo         50
#define X_SetFontPath               51
#define X_GetFontPath               52
#define X_CreatePixmap              53
#define X_FreePixmap                54
#define X_CreateGC                  55
#define X_ChangeGC                  56
#define X_CopyGC                    57
#define X_SetDashes                 58
#define X_SetClipRectangles         59
#define X_FreeGC                    60
#define X_ClearArea                 61
#define X_CopyArea                  62
#define X_CopyPlane                 63
#define X_PolyPoint                 64
#define X_PolyLine                  65
#define X_PolySegment               66
#define X_PolyRectangle             67
#define X_PolyArc                   68
#define X_FillPoly                  69
#define X_PolyFillRectangle         70
#define X_PolyFillArc               71
#define X_PutImage                  72
#define X_GetImage                  73
#define X_PolyText8                 74
#define X_PolyText16                75
#define X_ImageText8                76
#define X_ImageText16               77
#define X_CreateColormap            78
#define X_FreeColormap              79
#define X_CopyColormapAndFree       80
#define X_InstallColormap           81
#define X_UninstallColormap         82
#define X_ListInstalledColormaps    83
#define X_AllocColor                84
#define X_AllocNamedColor           85
#define X_AllocColorCells           86
#define X_AllocColorPlanes          87
#define X_FreeColors                88
#define X_StoreColors               89
#define X_StoreNamedColor           90
#define X_QueryColors               91
#define X_LookupColor               92
#define X_CreateCursor              93
#define X_CreateGlyphCursor         94
#define X_FreeCursor                95
#define X_RecolorCursor             96
#define X_QueryBestSize             97
#define X_QueryExtension            98
#define X_ListExtensions            99
#define X_ChangeKeyboardMapping     100
#define X_GetKeyboardMapping        101
#define X_ChangeKeyboardControl     102
#define X_GetKeyboardControl        103
#define X_Bell                      104
#define X_ChangePointerControl      105
#define X_GetPointerControl         106
#define X_SetScreenSaver            107
#define X_GetScreenSaver            108
#define X_ChangeHosts               109
#define X_ListHosts                 110
#define X_SetAccessControl          111
#define X_SetCloseDownMode          112
#define X_KillClient                113
#define X_RotateProperties          114
#define X_ForceScreenSaver          115
#define X_SetPointerMapping         116
#define X_GetPointerMapping         117
#define X_SetModifierMapping        118
#define X_GetModifierMapping        119
#define X_NoOperation               127

/* Core Event Codes */
#define X_KeyPress                  2
#define X_KeyRelease                3
#define X_ButtonPress               4
#define X_ButtonRelease             5
#define X_MotionNotify              6
#define X_EnterNotify               7
#define X_LeaveNotify               8
#define X_FocusIn                   9
#define X_FocusOut                  10
#define X_KeymapNotify              11
#define X_Expose                    12
#define X_GraphicsExpose            13
#define X_NoExpose                  14
#define X_VisibilityNotify          15
#define X_CreateNotify              16
#define X_DestroyNotify             17
#define X_UnmapNotify               18
#define X_MapNotify                 19
#define X_MapRequest                20
#define X_ReparentNotify            21
#define X_ConfigureNotify           22
#define X_ConfigureRequest          23
#define X_GravityNotify             24
#define X_ResizeRequest             25
#define X_CirculateNotify           26
#define X_CirculateRequest          27
#define X_PropertyNotify            28
#define X_SelectionClear            29
#define X_SelectionRequest          30
#define X_SelectionNotify           31
#define X_ColormapNotify            32
#define X_ClientMessage             33
#define X_MappingNotify             34

/* Core Error Codes */
#define BadSuccess                  0
#define BadRequest                  1
#define BadValue                    2
#define BadWindow                   3
#define BadPixmap                   4
#define BadAtom                     5
#define BadCursor                   6
#define BadFont                     7
#define BadMatch                    8
#define BadDrawable                 9
#define BadAccess                   10
#define BadAlloc                    11
#define BadColor                    12
#define BadGC                       13
#define BadIDChoice                 14
#define BadName                     15
#define BadLength                   16
#define BadImplementation           17

/* Event Masks */
#define NoEventMask                 0L
#define KeyPressMask                (1L<<0)
#define KeyReleaseMask              (1L<<1)
#define ButtonPressMask             (1L<<2)
#define ButtonReleaseMask           (1L<<3)
#define EnterWindowMask             (1L<<4)
#define LeaveWindowMask             (1L<<5)
#define PointerMotionMask           (1L<<6)
#define PointerMotionHintMask       (1L<<7)
#define Button1MotionMask           (1L<<8)
#define Button2MotionMask           (1L<<9)
#define Button3MotionMask           (1L<<10)
#define Button4MotionMask           (1L<<11)
#define Button5MotionMask           (1L<<12)
#define ButtonMotionMask            (1L<<13)
#define KeymapStateMask             (1L<<14)
#define ExposureMask                (1L<<15)
#define VisibilityChangeMask        (1L<<16)
#define StructureNotifyMask         (1L<<17)
#define ResizeRedirectMask          (1L<<18)
#define SubstructureNotifyMask      (1L<<19)
#define SubstructureRedirectMask    (1L<<20)
#define FocusChangeMask             (1L<<21)
#define PropertyChangeMask          (1L<<22)
#define ColormapChangeMask          (1L<<23)
#define OwnerGrabButtonMask         (1L<<24)

/* Window Classes */
#define InputOutput                 1
#define InputOnly                   2

/* Visual Classes */
#define StaticGray                  0
#define GrayScale                   1
#define StaticColor                 2
#define PseudoColor                 3
#define TrueColor                   4
#define DirectColor                 5

/* Graphics Context Masks */
#define GCFunction                  (1L<<0)
#define GCPlaneMask                 (1L<<1)
#define GCForeground                (1L<<2)
#define GCBackground                (1L<<3)
#define GCLineWidth                 (1L<<4)
#define GCLineStyle                 (1L<<5)
#define GCCapStyle                  (1L<<6)
#define GCJoinStyle                 (1L<<7)
#define GCFillStyle                 (1L<<8)
#define GCFillRule                  (1L<<9)
#define GCTile                      (1L<<10)
#define GCStipple                   (1L<<11)
#define GCTileStipXOrigin           (1L<<12)
#define GCTileStipYOrigin           (1L<<13)
#define GCFont                      (1L<<14)
#define GCSubwindowMode             (1L<<15)
#define GCGraphicsExposures         (1L<<16)
#define GCClipXOrigin               (1L<<17)
#define GCClipYOrigin               (1L<<18)
#define GCClipMask                  (1L<<19)
#define GCDashOffset                (1L<<20)
#define GCDashList                  (1L<<21)
#define GCArcMode                   (1L<<22)

/* GC Drawing Functions (Raster Ops) */
#define GXclear                     0x0
#define GXand                       0x1
#define GXandReverse                0x2
#define GXcopy                      0x3
#define GXandInverted               0x4
#define GXnoop                      0x5
#define GXxor                       0x6
#define GXor                        0x7
#define GXnor                       0x8
#define GXequiv                     0x9
#define GXinvert                    0xa
#define GXorReverse                 0xb
#define GXcopyInverted              0xc
#define GXorInverted                0xd
#define GXnand                      0xe
#define GXset                       0xf

/* Predefined Standard Atoms */
#define XA_PRIMARY                  ((uint32_t)1)
#define XA_SECONDARY                ((uint32_t)2)
#define XA_ARC                      ((uint32_t)3)
#define XA_ATOM                     ((uint32_t)4)
#define XA_BITMAP                   ((uint32_t)5)
#define XA_CARDINAL                 ((uint32_t)6)
#define XA_COLORMAP                 ((uint32_t)7)
#define XA_CURSOR                   ((uint32_t)8)
#define XA_CUT_BUFFER0              ((uint32_t)9)
#define XA_DRAWABLE                 ((uint32_t)17)
#define XA_FONT                     ((uint32_t)18)
#define XA_INTEGER                  ((uint32_t)19)
#define XA_PIXMAP                   ((uint32_t)20)
#define XA_POINT                    ((uint32_t)21)
#define XA_RECTANGLE                ((uint32_t)22)
#define XA_RESOURCE_MANAGER         ((uint32_t)23)
#define XA_RGB_COLOR_MAP            ((uint32_t)24)
#define XA_STRING                   ((uint32_t)31)
#define XA_VISUALID                 ((uint32_t)32)
#define XA_WINDOW                   ((uint32_t)33)
#define XA_WM_COMMAND               ((uint32_t)34)
#define XA_WM_HINTS                 ((uint32_t)35)
#define XA_WM_CLIENT_MACHINE        ((uint32_t)36)
#define XA_WM_ICON_NAME             ((uint32_t)37)
#define XA_WM_ICON_SIZE             ((uint32_t)38)
#define XA_WM_NAME                  ((uint32_t)39)
#define XA_WM_NORMAL_HINTS          ((uint32_t)40)
#define XA_WM_SIZE_HINTS            ((uint32_t)41)
#define XA_WM_ZOOM_HINTS            ((uint32_t)42)
#define XA_MIN_SPACE                ((uint32_t)43)
#define XA_NORM_SPACE               ((uint32_t)44)
#define XA_MAX_SPACE                ((uint32_t)45)
#define XA_END_SPACE                ((uint32_t)46)
#define XA_SUPERSCRIPT_X            ((uint32_t)47)
#define XA_SUPERSCRIPT_Y            ((uint32_t)48)
#define XA_SUBSCRIPT_X              ((uint32_t)49)
#define XA_SUBSCRIPT_Y              ((uint32_t)50)
#define XA_UNDERLINE_POSITION       ((uint32_t)51)
#define XA_UNDERLINE_THICKNESS      ((uint32_t)52)
#define XA_STRIKEOUT_ASCENT         ((uint32_t)53)
#define XA_STRIKEOUT_DESCENT        ((uint32_t)54)
#define XA_ITALIC_ANGLE             ((uint32_t)55)
#define XA_X_HEIGHT                 ((uint32_t)56)
#define XA_QUAD_WIDTH               ((uint32_t)57)
#define XA_WEIGHT                   ((uint32_t)58)
#define XA_POINT_SIZE               ((uint32_t)59)
#define XA_RESOLUTION               ((uint32_t)60)
#define XA_COPYRIGHT                ((uint32_t)61)
#define XA_NOTICE                   ((uint32_t)62)
#define XA_FONT_NAME                ((uint32_t)63)
#define XA_FAMILY_NAME              ((uint32_t)64)
#define XA_FULL_NAME                ((uint32_t)65)
#define XA_CAP_HEIGHT               ((uint32_t)66)
#define XA_WM_CLASS                 ((uint32_t)67)
#define XA_WM_TRANSIENT_FOR         ((uint32_t)68)
#define XA_LAST_PREDEFINED          ((uint32_t)68)

/* Packets Wire Structures */
#pragma pack(push, 1)

typedef struct {
    uint8_t  byte_order;
    uint8_t  pad1;
    uint16_t major_version;
    uint16_t minor_version;
    uint16_t auth_proto_len;
    uint16_t auth_data_len;
    uint16_t pad2;
} x11_conn_client_prefix_t;

typedef struct {
    uint8_t  status; /* 1 = Success, 0 = Failed, 2 = Authenticate */
    uint8_t  length_reason;
    uint16_t major_version;
    uint16_t minor_version;
    uint16_t length; /* Additional length in 4-byte units */
} x11_conn_setup_prefix_t;

typedef struct {
    uint32_t release_number;
    uint32_t resource_id_base;
    uint32_t resource_id_mask;
    uint32_t motion_buffer_size;
    uint16_t vendor_len;
    uint16_t max_request_size;
    uint8_t  num_screens;
    uint8_t  num_formats;
    uint8_t  image_byte_order; /* 0 = LSBFirst, 1 = MSBFirst */
    uint8_t  bitmap_bit_order;
    uint8_t  bitmap_format_scanline_unit;
    uint8_t  bitmap_format_scanline_pad;
    uint8_t  min_keycode;
    uint8_t  max_keycode;
    uint32_t pad;
} x11_conn_setup_t;

typedef struct {
    uint8_t  depth;
    uint8_t  bits_per_pixel;
    uint8_t  scanline_pad;
    uint8_t  pad[5];
} x11_pixmap_format_t;

typedef struct {
    uint32_t visual_id;
    uint8_t  class_val;
    uint8_t  bits_per_rgb;
    uint16_t colormap_entries;
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
    uint32_t pad;
} x11_visual_type_t;

typedef struct {
    uint8_t  depth;
    uint8_t  pad1;
    uint16_t num_visuals;
    uint32_t pad2;
} x11_depth_t;

typedef struct {
    uint32_t window_id;
    uint32_t colormap_id;
    uint32_t white_pixel;
    uint32_t black_pixel;
    uint32_t current_input_masks;
    uint16_t width_in_pixels;
    uint16_t height_in_pixels;
    uint16_t width_in_millimeters;
    uint16_t height_in_millimeters;
    uint16_t min_installed_maps;
    uint16_t max_installed_maps;
    uint32_t root_visual_id;
    uint8_t  backing_stores;
    uint8_t  save_unders;
    uint8_t  root_depth;
    uint8_t  num_depths;
} x11_screen_t;

typedef struct {
    uint8_t  req_type;
    uint8_t  data;
    uint16_t length; /* in 4-byte units */
} x11_req_header_t;

typedef struct {
    uint8_t  response_type; /* 1 = Reply, 0 = Error */
    uint8_t  data;
    uint16_t sequence_number;
    uint32_t length; /* in 4-byte units */
} x11_reply_header_t;

typedef struct {
    uint8_t  response_type; /* 0 = Error */
    uint8_t  error_code;
    uint16_t sequence_number;
    uint32_t bad_value;
    uint16_t minor_opcode;
    uint8_t  major_opcode;
    uint8_t  pad[21];
} x11_error_event_t;

typedef struct {
    uint8_t  type;
    uint8_t  detail;
    uint16_t sequence_number;
    uint32_t time;
    uint32_t root;
    uint32_t event;
    uint32_t child;
    int16_t  root_x;
    int16_t  root_y;
    int16_t  event_x;
    int16_t  event_y;
    uint16_t state;
    uint8_t  same_screen;
    uint8_t  pad;
} x11_key_button_pointer_event_t;

typedef struct {
    uint8_t  type;
    uint8_t  pad1;
    uint16_t sequence_number;
    uint32_t window;
    int16_t  x;
    int16_t  y;
    uint16_t width;
    uint16_t height;
    uint16_t count;
    uint8_t  pad2[14];
} x11_expose_event_t;

typedef struct {
    uint8_t  type;
    uint8_t  pad1;
    uint16_t sequence_number;
    uint32_t event;
    uint32_t window;
    uint8_t  override_redirect;
    uint8_t  pad2[19];
} x11_map_event_t;

typedef struct {
    uint8_t  type;
    uint8_t  pad1;
    uint16_t sequence_number;
    uint32_t event;
    uint32_t window;
    uint32_t above_sibling;
    int16_t  x;
    int16_t  y;
    uint16_t width;
    uint16_t height;
    uint16_t border_width;
    uint8_t  override_redirect;
    uint8_t  pad2[5];
} x11_configure_event_t;

typedef struct {
    uint8_t  type;
    uint8_t  format;
    uint16_t sequence_number;
    uint32_t window;
    uint32_t message_type;
    union {
        uint8_t  b[20];
        uint16_t s[10];
        uint32_t l[5];
    } data;
} x11_client_message_event_t;

#pragma pack(pop)

#endif /* SZPONT_XPROTO_DEFS_H */
