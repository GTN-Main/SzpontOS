# SzpontOS
> **SzpontOS** to pierwszy w pełni naszponcony, 64-bitowy system operacyjny pisany od zera zgodnie z myślą techniczną dr. hab. Igora "Makaljera" Grabowskiego, największego Szponciciela, twórcy i pioniera Szpontu.
> Projekt został całkowicie przyszponcony przy użyciu **Szpont Maszyny** Klaudiusz Kodiusz z Modelem Klaudiusz Fable 5.

<p align="center">
  <img src="artwork/szpont-detected.jpg" alt="Szpont Detected" width="450" />
</p>

<p align="center">
  <img src="https://img.shields.io/badge/OS-SzpontOS-blue.svg" alt="OS" />
  <img src="https://img.shields.io/badge/Arch-x86__64-green.svg" alt="Arch" />
  <img src="https://img.shields.io/badge/Bootloader-Limine%20v8-orange.svg" alt="Limine" />
  <img src="https://img.shields.io/badge/SMP-Up%20to%2064%20Cores-red.svg" alt="SMP" />
  <img src="https://img.shields.io/badge/Display-X.Org%20X11-informational.svg" alt="X11" />
  <img src="https://img.shields.io/badge/Desktop-Szpont%20Experience-blue.svg" alt="Szpont Experience" />
  <img src="https://img.shields.io/badge/3D%20Graphics-Mesa%203D%2025.x-yellowgreen.svg" alt="Mesa" />
  <img src="https://img.shields.io/badge/C%2B%2B%20Runtime-libstdc%2B%2B-blueviolet.svg" alt="libstdc++" />
  <img src="https://img.shields.io/badge/Szpont-Kwantowy-purple.svg" alt="Szpont" />
  <img src="https://img.shields.io/badge/Licencja-MIT-yellow.svg" alt="License" />
</p>

<p align="center">
  <img src="artwork/screenshot.png" alt="SzpontOS Szpont Experience Screenshot" width="780" />
</p>

---

## Funkcje i Naszponcona Architektura

SzpontOS łączy potęgę nowoczesnego jądra monolitycznego napisanego w standardzie **C17** i asemblerze x86_64 z kompletnym, graficznym środowiskiem użytkownika X11, akceleracją 3D oraz bogatym ekosystemem narzędzi:

### 1. Środowisko Graficzne X11 & Szpont Experience
- **Autorski Menedżer Okien i Środowisko (Szpont Experience):** Superlekkie, wysoce przenośne i responsywne środowisko graficzne (reparenting window manager) napisane od podstaw w C z bezpośrednim wykorzystaniem Xlib:
  - **Minimalny narzut i bezkompromisowa wydajność:** Całkowicie niezależne od ciężkich toolkitów (GTK, Qt) – zużywa znikome zasoby pamięci RAM i cykli procesora, uruchamiając się błyskawicznie na dowolnym systemie z serwerem X11.
  - **Pełna przenośność:** Czysty kod w standardowym C17 i Xlib/XShape bez zbędnych zależności zewnętrznych – łatwy do skompilowania i uruchomienia na różnych systemach operacyjnych i architekturach.
  - **Zaokrąglone rogi okien** realizowane sprzętowo za pomocą rozszerzenia **XShape**.
  - **Szklany TopBar** (pasek górny) z zegarem RTC i panelem statusu.
  - **Dynamiczny Taskbar** (pasek zadań) z aktywnymi ikonami, podglądem procesów i minimalizacją/przywracaniem okien.
  - **Kontrolki okna w stylu Traffic Light** (zamknij, zminimalizuj, zmaksymalizuj) z płynną obsługą zdarzeń kursora.
  - Generowany algorytmicznie **gradient tła pulpitu**.
  - Płynne przesuwanie i zmiana rozmiarów okien (*drag & resize*).
- **Oficjalny X.Org Server (X11):** Natywny port serwera `Xorg` działający w oparciu o sterownik DRM/KMS modesetting oraz pełne mapowanie układów klawiatury **XKB** (`xkeyboard-config`).
- **Natywny Terminal X11 (SzponTerm):** Dedykowany, lekki emulator terminala z pełną obsługą pseudoterminali PTY (`/dev/ptmx`, `/dev/pts/*`), sekwencji ucieczki ANSI, palety 256 kolorów i bufora przewijania.
- **Wizualne Narzędzia GUI:** `szpontlogin` (stylowy ekran logowania i wyboru sesji), `szpontdetected` oraz starter grafiki `startx`.

### 2. Podsystem DRM/KMS, Mesa 3D & Akceleracja OpenGL
- **Natywne Sterowniki Jądra DRM:** Węzeł główny `/dev/dri/card0` oraz render nodes `/dev/dri/renderD128` z obsługą dumb bufferów, mapowania mmap i autoryzacji PID.
- **PRIME dma-buf Buffer Sharing:** Bezpośrednie współdzielenie buforów graficznych pomiędzy procesami w pamięci zero-copy z użyciem gniazd uniksowych i deskryptorów `SCM_RIGHTS`.
- **Własne Biblioteki Grafikowe:** Autorskie implementacje `libdrm.so` (funkcje `xf86drm`, `xf86drmMode`) oraz `libgbm.so` (Generic Buffer Management).
- **Port Mesa 3D (25.x):** Pełny stos 3D z driverami Gallium (`softpipe`, `llvmpipe`, `virtio-gpu`), EGL (`libEGL.so`) oraz OpenGL ES 2.0 (`libGLESv2.so`).
- **Dema i Testy 3D:** Wirujące koła zębate `glxgears`, dynamiczny `gltriangle`, `mesadrmtest`, `drmtest` oraz kultowy terminalowy `donut`.

### 3. Wielordzeniowość (SMP — Symmetric Multiprocessing)
- **Obsługa do 64 Rdzeni CPU:** Równoległe planowanie procesów i wątków w trybie SMP.
- **ACPI & MADT:** Automatyczne wykrywanie topologii procesorów i konfiguracji kontrolerów APIC/IOAPIC z tablic BIOS/UEFI.
- **Niezależne Zegary LAPIC:** Każdy rdzeń procesora konfiguruje własny, lokalny timer APIC (1000 Hz) dla precyzyjnego wywłaszczania zadań.
- **IPI & Spinlocki:** Przerwania międzyprocesorowe (Inter-Processor Interrupts) oraz blokady wielordzeniowe gwarantujące spójność struktur jądra.
- **Per-CPU GS Segment:** Sprzętowy wskaźnik do struktury `cpu_t` przechowywany w rejestrze MSR `%gs:0`.

### 4. Środowisko C++ i GNU libstdc++-v3
- **Kompletny Runtime C++:** Port oficjalnej biblioteki **GNU libstdc++-v3** (`libstdc++.so`, `libstdc++.a`) skompilowany przeciwko sysroot SzpontOS.
- **Pełne Wsparcie Standardu C++17 i C++20:** RTTI (`typeid`, `dynamic_cast`), obsługa wyjątków (`try`, `catch`, `throw`), alokatory pamięci oraz standardowe kontenery i strumienie STL (`std::vector`, `std::string`, `std::map`, `std::cout`).
- **Cross-Kompilatory SzpontOS:** Zestaw wrapperów `szpontos-gcc` oraz `szpontos-g++` dla budowania aplikacji w C i C++.

### 5. Zaawansowany Podsystem Kompatybilności z Linuksem
- **epoll(7):** Kompletny mechanizm multipleksacji I/O ze wsparciem dla Level-Triggered (LT), Edge-Triggered (ET), `EPOLLONESHOT` oraz `EPOLLEXCLUSIVE`.
- **kqueue(2):** Klasyczny mechanizm subskrypcji zdarzeń wzorowany na FreeBSD (`EVFILT_READ`, `EVFILT_WRITE`).
- **eventfd(2):** 64-bitowy licznik i semafor jądra do asynchronicznej synchronizacji procesów.
- **timerfd(2):** Precyzyjne timery dostarczające powiadomienia czasowe poprzez deskryptory plików.
- **signalfd(4):** Odbieranie sygnałów uniksowych (`SIGINT`, `SIGCHLD`, etc.) bezpośrednio w pętli zdarzeń bez asynchronicznych handlerów.
- **inotify(7):** Subsystem powiadomień o zmianach w systemie plików (tworzenie, modyfikacja, kasowanie plików) zintegrowany z VFS.
- **Wirtualny System Plików sysfs:** Hierarchia `/sys` udostępniająca drzewo urządzeń, magistralę PCI oraz klasy DRM.
- **Zaawansowane Syscalle:** Atomowe `pipe2(2)`, `dup3(2)`, zmiana katalogu przez deskryptor `fchdir(2)`, grupy procesów (PGID/SID) i izolacja sesji terminalowych.

### 6. Sieć, Zdalny Dostęp i Bezpieczeństwo
- **Pakiet OpenSSH:** W pełni działający demon `sshd`, klient `ssh` oraz generator kluczy `ssh-keygen` — możliwość bezpośredniego logowania do SzpontOS z komputera-hosta!
- **Kryptografia i Bezpieczeństwo:** Port **OpenSSL** / **LibreSSL** (`libcrypto.so`, `libssl.so`).
- **Narzędzia Sieciowe:** Port biblioteki i narzędzia **cURL** (`libcurl.so`, `curl`), klient `git`, serwer WWW `httpd`, `ping`, `nc` (netcat), `ifconfig`, `host`.
- **Sterowniki Kart Sieciowych:** Natywne wsparcie dla **Intel E1000 (8254x)** oraz Realtek RTL8139, pełny stos TCP/IP z Berkeley Sockets.
- **Użytkownicy i Uprawnienia:** Baza kont `/etc/passwd` i `/etc/shadow`, polecenia `useradd`, `userdel`, `groupadd`, `su`, `id`, `whoami`.

### 7. Nowoczesny Podsystem Wejścia & USB
- **USB 3.0 (xHCI) & USB 2.0 (EHCI):** Sprzętowa inicjalizacja magistrali USB, slotów urządzeń, pierścieni transferowych i endpointów.
- **Sterownik USB HID:** Natywna obsługa klawiatur i myszy USB na współczesnym sprzęcie UEFI.
- **Kontroler i8042 PS/2:** Pełny sterownik klawiatury i myszy PS/2 ze wsparciem dla kółka przewijania (IntelliMouse / 4-bajtowe pakiety).
- **Podsystem evdev:** Zunifikowane węzły urządzeń `/dev/input/event*` oraz emulacja strumienia myszy `/dev/input/mice`.

### 8. Fundamenty Jądra i VFS
- **Higher-Half Kernel:** Baza jądra pod `0xffffffff80000000`, HHDM `0xffff800000000000+` z protokołem **Limine v8.x** (BIOS & UEFI).
- **Fast Syscalls:** Sprzętowe `syscall`/`sysretq` z zachowaniem konwencji System V AMD64 ABI (ponad 100 wywołań systemowych).
- **Pamięć i Paging:** 4-poziomowe stronicowanie z izolacją Ring 3, bezpieczny `usercopy` (zapobieganie atakom Ring 0), Copy-on-Write (`fork()`), dynamiczna sterta `brk()` i mapowania `mmap()`.
- **Systemy Plików:** Modularny VFS obsługujący `DevFS` (`/dev`), `ProcFS` (`/proc`), `SysFS` (`/sys`), `TmpFS` (`/tmp`, `/run`), `Initramfs` (USTAR) oraz `Ext2` z buforem blokowym (Buffer Cache z 256 kubełkami i polityką LRU).
- **In-Kernel Dynamic ELF Loader:** Automatyczne ładowanie i relokacja bibliotek współdzielonych `.so` pod przestrzenią `0x0000700000000000` oraz wsparcie dla `dlopen()` / `dlsym()`.
- **Moduły Jądra (.sko):** Ładowanie i usuwanie sterowników w czasie działania jądra (`insmod`, `lsmod`, `rmmod`).

---

## Skala Szpontu

Cały projekt uplasował się na zaszczytnym miejscu **"Szpont Kwantowy"** w oficjalnej Skali Szpontu:

<p align="center">
  <img src="artwork/szpont-scale.png" alt="Skala Szpontu" width="750" />
</p>

> **Diagnoza:** Poziom naszpocenia jądra osiągnął stan koherencji kwantowej. Wszystkie przerwania, wątki SMP, shadery Mesa 3D i pakiety sieciowe poruszają się po magistralach z maksymalną prędkością szpontu.

---

## Jak Naszponcić i Odpalić (Getting Started)

### Wymagania wstępne

- Kompilator: `x86_64-elf-gcc`, `x86_64-elf-ld`, `x86_64-elf-ar` (lub hostowy GCC na Linuxie)
- Asembler: `nasm`
- Generator ISO: `xorriso`
- Emulator: `qemu-system-x86_64` (wspiera KVM na Linuxie oraz Cocoa/Hypervisor na macOS)

#### Automatyczna konfiguracja na macOS:
```bash
./scripts/setup_macos.sh
```

#### Automatyczna konfiguracja na Linuxie (Ubuntu / Debian / Arch / Fedora / openSUSE / Alpine):
```bash
./scripts/setup_linux.sh
```

---

### Budowanie i Uruchamianie

Wszystkie komendy kompilacji i emulacji są zarządzane przez główny `Makefile`:

```bash
# 1. Zbuduj kompletny bootowalny obraz ISO ze wszystkimi bibliotekami
make iso

# 2. Uruchom SzpontOS z akceleracją Virtio-VGA (Zalecane dla X11 & Mesa 3D!)
make run-virtio

# 3. Uruchom w klasycznym oknie graficznym QEMU (standardowe VGA)
make run

# 4. Uruchom w trybie czystego USB 3.0 (xHCI, nowoczesne płyty główne UEFI bez PS/2)
make run-usb

# 5. Uruchom w trybie klasycznego sprzętu i8042 PS/2
make run-ps2

# 6. Uruchom w trybie tekstowym headless (konsola COM1 przekierowana do terminala)
make run-cli

# 7. Uruchom z aktywnym serwerem debugera GDB (port localhost:1234)
make debug

# 8. Wygeneruj bazę kompilacji compile_commands.json dla Clangd / IDE
make bear
```

---

### Dostęp Sieciowy z Komputera-Hosta (Port Forwarding)

Skrypt uruchomieniowy QEMU automatycznie konfiguruje przekierowanie portów:

- **SSH (OpenSSH):** Dostępne na porcie `localhost:2222` (lub kolejnym wolnym):
  ```bash
  ssh -p 2222 root@localhost
  ```
- **HTTP (Wbudowany serwer WWW):** Dostępny w przeglądarce pod adresem:
  ```
  http://localhost:8080/
  ```

---

## Struktura Katalogów

```
SzpontOS/
├── kernel/                      # Wyższe-Pół (Higher-Half) Jądro Systemu
│   ├── arch/x86_64/             # Inicjalizacja CPU: GDT, IDT, PIC, PIT, ISR, Syscall entry
│   ├── include/                 # Wewnętrzne nagłówki jądra (HAL, FS, MM, NET, SCHED)
│   │   └── uapi/                # Czyste nagłówki interfejsu jądro-użytkownik (UAPI)
│   └── src/                     # Implementacja jądra:
│       ├── drivers/             # DRM, FB, UART, USB (xHCI/EHCI/HID), PS/2, E1000, PCI, PTY, Evdev
│       ├── fs/                  # VFS, DevFS, ProcFS, SysFS, Ext2, TmpFS, Epoll, Eventfd, Inotify, ELF
│       ├── kernel/              # SMP bootstrap, IPI, LAPIC timer, kqueue
│       ├── mm/                  # PMM (bitmapa), VMM (4-level paging), Slab/Buddy, Usercopy
│       ├── net/                 # Stos TCP/IP, Ethernet, ARP, IPv4, ICMP, UDP, Berkeley Sockets
│       ├── sched/               # Wywłaszczający scheduler SMP, wątki, procesy, futex
│       └── syscall/             # Ponad 100 wywołań systemowych POSIX
│
├── libc/                        # Freestanding C Standard Library (libc.a, libc.so, libm.so, libdl.a)
│   ├── include/                 # 100% zgodne ze standardem POSIX / C17 / BSD nagłówki
│   └── src/                     # Implementacja stdio, stdlib, string, pthread, sockets, dlfcn, time
│
├── libdrm/                      # Natywna biblioteka DRM (libdrm.so, xf86drm, xf86drmMode, syncobj)
├── libgbm/                      # Natywna biblioteka Generic Buffer Management (libgbm.so)
│
├── third_party/                 # Przeportowane pakiety open-source wkomponowane w sysroot
│   ├── xorg/                    # Oficjalny serwer X.Org X11 z obsługą DRM/KMS i XKB
│   ├── mesa/                    # Mesa 3D (25.x): sterowniki Gallium, EGL, GLESv2, DRI3
│   ├── libstdc++/               # GNU C++ Standard Library runtime (libstdc++.so, libstdc++.a)
│   ├── openssh/                 # Pakiet OpenSSH (sshd, ssh, ssh-keygen)
│   ├── curl/                    # Biblioteka libcurl i narzędzie wiersza poleceń curl
│   ├── zsh/                     # Zaawansowana powłoka Z shell (/bin/zsh)
│   ├── nano/                    # Edytor tekstu GNU nano (/bin/nano)
│   ├── fastfetch/               # Narzędzie informacji o systemie (/bin/fastfetch)
│   ├── file/                    # Narzędzie libmagic i identyfikacja typów plików (/bin/file)
│   ├── ncurses/                 # Biblioteka konsolowa TUI (libncurses.a)
│   ├── zlib/                    # Biblioteka kompresji danych (libz.so)
│   ├── pixman/                  # Biblioteka operacji na pikselach (libpixman-1.so)
│   └── libX11/, libxcb/, ...    # Zestaw bibliotek klienckich X11 (libX11, libXext, libXrandr...)
│
├── userland/                    # Programy przestrzeni użytkownika
│   ├── init/                    # Proces PID 1 (zarządzanie rozruchem i demonami)
│   ├── sh/                      # Interaktywny shell Unixowy z historią i autouzupełnianiem
│   ├── bin/                     # Zestaw ponad 80 narzędzi i dem:
│   │                            # startx, szpontdesktop, szponterm, szpontlogin, glxgears, gltriangle,
│   │                            # donut, fastfetch, top, ping, ifconfig, httpd, ssh, curl, git, cpptest,
│   │                            # timerfdtest, signalfdtest, epolltest, kqueuetest, inotifytest, ...
│   └── skeleton/                # Statyczna struktura katalogów rootfs (/etc, /root, /home, /var...)
│
├── artwork/                     # Oficjalne grafiki, zrzuty ekranu i Skala Szpontu
├── scripts/                     # Skrypty instalacji, budowania obrazów, libstdc++ i launchery QEMU
├── limine.conf                  # Konfiguracja wieloarchitekturalnego bootloadera Limine
└── Makefile                     # Główny, równoległy system kompilacji DAG
```

---

## Przykładowe komendy w SzpontOS

Po uruchomieniu systemu w interaktywnej powłoce `/bin/sh` możesz przetestować nowe możliwości systemu:

### Środowisko Graficzne i Grafika 3D
```bash
# Uruchom pełne środowisko graficzne X11 ze Szpont Experience i SzponTerm:
startx

# Wewnątrz sesji graficznej (lub w terminalu) przetestuj akcelerację 3D:
glxgears          # Klasyczne wirujące koła zębate OpenGL
gltriangle        # Kolorowy trójkąt renderowany przez Mesa 3D
mesadrmtest       # Test bezpośredniego renderowania DRM/KMS
drmtest           # Diagnostyka węzłów /dev/dri/card0 i dumb bufferów
donut             # Kultowy pączek 3D obracający się w konsoli ASCII
```

### Diagnostyka i Podsystemy Kompatybilności
```bash
# Informacje o systemie i sprzęcie:
fastfetch
top               # Dynamiczny monitor procesów i obciążenia CPU
lspci             # Wykaz wykrytych urządzeń PCI (VGA, NIC, USB, pamięć masowa)
lsusb             # Wykryte kontrolery xHCI/EHCI oraz urządzenia USB HID
free              # Stan pamięci fizycznej PMM i sterty jądra
df                # Wykorzystanie zamontowanych systemów plików (Ext2, TmpFS)
dmesg             # Bufor logów startowych jądra

# Testy nowych mechanizmów Linuksa i C++:
cpptest           # Weryfikacja środowiska uruchomieniowego GNU libstdc++-v3
timerfdtest       # Test zegarów odliczających czas przez deskryptory plików
signalfdtest      # Test asynchronicznej obsługi sygnałów w pętli zdarzeń
epolltest         # Test multipleksacji I/O subsystemu epoll(7)
eventfdtest       # Test semaforów eventfd(2)
inotifytest       # Monitorowanie zdarzeń systemu plików w locie
```

### Sieć, Narzędzia i Zdalne Połączenie
```bash
# Sprawdź interfejsy i przetestuj łączność:
ifconfig
ping 10.0.2.2

# Uruchom wbudowany serwer HTTP:
httpd

# Pobierz stronę za pomocą cURL:
curl http://10.0.2.2:8080/

# Sprawdź repozytorium przez Git:
git status

# Zaloguj się przez SSH ze swojego komputera-hosta:
# (W osobnym oknie terminala na macOS/Linuxie)
ssh -p 2222 root@localhost
```

### Zarządzanie Modułami Jądra i Użytkownikami
```bash
# Załaduj i usuń moduł jądra .sko:
insmod /lib/modules/hello.sko
lsmod
rmmod hello

# Zarządzanie kontami użytkowników:
useradd testuser
id testuser
su - testuser
```

---

## Podziękowania

- Serdeczne podziękowania dla **Szpont Maszyny**, która pozwoliła zrealizować i naszponcić ten system operacyjny w C i Asemblerze.
- Podziękowania dla twórców projektów **X.Org**, **Mesa 3D**, **FreeBSD**, **Limine Bootloader**, **Vinix** oraz społeczności **OSDev.org** za inspirację architektoniczną i solidne fundamenty inżynierii systemowej.

## Licencja

Projekt SzpontOS jest udostępniany na warunkach otwartoźródłowej licencji [MIT](LICENSE).

---

*(C) Copyright by Szpont Industries. All rights reserved.*

