#!/usr/bin/env python3
"""Read and selectively extract CTR Nitro-Fueled IGA v13 PAK archives.

This is intentionally read-only with respect to the source archive.  It supports
the uncompressed, raw-deflate and LZMA block modes used by the PS4 archives.
"""

from __future__ import annotations

import argparse
import csv
import json
import lzma
import os
from pathlib import Path, PurePosixPath
import struct
import sys
import zlib


MAGIC = 0x1A414749
HEADER_SIZE = 56
BLOCK_SIZE = 0x8000


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def i32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<i", data, offset)[0]


def cstring(data: bytes, offset: int) -> tuple[str, int]:
    end = data.index(0, offset)
    return data[offset:end].decode("utf-8", errors="replace"), end + 1


def safe_relative_path(value: str, fallback: str) -> Path:
    value = value.replace("\\", "/")
    if len(value) >= 2 and value[1] == ":":
        value = value[2:]
    parts = [part for part in PurePosixPath(value).parts if part not in ("", ".", "/")]
    if not parts or any(part == ".." for part in parts):
        return Path(fallback)
    # Windows-invalid filename characters are replaced, while directory layout is kept.
    cleaned = []
    for part in parts:
        for char in '<>:"|?*':
            part = part.replace(char, "_")
        cleaned.append(part.rstrip(". ") or "_")
    return Path(*cleaned)


class Archive:
    def __init__(self, path: Path):
        self.path = path
        self.data = path.read_bytes()
        if len(self.data) < HEADER_SIZE or u32(self.data, 0) != MAGIC:
            raise ValueError(f"Not a little-endian IGA archive: {path}")

        self.version = u32(self.data, 4)
        self.file_count = u32(self.data, 12)
        self.sector_size = u32(self.data, 16)
        self.large_count = u32(self.data, 28)
        self.medium_count = u32(self.data, 32)
        self.small_count = u32(self.data, 36)
        self.names_offset = u32(self.data, 40)
        self.names_size = u32(self.data, 48)
        if self.version != 13:
            raise ValueError(f"Expected Nitro-Fueled IGA version 13, got {self.version}")

        ids_at = HEADER_SIZE
        descriptors_at = HEADER_SIZE + self.file_count * 4
        tables_at = HEADER_SIZE + self.file_count * 20
        self.large_table = [u32(self.data, tables_at + i * 4) for i in range(self.large_count)]
        tables_at += self.large_count * 4
        self.medium_table = [u16(self.data, tables_at + i * 2) for i in range(self.medium_count)]
        tables_at += self.medium_count * 2
        self.small_table = list(self.data[tables_at:tables_at + self.small_count])

        self.files = []
        for index in range(self.file_count):
            descriptor = descriptors_at + index * 16
            name_rel = u32(self.data, self.names_offset + index * 4)
            full_path, pos = cstring(self.data, self.names_offset + name_rel)
            short_path, _ = cstring(self.data, pos)
            self.files.append({
                "index": index,
                "id": u32(self.data, ids_at + index * 4),
                "offset": u32(self.data, descriptor),
                "ordinal": i32(self.data, descriptor + 4),
                "size": u32(self.data, descriptor + 8),
                "compression": u32(self.data, descriptor + 12),
                "full_path": full_path,
                "path": short_path,
            })

    def _block_info(self, file: dict, block_number: int) -> tuple[int, bool]:
        compression = file["compression"]
        index = (compression & 0x0FFFFFFF) + block_number
        size = file["size"]
        if size <= 0x7F * self.sector_size:
            table, mask, shift = self.small_table, 0x7F, 7
        elif size <= 0x7FFF * self.sector_size:
            table, mask, shift = self.medium_table, 0x7FFF, 15
        else:
            table, mask, shift = self.large_table, 0x7FFFFFFF, 31
        if index >= len(table):
            raise ValueError(f"Block-table index {index} is outside table ({len(table)})")
        entry = table[index]
        return (entry & mask) * self.sector_size, (entry >> shift) == 1

    def extract_bytes(self, file: dict) -> bytes:
        start = file["offset"]
        size = file["size"]
        compression = file["compression"]
        if compression == 0xFFFFFFFF:
            return self.data[start:start + size]

        source = self.data[start:start + size]
        output = bytearray(size)
        blocks = (size + BLOCK_SIZE - 1) // BLOCK_SIZE
        compression_type = compression >> 28
        for block_number in range(blocks):
            source_offset, is_compressed = self._block_info(file, block_number)
            output_offset = block_number * BLOCK_SIZE
            output_size = min(BLOCK_SIZE, size - output_offset)
            if not is_compressed:
                chunk = source[source_offset:source_offset + output_size]
            elif compression_type == 1:
                compressed_size = u32(source, source_offset)
                payload = source[source_offset + 4:source_offset + 4 + compressed_size]
                chunk = zlib.decompress(payload, wbits=-15)
            elif compression_type == 2:
                # IGA v13 stores a 32-bit compressed length; older IGA variants
                # and the NST-oriented parser use a 16-bit field here.
                compressed_size = u32(source, source_offset)
                properties = source[source_offset + 4:source_offset + 9]
                payload = source[source_offset + 9:source_offset + 9 + compressed_size]
                property_byte = properties[0]
                lc = property_byte % 9
                remainder = property_byte // 9
                lp = remainder % 5
                pb = remainder // 5
                dictionary_size = u32(properties, 1)
                filters = [{
                    "id": lzma.FILTER_LZMA1,
                    "dict_size": dictionary_size,
                    "lc": lc,
                    "lp": lp,
                    "pb": pb,
                }]
                # Game chunks have a known output size and may omit an LZMA end marker.
                decoder = lzma.LZMADecompressor(format=lzma.FORMAT_RAW, filters=filters)
                chunk = decoder.decompress(payload, max_length=output_size)
            else:
                raise ValueError(f"Unsupported compression type {compression_type}")
            if len(chunk) < output_size:
                raise ValueError(
                    f"Short block for {file['path']}: expected {output_size}, got {len(chunk)}"
                )
            output[output_offset:output_offset + output_size] = chunk[:output_size]
        return bytes(output)


def selected(files: list[dict], patterns: list[str], exclusions: list[str]) -> list[dict]:
    lowered = [pattern.lower() for pattern in patterns]
    excluded = [pattern.lower() for pattern in exclusions]
    results = []
    for file in files:
        haystacks = (file["path"].lower(), file["full_path"].lower())
        if lowered and not any(pattern in value for pattern in lowered for value in haystacks):
            continue
        if any(pattern in value for pattern in excluded for value in haystacks):
            continue
        results.append(file)
    return results


def write_manifest(archive: Archive, output: Path, fmt: str) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    metadata = {
        "archive": str(archive.path.resolve()),
        "version": archive.version,
        "file_count": archive.file_count,
        "sector_size": archive.sector_size,
        "files": archive.files,
    }
    if fmt == "json":
        output.write_text(json.dumps(metadata, indent=2, ensure_ascii=False), encoding="utf-8")
    else:
        with output.open("w", newline="", encoding="utf-8-sig") as handle:
            writer = csv.DictWriter(handle, fieldnames=archive.files[0].keys())
            writer.writeheader()
            writer.writerows(archive.files)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--list", action="store_true", help="Print matching archive entries")
    parser.add_argument("--manifest", type=Path, help="Write a CSV or JSON manifest")
    parser.add_argument("--extract-to", type=Path, help="Extract matching entries")
    parser.add_argument("--match", action="append", default=[], help="Case-insensitive path substring")
    parser.add_argument("--exclude", action="append", default=[], help="Case-insensitive path substring to omit")
    parser.add_argument("--limit", type=int, default=0, help="Maximum matching entries (0 = unlimited)")
    args = parser.parse_args()

    archive = Archive(args.archive)
    matches = selected(archive.files, args.match, args.exclude)
    if args.limit:
        matches = matches[:args.limit]

    if args.manifest:
        fmt = "json" if args.manifest.suffix.lower() == ".json" else "csv"
        write_manifest(archive, args.manifest, fmt)

    if args.list:
        for file in matches:
            print(f"{file['index']:5d} {file['size']:10d} 0x{file['compression']:08X} {file['path']}")

    if args.extract_to:
        root = args.extract_to.resolve()
        root.mkdir(parents=True, exist_ok=True)
        for number, file in enumerate(matches, 1):
            relative = safe_relative_path(file["path"], f"entry_{file['index']:05d}.bin")
            destination = (root / relative).resolve()
            if os.path.commonpath((str(root), str(destination))) != str(root):
                raise ValueError(f"Unsafe archive path: {file['path']}")
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(archive.extract_bytes(file))
            print(f"[{number}/{len(matches)}] {destination} ({file['size']} bytes)")

    print(
        f"Archive: {archive.path.name}; IGA v{archive.version}; "
        f"entries: {archive.file_count}; selected: {len(matches)}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, EOFError, lzma.LZMAError, zlib.error) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(1)
