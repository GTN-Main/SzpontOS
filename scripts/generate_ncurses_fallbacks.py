#!/usr/bin/env python3
"""
Generator pliku fallback.c dla GNU Ncurses w SzpontOS.
Używa systemowego narzędzia infocmp do wygenerowania wbudowanych definicji terminali
(xterm, xterm-256color, vt100, vt220, linux, ansi, screen, tmux, rxvt, unknown)
bez potrzeby posiadania zewnętrznej bazy terminfo na dysku.
"""

import sys
import subprocess
import os

TERMS = ["xterm", "xterm-256color", "vt100", "vt220", "linux", "ansi", "screen", "tmux", "rxvt", "unknown"]

def generate_fallback_c(output_path):
    print(f"[*] Generowanie fallback.c do: {output_path} dla terminali: {', '.join(TERMS)}")
    
    entries_E = []
    entries_e = []
    valid_terms = []

    for term in TERMS:
        try:
            res_E = subprocess.run(["infocmp", "-E", term], stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True, text=True)
            res_e = subprocess.run(["infocmp", "-e", term], stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True, text=True)
            
            # Zamień short na NCURSES_INT2 zgodnie z wymaganiami ncurses
            clean_E = res_E.stdout.replace("short", "NCURSES_INT2")
            entries_E.append((term, clean_E))
            entries_e.append((term, res_e.stdout))
            valid_terms.append(term)
            print(f"  + Wygenerowano wpis dla: {term}")
        except Exception as e:
            print(f"  - Ostrzeżenie: Nie udało się pobrać definicji dla '{term}': {e}", file=sys.stderr)

    if not valid_terms:
        print("[!] Błąd: Brak prawidłowych terminali do fallback.c!", file=sys.stderr)
        sys.exit(1)

    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    with open(output_path, "w") as f:
        f.write("/* Wygenerowano automatycznie dla SzpontOS ncurses */\n")
        f.write("#include <curses.priv.h>\n")
        f.write("#include <tic.h>\n\n")

        for term, code in entries_E:
            f.write(f"/* === Terminfo definition for: {term} === */\n")
            f.write(code)
            f.write("\n\n")

        f.write(f"static const TERMTYPE2 fallbacks[{len(valid_terms)}] =\n{{\n")
        for i, (term, code) in enumerate(entries_e):
            comma = "," if i > 0 else " "
            f.write(f"\t{comma} /* {term} */\n")
            f.write(code)
        f.write("};\n\n")

        f.write("""NCURSES_EXPORT(const TERMTYPE2 *)
_nc_fallback2 (const char *name GCC_UNUSED)
{
    const TERMTYPE2 *tp;

    if (!name || !*name) return &fallbacks[0];

    for (tp = fallbacks;
         tp < fallbacks + sizeof(fallbacks)/sizeof(TERMTYPE2);
         tp++) {
        if (_nc_name_match(tp->term_names, name, "|")) {
            return tp;
        }
    }
    /* Default fallback to first valid terminal (xterm) if unknown */
    return &fallbacks[0];
}

#if NCURSES_EXT_NUMBERS
#undef _nc_fallback
NCURSES_EXPORT(const TERMTYPE *)
_nc_fallback (const char *name)
{
    const TERMTYPE2 *tp = _nc_fallback2(name);
    const TERMTYPE *result = 0;
    if (tp != 0) {
        static TERMTYPE temp;
        _nc_export_termtype2(&temp, tp);
        result = &temp;
    }
    return result;
}
#endif
""")

    print(f"[OK] Wygenerowano pomyślnie {output_path} ({os.path.getsize(output_path)} bajtów).")

if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "build/third_party/ncurses/ncurses/fallback.c"
    generate_fallback_c(out)
