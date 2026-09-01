#!/usr/bin/env python3
"""
Fetch or cache Mozilla CA Root Certificates bundle for SzpontOS.
Usage: scripts/fetch_cacerts.py <dest_cert_pem> [<dest_ca_certs_crt> ...]
"""

import sys
import os
import urllib.request

CACERT_URL = "https://curl.se/ca/cacert.pem"

def main():
    if len(sys.argv) < 2:
        print("Usage: fetch_cacerts.py <output_file> [<output_file2> ...]", file=sys.stderr)
        sys.exit(1)

    dest_files = [os.path.abspath(p) for p in sys.argv[1:]]
    script_dir = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.abspath(os.path.join(script_dir, ".."))
    cache_file = os.path.join(root_dir, "build", "cacert.pem")
    os.makedirs(os.path.dirname(cache_file), exist_ok=True)

    data = None
    if os.path.exists(cache_file) and os.path.getsize(cache_file) > 1000:
        with open(cache_file, "rb") as f:
            data = f.read()
    else:
        print(f"[*] Pobieranie certyfikatów Mozilla Root CA z {CACERT_URL}...")
        try:
            req = urllib.request.Request(CACERT_URL, headers={"User-Agent": "SzpontOS-FetchCACerts/1.0"})
            with urllib.request.urlopen(req, timeout=30) as resp:
                data = resp.read()
                with open(cache_file, "wb") as f:
                    f.write(data)
            print(f"[OK] Pobrano {len(data)} bajtów certyfikatów CA.")
        except Exception as e:
            print(f"[!] Błąd pobierania certyfikatów z sieci: {e}")
            if os.path.exists(cache_file) and os.path.getsize(cache_file) > 0:
                with open(cache_file, "rb") as f:
                    data = f.read()
            else:
                sys.exit(1)

    for dest in dest_files:
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        with open(dest, "wb") as f:
            f.write(data)
        print(f"  + Zapisano certyfikaty do {dest}")

if __name__ == "__main__":
    main()
