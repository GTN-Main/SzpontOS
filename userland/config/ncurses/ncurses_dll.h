/*
 * SzpontOS - ncurses_dll.h
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef NCURSES_DLL_H
#define NCURSES_DLL_H

#define NCURSES_IMPEXP
#define NCURSES_API
#define NCURSES_EXPORT(type) type
#define NCURSES_EXPORT_VAR(type) type
#define NCURSES_PUBLIC_VAR(name) _nc_##name
#define NCURSES_WRAPPED_VAR(type,name) type

#endif /* NCURSES_DLL_H */
