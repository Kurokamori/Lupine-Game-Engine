"""
Lupine Engine Pack File Format

This module implements the .pck pack file format for bundling game assets.
Compatible with the C++ PackFile implementation in core/platform/PackFile.hpp

File structure:
- Header (32 bytes):
  - Magic number: "LPCK" (4 bytes)
  - Version: uint32_t (4 bytes)
  - Flags: uint32_t (4 bytes)
  - File count: uint32_t (4 bytes)
  - Index offset: uint64_t (8 bytes)
  - Data offset: uint64_t (8 bytes)

- Data section:
  - Concatenated file data (compressed or raw)

- Index section:
  - For each file:
    - Path length: uint32_t
    - Path: UTF-8 string (without null terminator)
    - Data offset: uint64_t
    - Compressed size: uint64_t
    - Uncompressed size: uint64_t
    - Flags: uint32_t
    - CRC32: uint32_t
"""

import struct
import zlib
import os
from pathlib import Path
from typing import Dict, List, Optional, BinaryIO, Tuple
from dataclasses import dataclass
import io


# Constants matching C++ implementation
PACK_MAGIC = 0x4B43504C  # "LPCK" in little-endian
PACK_VERSION = 1

# Entry flags
FLAG_NONE = 0
FLAG_COMPRESSED = 1 << 0
FLAG_ENCRYPTED = 1 << 1
FLAG_BINARY = 1 << 2

# Header struct format (little-endian)
HEADER_FORMAT = '<IIIIQQ'  # magic, version, flags, file_count, index_offset, data_offset
HEADER_SIZE = 32


@dataclass
class PackEntry:
    """Represents a file entry in a pack file"""
    path: str
    data_offset: int = 0
    compressed_size: int = 0
    uncompressed_size: int = 0
    flags: int = 0
    crc32: int = 0


class PackFileWriter:
    """Creates pack files for exporting games"""

    def __init__(self):
        self.entries: List[PackEntry] = []
        self.entry_paths: set = set()
        self.data_buffer: bytearray = bytearray()
        self.compression_enabled: bool = True
        self.compression_level: int = 9

    def set_compression(self, enabled: bool, level: int = 9):
        """Enable/disable compression for subsequent files"""
        self.compression_enabled = enabled
        self.compression_level = max(1, min(9, level))

    def add_file(self, pack_path: str, source_path: str) -> bool:
        """
        Add a file from disk to the pack

        Args:
            pack_path: Path inside the pack file (e.g., "assets/texture.png")
            source_path: Path to the source file on disk

        Returns:
            True if successful
        """
        try:
            with open(source_path, 'rb') as f:
                data = f.read()
            return self.add_data(pack_path, data)
        except Exception as e:
            print(f"Failed to add file {source_path}: {e}")
            return False

    def add_data(self, pack_path: str, data: bytes) -> bool:
        """
        Add raw data to the pack

        Args:
            pack_path: Path inside the pack file
            data: Binary data to add

        Returns:
            True if successful
        """
        try:
            # Normalize path separators
            normalized_path = pack_path.replace('\\', '/')

            # Several passes overlap (a .json under scenes/ is reachable from both the
            # scenes pass and the project-wide resource sweep). Entries are appended, and
            # the reader resolves a path to whichever entry it finds, so a second copy is
            # dead weight in the pack at best and a stale shadow at worst.
            if normalized_path in self.entry_paths:
                return True

            # Calculate CRC32
            crc = zlib.crc32(data) & 0xFFFFFFFF

            # Compress if enabled and worthwhile
            write_data = data
            flags = FLAG_NONE

            if self.compression_enabled and len(data) > 64:
                compressed = zlib.compress(data, self.compression_level)
                if len(compressed) < len(data):
                    write_data = compressed
                    flags |= FLAG_COMPRESSED

            # Create entry
            entry = PackEntry(
                path=normalized_path,
                data_offset=len(self.data_buffer),
                compressed_size=len(write_data),
                uncompressed_size=len(data),
                flags=flags,
                crc32=crc
            )

            self.entries.append(entry)
            self.entry_paths.add(normalized_path)
            self.data_buffer.extend(write_data)

            return True

        except Exception as e:
            print(f"Failed to add data for {pack_path}: {e}")
            return False

    def add_string(self, pack_path: str, content: str, encoding: str = 'utf-8') -> bool:
        """Add string data to the pack"""
        return self.add_data(pack_path, content.encode(encoding))

    def add_directory(self, pack_base_path: str, source_dir: str,
                      extensions: Optional[List[str]] = None,
                      exclude_patterns: Optional[List[str]] = None) -> int:
        """
        Recursively add all files from a directory

        Args:
            pack_base_path: Base path in pack (e.g., "assets")
            source_dir: Source directory on disk
            extensions: Optional list of extensions to include (e.g., ['.png', '.json'])
            exclude_patterns: Optional list of patterns to exclude

        Returns:
            Number of files added
        """
        count = 0
        source_path = Path(source_dir)

        if not source_path.exists():
            print(f"Source directory does not exist: {source_dir}")
            return 0

        exclude_patterns = exclude_patterns or []

        for file_path in source_path.rglob('*'):
            if not file_path.is_file():
                continue

            # Check extensions
            if extensions and file_path.suffix.lower() not in extensions:
                continue

            # Check exclusions
            relative_path = file_path.relative_to(source_path)
            skip = False
            for pattern in exclude_patterns:
                if pattern in str(relative_path):
                    skip = True
                    break
            if skip:
                continue

            # Build pack path
            pack_path = f"{pack_base_path}/{relative_path}".replace('\\', '/')

            if self.add_file(pack_path, str(file_path)):
                count += 1

        return count

    def write(self, output_path: str) -> bool:
        """
        Write the pack file to disk

        Args:
            output_path: Path for the output .pck file

        Returns:
            True if successful
        """
        try:
            with open(output_path, 'wb') as f:
                # Write placeholder header
                header_data = struct.pack(HEADER_FORMAT,
                    PACK_MAGIC, PACK_VERSION, 0, len(self.entries), 0, 0)
                # Pad to 32 bytes
                header_data += b'\x00' * (HEADER_SIZE - len(header_data))
                f.write(header_data)

                # Record data offset
                data_offset = f.tell()

                # Write data section
                f.write(self.data_buffer)

                # Record index offset
                index_offset = f.tell()

                # Write index section
                for entry in self.entries:
                    path_bytes = entry.path.encode('utf-8')

                    # Write path length and path
                    f.write(struct.pack('<I', len(path_bytes)))
                    f.write(path_bytes)

                    # Write entry data
                    f.write(struct.pack('<QQQ',
                        entry.data_offset,
                        entry.compressed_size,
                        entry.uncompressed_size))
                    f.write(struct.pack('<II', entry.flags, entry.crc32))

                # Update header with correct offsets
                f.seek(0)
                header_data = struct.pack(HEADER_FORMAT,
                    PACK_MAGIC, PACK_VERSION, 0, len(self.entries),
                    index_offset, data_offset)
                header_data += b'\x00' * (HEADER_SIZE - len(header_data))
                f.write(header_data)

            print(f"Pack file created: {output_path} ({len(self.entries)} files)")
            return True

        except Exception as e:
            print(f"Failed to write pack file: {e}")
            return False

    def get_total_size(self) -> int:
        """Get the total size of all data (compressed)"""
        return len(self.data_buffer)

    def get_file_count(self) -> int:
        """Get the number of files in the pack"""
        return len(self.entries)

    def clear(self):
        """Clear all entries and reset the writer"""
        self.entries.clear()
        self.entry_paths.clear()
        self.data_buffer.clear()


class PackFileReader:
    """Reads pack files for asset extraction and verification"""

    def __init__(self):
        self.entries: Dict[str, PackEntry] = {}
        self.file_handle: Optional[BinaryIO] = None
        self.data_offset: int = 0
        self.is_open: bool = False
        self.path: str = ""

    def open(self, path: str) -> bool:
        """
        Open a pack file for reading

        Args:
            path: Path to the .pck file

        Returns:
            True if successful
        """
        try:
            self.close()

            self.file_handle = open(path, 'rb')
            self.path = path

            # Read header
            header_data = self.file_handle.read(HEADER_SIZE)
            if len(header_data) < 24:  # Minimum header size
                raise ValueError("File too small for header")

            magic, version, flags, file_count, index_offset, data_offset = \
                struct.unpack(HEADER_FORMAT, header_data[:24])

            if magic != PACK_MAGIC:
                raise ValueError(f"Invalid magic number: expected LPCK")

            if version > PACK_VERSION:
                raise ValueError(f"Unsupported version: {version}")

            self.data_offset = data_offset

            # Read index
            self.file_handle.seek(index_offset)

            for _ in range(file_count):
                # Read path length
                path_len_data = self.file_handle.read(4)
                path_len = struct.unpack('<I', path_len_data)[0]

                # Read path
                path = self.file_handle.read(path_len).decode('utf-8')

                # Read entry data
                entry_data = self.file_handle.read(8 * 3 + 4 * 2)  # 3 uint64 + 2 uint32
                data_off, comp_size, uncomp_size = struct.unpack('<QQQ', entry_data[:24])
                entry_flags, crc = struct.unpack('<II', entry_data[24:])

                entry = PackEntry(
                    path=path,
                    data_offset=data_off,
                    compressed_size=comp_size,
                    uncompressed_size=uncomp_size,
                    flags=entry_flags,
                    crc32=crc
                )

                self.entries[path] = entry

            self.is_open = True
            return True

        except Exception as e:
            print(f"Failed to open pack file: {e}")
            self.close()
            return False

    def close(self):
        """Close the pack file"""
        if self.file_handle:
            self.file_handle.close()
            self.file_handle = None
        self.entries.clear()
        self.is_open = False
        self.path = ""

    def exists(self, path: str) -> bool:
        """Check if a file exists in the pack"""
        normalized = path.replace('\\', '/')
        return normalized in self.entries

    def list_files(self) -> List[str]:
        """Get list of all files in the pack"""
        return list(self.entries.keys())

    def read_file(self, path: str) -> Optional[bytes]:
        """
        Read a file from the pack

        Args:
            path: Path inside the pack

        Returns:
            File contents as bytes, or None if not found
        """
        if not self.is_open or not self.file_handle:
            return None

        normalized = path.replace('\\', '/')
        entry = self.entries.get(normalized)

        if not entry:
            return None

        try:
            # Seek to data
            self.file_handle.seek(self.data_offset + entry.data_offset)
            data = self.file_handle.read(entry.compressed_size)

            # Decompress if needed
            if entry.flags & FLAG_COMPRESSED:
                data = zlib.decompress(data)

            # Verify CRC
            actual_crc = zlib.crc32(data) & 0xFFFFFFFF
            if actual_crc != entry.crc32:
                print(f"CRC mismatch for {path}")
                return None

            return data

        except Exception as e:
            print(f"Failed to read {path} from pack: {e}")
            return None

    def read_file_as_string(self, path: str, encoding: str = 'utf-8') -> Optional[str]:
        """Read a file as a string"""
        data = self.read_file(path)
        if data:
            return data.decode(encoding)
        return None

    def extract_file(self, pack_path: str, output_path: str) -> bool:
        """
        Extract a file from the pack to disk

        Args:
            pack_path: Path inside the pack
            output_path: Destination path on disk

        Returns:
            True if successful
        """
        data = self.read_file(pack_path)
        if data is None:
            return False

        try:
            os.makedirs(os.path.dirname(output_path), exist_ok=True)
            with open(output_path, 'wb') as f:
                f.write(data)
            return True
        except Exception as e:
            print(f"Failed to extract {pack_path}: {e}")
            return False

    def extract_all(self, output_dir: str) -> int:
        """
        Extract all files from the pack

        Args:
            output_dir: Destination directory

        Returns:
            Number of files extracted
        """
        count = 0
        for path in self.entries:
            output_path = os.path.join(output_dir, path)
            if self.extract_file(path, output_path):
                count += 1
        return count

    def get_entry(self, path: str) -> Optional[PackEntry]:
        """Get entry info for a file"""
        normalized = path.replace('\\', '/')
        return self.entries.get(normalized)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False


def append_pack_to_executable(exe_path: str, pack_path: str, output_path: str) -> bool:
    """
    Append a pack file to an executable, creating an embedded pack

    The pack data is appended to the end of the executable, followed by
    a trailer containing:
    - Magic marker "PLCK" (4 bytes)
    - Pack data offset (8 bytes)

    This matches the C++ runtime's checkEmbeddedPack() which reads the marker first.

    Args:
        exe_path: Path to the template executable
        pack_path: Path to the pack file
        output_path: Path for the output executable

    Returns:
        True if successful
    """
    EMBEDDED_MARKER = 0x4B434C50  # "PLCK"

    try:
        # Read executable
        with open(exe_path, 'rb') as f:
            exe_data = f.read()

        # Read pack file
        with open(pack_path, 'rb') as f:
            pack_data = f.read()

        # Calculate pack offset (where pack data starts in final file)
        pack_offset = len(exe_data)

        # Create trailer: marker first (4 bytes), then offset (8 bytes)
        # This matches C++ checkEmbeddedPack() which reads marker then offset
        trailer = struct.pack('<IQ', EMBEDDED_MARKER, pack_offset)

        # Write combined file
        with open(output_path, 'wb') as f:
            f.write(exe_data)
            f.write(pack_data)
            f.write(trailer)

        print(f"Created embedded executable: {output_path}")
        return True

    except Exception as e:
        print(f"Failed to create embedded executable: {e}")
        return False
