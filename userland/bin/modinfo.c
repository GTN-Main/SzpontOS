/*
 * SzpontOS - modinfo (Show Information About a Kernel Module)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

typedef struct {
    uint8_t e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} Elf64_Shdr;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <module.sko>\n", argv[0]);
        return 1;
    }

    const char *path = argv[1];
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("modinfo: open failed");
        return 1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("modinfo: fstat failed");
        close(fd);
        return 1;
    }

    size_t size = st.st_size;
    void *buf = malloc(size);
    if (!buf) {
        fprintf(stderr, "modinfo: out of memory\n");
        close(fd);
        return 1;
    }

    read(fd, buf, size);
    close(fd);

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)buf;
    if (memcmp(ehdr->e_ident,
               "\x7F"
               "ELF",
               4) != 0 ||
        ehdr->e_ident[4] != 2) {
        fprintf(stderr, "modinfo: '%s' is not a valid 64-bit ELF object\n", path);
        free(buf);
        return 1;
    }

    printf("filename:       %s\n", path);

    Elf64_Shdr *shdrs = (Elf64_Shdr *)((uintptr_t)buf + ehdr->e_shoff);
    const char *shstrtab = (const char *)((uintptr_t)buf + shdrs[ehdr->e_shstrndx].sh_offset);

    int found_modinfo = 0;
    for (size_t i = 0; i < ehdr->e_shnum; i++) {
        const char *name = shstrtab + shdrs[i].sh_name;
        if (strcmp(name, ".modinfo") == 0) {
            found_modinfo = 1;
            const char *info = (const char *)((uintptr_t)buf + shdrs[i].sh_offset);
            size_t info_len = shdrs[i].sh_size;
            size_t pos = 0;

            while (pos < info_len) {
                const char *entry = info + pos;
                size_t entry_len = strlen(entry);
                if (entry_len == 0) {
                    pos++;
                    continue;
                }

                char key[64] = {0};
                char val[256] = {0};
                const char *eq = strchr(entry, '=');
                if (eq) {
                    size_t klen = eq - entry;
                    if (klen < sizeof(key)) {
                        strncpy(key, entry, klen);
                        strncpy(val, eq + 1, sizeof(val) - 1);
                        printf("%-15s %s\n", key, val);
                    }
                }
                pos += entry_len + 1;
            }
        }
    }

    if (!found_modinfo) {
        printf("description:    (no .modinfo metadata section)\n");
    }

    free(buf);
    return 0;
}
