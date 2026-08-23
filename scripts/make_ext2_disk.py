#!/usr/bin/env python3
"""
Generator obrazu dysku ext2 (32 MiB) dla SzpontOS.
Tworzy 100% poprawny, standardowy obraz systemu plików ext2.
(C) Copyright by Szpont Industries. All rights reserved.
"""

import struct
import sys
import os
import time

BLOCK_SIZE = 1024
BLOCKS_COUNT = 32768         # 32 MiB
INODES_COUNT = 8192
BLOCKS_PER_GROUP = 8192
INODES_PER_GROUP = 2048
INODE_SIZE = 128
GROUPS_COUNT = BLOCKS_COUNT // BLOCKS_PER_GROUP  # 4 groups

EXT2_MAGIC = 0xEF53
EXT2_S_IFDIR = 0x4000
EXT2_S_IFREG = 0x8000
EXT2_FT_REG_FILE = 1
EXT2_FT_DIR = 2

def make_ext2_image(output_path):
    print(f"[*] Tworzenie obrazu ext2: {output_path} (32 MiB, bloki: 1024B, inody: {INODES_COUNT})...")
    img = bytearray(BLOCKS_COUNT * BLOCK_SIZE)

    # 1. Superblock (Offset 1024, Block 1)
    s_first_data_block = 1
    s_log_block_size = 0  # 1024 << 0 = 1024
    s_blocks_per_group = BLOCKS_PER_GROUP
    s_inodes_per_group = INODES_PER_GROUP
    
    # We will allocate some blocks & inodes in Group 0
    # Group 0 layout:
    # Block 1: Superblock
    # Block 2: Group Descriptors
    # Block 3: Block Bitmap
    # Block 4: Inode Bitmap
    # Blocks 5..260: Inode Table (256 blocks = 2048 * 128B)
    # Block 261: Root Dir '/' data block
    # Block 262: '/test.txt' data block
    # Block 263: '/docs' dir data block
    # Block 264: '/docs/manual.txt' data block
    # Block 265: '/szpont.txt' data block
    
    used_blocks_g0 = 266  # 0..265 used
    free_blocks_g0 = BLOCKS_PER_GROUP - used_blocks_g0
    used_inodes_g0 = 14   # 1..14 used
    free_inodes_g0 = INODES_PER_GROUP - used_inodes_g0
    
    total_free_blocks = free_blocks_g0 + (GROUPS_COUNT - 1) * (BLOCKS_PER_GROUP - 261)
    total_free_inodes = free_inodes_g0 + (GROUPS_COUNT - 1) * (INODES_PER_GROUP)
    
    cur_time = int(time.time())
    
    sb_bytes = struct.pack(
        "<13I6H4I2H",
        INODES_COUNT,           # s_inodes_count
        BLOCKS_COUNT,           # s_blocks_count
        1000,                   # s_r_blocks_count
        total_free_blocks,      # s_free_blocks_count
        total_free_inodes,      # s_free_inodes_count
        s_first_data_block,     # s_first_data_block
        s_log_block_size,       # s_log_block_size
        0,                      # s_log_frag_size
        s_blocks_per_group,     # s_blocks_per_group
        s_blocks_per_group,     # s_frags_per_group
        s_inodes_per_group,     # s_inodes_per_group
        cur_time,               # s_mtime
        cur_time,               # s_wtime
        1,                      # s_mnt_count
        20,                     # s_max_mnt_count
        EXT2_MAGIC,             # s_magic (0xEF53)
        1,                      # s_state (clean)
        1,                      # s_errors
        0,                      # s_minor_rev_level
        cur_time,               # s_lastcheck
        86400 * 180,            # s_checkinterval
        0,                      # s_creator_os (Linux)
        1,                      # s_rev_level (Dynamic revision)
        0,                      # s_def_resuid
        0                       # s_def_resgid
    )
    
    # Extended superblock fields
    sb_ext = struct.pack(
        "<IH",
        11,                     # s_first_ino
        INODE_SIZE              # s_inode_size (128)
    )
    
    sb_full = sb_bytes + sb_ext
    img[1024:1024+len(sb_full)] = sb_full
    
    # 2. Block Group Descriptors (Block 2)
    # Group 0
    bg0 = struct.pack(
        "<IIIHHH",
        3,                      # bg_block_bitmap
        4,                      # bg_inode_bitmap
        5,                      # bg_inode_table
        free_blocks_g0,         # bg_free_blocks_count
        free_inodes_g0,         # bg_free_inodes_count
        2                       # bg_used_dirs_count ('/' and '/docs')
    )
    
    # Groups 1, 2, 3
    bgs = [bg0]
    for g in range(1, GROUPS_COUNT):
        bg_start = g * BLOCKS_PER_GROUP
        bg_desc = struct.pack(
            "<IIIHHH",
            bg_start,           # bg_block_bitmap
            bg_start + 1,       # bg_inode_bitmap
            bg_start + 2,       # bg_inode_table
            BLOCKS_PER_GROUP - 261,
            INODES_PER_GROUP,
            0
        )
        bgs.append(bg_desc)
        
    gdt_bytes = b"".join(bgs)
    img[2 * BLOCK_SIZE:2 * BLOCK_SIZE + len(gdt_bytes)] = gdt_bytes
    
    # 3. Block Bitmap (Block 3)
    # Mark blocks 0..265 as used in group 0
    for b in range(used_blocks_g0):
        byte_idx = (3 * BLOCK_SIZE) + (b // 8)
        bit_idx = b % 8
        img[byte_idx] |= (1 << bit_idx)
        
    # 4. Inode Bitmap (Block 4)
    # Mark inodes 1..14 as used in group 0 (1-indexed, bit 0 corresponds to Inode 1)
    for ino in range(1, used_inodes_g0 + 1):
        idx = ino - 1
        byte_idx = (4 * BLOCK_SIZE) + (idx // 8)
        bit_idx = idx % 8
        img[byte_idx] |= (1 << bit_idx)
        
    # 5. Inode Table (Blocks 5..260)
    inode_table_offset = 5 * BLOCK_SIZE
    
    def write_inode(ino, mode, uid, gid, size, block_ptrs):
        offset = inode_table_offset + (ino - 1) * INODE_SIZE
        blocks_512 = (len([b for b in block_ptrs if b != 0]) * BLOCK_SIZE) // 512
        
        # 15 block pointers
        ptrs = block_ptrs + [0] * (15 - len(block_ptrs))
        
        raw_inode = struct.pack(
            "<2H5I2H3I15I",
            mode,
            uid,
            size,
            cur_time,
            cur_time,
            cur_time,
            0,                  # i_dtime
            gid,
            1 if (mode & EXT2_S_IFDIR) == 0 else 2, # i_links_count
            blocks_512,
            0,                  # i_flags
            0,                  # i_osd1
            *ptrs[:15]
        )
        img[offset:offset+len(raw_inode)] = raw_inode

    # Inode 2: Root Directory '/'
    write_inode(2, EXT2_S_IFDIR | 0o755, 0, 0, 1024, [261])
    
    # Inode 11: '/test.txt'
    test_txt_data = b"Witaj w SzpontOS na partycji ext2!\n(C) Copyright by Szpont Industries.\nDysk dziala w trybie ATA LBA28.\n"
    write_inode(11, EXT2_S_IFREG | 0o644, 0, 0, len(test_txt_data), [262])
    
    # Inode 12: '/docs' Directory
    write_inode(12, EXT2_S_IFDIR | 0o755, 0, 0, 1024, [263])
    
    # Inode 13: '/docs/manual.txt'
    manual_data = b"SzpontOS Documentation\n======================\nExt2 driver supports direct and indirect block traversal, inodes, and permissions.\n"
    write_inode(13, EXT2_S_IFREG | 0o644, 1000, 1000, len(manual_data), [264])
    
    # Inode 14: '/szpont.txt'
    szpont_data = b"SzpontOS: Independent 64-bit Unix-like OS with ext2 storage!\n"
    write_inode(14, EXT2_S_IFREG | 0o644, 0, 0, len(szpont_data), [265])

    # 6. Data Blocks
    # Block 261: Root Directory '/' entries:
    # . (ino 2), .. (ino 2), test.txt (ino 11), docs (ino 12), szpont.txt (ino 14)
    root_entries = [
        (2, EXT2_FT_DIR, b"."),
        (2, EXT2_FT_DIR, b".."),
        (11, EXT2_FT_REG_FILE, b"test.txt"),
        (12, EXT2_FT_DIR, b"docs"),
        (14, EXT2_FT_REG_FILE, b"szpont.txt"),
    ]
    
    def build_dir_block(entries):
        buf = bytearray(BLOCK_SIZE)
        offset = 0
        for i, (ino, ftype, name) in enumerate(entries):
            is_last = (i == len(entries) - 1)
            rec_len = (BLOCK_SIZE - offset) if is_last else ((8 + len(name) + 3) & ~3)
            entry_bytes = struct.pack(
                "<IHBB",
                ino,
                rec_len,
                len(name),
                ftype
            ) + name
            buf[offset:offset+len(entry_bytes)] = entry_bytes
            offset += rec_len
        return buf

    img[261 * BLOCK_SIZE:262 * BLOCK_SIZE] = build_dir_block(root_entries)
    img[262 * BLOCK_SIZE:262 * BLOCK_SIZE + len(test_txt_data)] = test_txt_data
    
    # Block 263: '/docs' entries:
    # . (ino 12), .. (ino 2), manual.txt (ino 13)
    docs_entries = [
        (12, EXT2_FT_DIR, b"."),
        (2, EXT2_FT_DIR, b".."),
        (13, EXT2_FT_REG_FILE, b"manual.txt"),
    ]
    img[263 * BLOCK_SIZE:264 * BLOCK_SIZE] = build_dir_block(docs_entries)
    img[264 * BLOCK_SIZE:264 * BLOCK_SIZE + len(manual_data)] = manual_data
    img[265 * BLOCK_SIZE:265 * BLOCK_SIZE + len(szpont_data)] = szpont_data

    # Write out image file
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "wb") as f:
        f.write(img)
        
    print(f"[OK] Utworzono poprawnie obraz ext2: {output_path} ({len(img)} bajtow).")

if __name__ == "__main__":
    out_file = sys.argv[1] if len(sys.argv) > 1 else "build/disk.img"
    make_ext2_image(out_file)
