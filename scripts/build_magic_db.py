#!/usr/bin/env python3
"""
SzpontOS - Magic Database Builder for 'file' and 'libmagic'
(C) Copyright by Szpont Industries. All rights reserved.
"""

import os
import sys

BASE_MAGIC = """# SzpontOS System Magic Database
# (C) Copyright by Szpont Industries. All rights reserved.

# --- Executables & Binaries ---
0	string		\\177ELF		ELF
>4	byte		1		32-bit
>4	byte		2		64-bit
>5	byte		1		LSB
>5	byte		2		MSB
>16	leshort		1		relocatable,
>16	leshort		2		executable,
>16	leshort		3		shared object,
>16	leshort		4		core file,
>18	leshort		62		x86-64
>18	leshort		3		Intel 80386
>18	leshort		183		ARM aarch64

# --- Shell & Scripts ---
0	string		#!/bin/sh	POSIX shell script text executable
0	string		#!/bin/bash	Bourne-Again shell script text executable
0	string		#!/usr/bin/env	script text executable
0	string		#!		script text executable

# --- Archives ---
257	string		ustar		POSIX tar archive (USTAR)
0	string		\\037\\213	gzip compressed data
0	string		BZh		bzip2 compressed data
0	string		\\3757zXZ\\0	XZ compressed data
0	string		PK\\003\\004	Zip archive data
0	string		7z\\274\\257\\047\\034	7-zip archive data

# --- Images & Multimedia ---
0	string		\\211PNG\\r\\n\\032\\n	PNG image data
0	string		\\377\\338\\377	JPEG image data
0	string		GIF87a		GIF image data
0	string		GIF89a		GIF image data
0	string		BM		PC bitmap, Windows BMP image data
0	string		RIFF		RIFF (little-endian) data

# --- Documents & Text ---
0	string		%PDF-		PDF document
0	string		/*		C source, ASCII text
0	string		//		C++ source, ASCII text
0	string		#include	C source, ASCII text
"""

def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <magdir_path> <output_magic_path>")
        sys.exit(1)

    magdir = sys.argv[1]
    out_file = sys.argv[2]

    os.makedirs(os.path.dirname(out_file), exist_ok=True)

    with open(out_file, "w", encoding="utf-8") as out:
        out.write(BASE_MAGIC)
        out.write("\n")

    print(f"[*] Generated magic database: {out_file} ({os.path.getsize(out_file)} bytes)")

if __name__ == "__main__":
    main()
