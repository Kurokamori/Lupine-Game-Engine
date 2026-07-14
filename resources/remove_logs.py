#!/usr/bin/env python3
"""
Script to remove all logging macros from Lupine Engine codebase.
Handles both single-line and multi-line macro invocations.
Supports C++ LOG_* macros and Python print/logging statements.
"""

import os
import re
import argparse
from pathlib import Path

# All logging macros from Logger.hpp (C++)
LOG_MACROS = [
    # Base macros
    'LOG_TRACE', 'LOG_DEBUG', 'LOG_INFO', 'LOG_WARN', 'LOG_ERROR', 'LOG_FATAL',
    # Core
    'LOG_CORE_TRACE', 'LOG_CORE_DEBUG', 'LOG_CORE_INFO', 'LOG_CORE_WARN', 'LOG_CORE_ERROR', 'LOG_CORE_FATAL',
    # ECS
    'LOG_ECS_TRACE', 'LOG_ECS_DEBUG', 'LOG_ECS_INFO', 'LOG_ECS_WARN', 'LOG_ECS_ERROR', 'LOG_ECS_FATAL',
    # Render
    'LOG_RENDER_TRACE', 'LOG_RENDER_DEBUG', 'LOG_RENDER_INFO', 'LOG_RENDER_WARN', 'LOG_RENDER_ERROR', 'LOG_RENDER_FATAL',
    # Audio
    'LOG_AUDIO_TRACE', 'LOG_AUDIO_DEBUG', 'LOG_AUDIO_INFO', 'LOG_AUDIO_WARN', 'LOG_AUDIO_ERROR', 'LOG_AUDIO_FATAL',
    # Physics
    'LOG_PHYSICS_TRACE', 'LOG_PHYSICS_DEBUG', 'LOG_PHYSICS_INFO', 'LOG_PHYSICS_WARN', 'LOG_PHYSICS_ERROR', 'LOG_PHYSICS_FATAL',
    # Asset
    'LOG_ASSET_TRACE', 'LOG_ASSET_DEBUG', 'LOG_ASSET_INFO', 'LOG_ASSET_WARN', 'LOG_ASSET_ERROR', 'LOG_ASSET_FATAL',
    # Scripting
    'LOG_SCRIPT_TRACE', 'LOG_SCRIPT_DEBUG', 'LOG_SCRIPT_INFO', 'LOG_SCRIPT_WARN', 'LOG_SCRIPT_ERROR', 'LOG_SCRIPT_FATAL',
    # Network
    'LOG_NETWORK_TRACE', 'LOG_NETWORK_DEBUG', 'LOG_NETWORK_INFO', 'LOG_NETWORK_WARN', 'LOG_NETWORK_ERROR', 'LOG_NETWORK_FATAL',
    # Input
    'LOG_INPUT_TRACE', 'LOG_INPUT_DEBUG', 'LOG_INPUT_INFO', 'LOG_INPUT_WARN', 'LOG_INPUT_ERROR', 'LOG_INPUT_FATAL',
    # UI
    'LOG_UI_TRACE', 'LOG_UI_DEBUG', 'LOG_UI_INFO', 'LOG_UI_WARN', 'LOG_UI_ERROR', 'LOG_UI_FATAL',
]

# Python logging functions to remove
PYTHON_LOG_FUNCTIONS = [
    # Standard logging module
    'logging.debug', 'logging.info', 'logging.warning', 'logging.error',
    'logging.critical', 'logging.exception', 'logging.log',
    # Logger instance methods (e.g., logger.debug)
    r'\w+\.debug', r'\w+\.info', r'\w+\.warning', r'\w+\.error',
    r'\w+\.critical', r'\w+\.exception',
]

def find_matching_paren(content, start_pos):
    """
    Find the position of the closing parenthesis that matches the opening one at start_pos.
    Handles nested parentheses, strings, and character literals.
    Returns the index of the closing ')' or -1 if not found.
    """
    if start_pos >= len(content) or content[start_pos] != '(':
        return -1

    depth = 1
    i = start_pos + 1
    in_string = False
    in_char = False
    string_char = None

    while i < len(content) and depth > 0:
        c = content[i]

        # Handle escape sequences
        if i > 0 and content[i-1] == '\\':
            i += 1
            continue

        # Handle string literals
        if not in_char and c in '"':
            if in_string and string_char == c:
                in_string = False
                string_char = None
            elif not in_string:
                in_string = True
                string_char = c

        # Handle character literals
        elif not in_string and c == "'":
            in_char = not in_char

        # Count parentheses only outside strings and char literals
        elif not in_string and not in_char:
            if c == '(':
                depth += 1
            elif c == ')':
                depth -= 1

        i += 1

    if depth == 0:
        return i - 1  # Position of the closing ')'
    return -1

def remove_cpp_log_statements(content):
    """
    Remove all C++ logging macro invocations from the content.
    Handles both single-line and multi-line macros with nested parentheses.
    """
    # Create regex pattern for all log macros
    macro_pattern = '|'.join(re.escape(macro) for macro in LOG_MACROS)
    pattern = re.compile(rf'\b({macro_pattern})\s*\(', re.DOTALL)

    result = []
    last_end = 0

    for match in pattern.finditer(content):
        # Find the opening parenthesis position
        paren_start = match.end() - 1  # Position of '('

        # Find the matching closing parenthesis
        paren_end = find_matching_paren(content, paren_start)

        if paren_end == -1:
            # Couldn't find matching paren, skip this match
            continue

        # Check for semicolon after the closing parenthesis
        after_paren = paren_end + 1
        # Skip whitespace
        while after_paren < len(content) and content[after_paren] in ' \t\n\r':
            after_paren += 1

        if after_paren < len(content) and content[after_paren] == ';':
            # Found a complete LOG statement, remove it
            result.append(content[last_end:match.start()])
            last_end = after_paren + 1  # Skip past the semicolon
        # else: not a complete statement, keep it

    # Append remaining content
    result.append(content[last_end:])

    return ''.join(result)

def remove_python_log_statements(content):
    """
    Remove Python print statements and logging calls.
    Handles single-line and multi-line statements.
    """
    lines = content.split('\n')
    result_lines = []
    skip_until_closed = False
    paren_count = 0

    for line in lines:
        stripped = line.strip()

        # If we're in the middle of a multi-line statement, continue skipping
        if skip_until_closed:
            paren_count += line.count('(') - line.count(')')
            if paren_count <= 0:
                skip_until_closed = False
                paren_count = 0
            continue

        # Check for print statements
        if re.match(r'^print\s*\(', stripped):
            paren_count = line.count('(') - line.count(')')
            if paren_count > 0:
                skip_until_closed = True
            continue

        # Check for logging.* calls at start of line
        if re.match(r'^logging\.(debug|info|warning|error|critical|exception|log)\s*\(', stripped):
            paren_count = line.count('(') - line.count(')')
            if paren_count > 0:
                skip_until_closed = True
            continue

        # Check for logger instance calls (e.g., logger.debug, self.logger.info)
        if re.match(r'^(self\.)?(\w+)\.(debug|info|warning|error|critical|exception)\s*\(', stripped):
            # Make sure it's actually a logger call, not something like "data.info"
            match = re.match(r'^(self\.)?(\w+)\.(debug|info|warning|error|critical|exception)\s*\(', stripped)
            if match:
                var_name = match.group(2)
                # Common logger variable names
                if var_name.lower() in ['logger', 'log', '_logger', '_log', 'logging']:
                    paren_count = line.count('(') - line.count(')')
                    if paren_count > 0:
                        skip_until_closed = True
                    continue

        result_lines.append(line)

    return '\n'.join(result_lines)

def remove_log_statements(content, file_ext):
    """
    Remove all logging invocations from the content based on file type.
    """
    if file_ext == '.py':
        modified_content = remove_python_log_statements(content)
    else:
        modified_content = remove_cpp_log_statements(content)

    # Clean up multiple consecutive blank lines (more than 2)
    modified_content = re.sub(r'\n\s*\n\s*\n+', '\n\n', modified_content)

    return modified_content

def process_file(file_path, dry_run=False):
    """
    Process a single file to remove log statements.
    Returns True if file was modified, False otherwise.
    """
    try:
        file_ext = Path(file_path).suffix.lower()
        with open(file_path, 'r', encoding='utf-8') as f:
            original_content = f.read()

        modified_content = remove_log_statements(original_content, file_ext)

        if original_content != modified_content:
            if not dry_run:
                with open(file_path, 'w', encoding='utf-8') as f:
                    f.write(modified_content)
                print(f"✓ Modified: {file_path}")
            else:
                print(f"[DRY RUN] Would modify: {file_path}")
            return True
        else:
            return False
    except Exception as e:
        print(f"✗ Error processing {file_path}: {e}")
        return False

def process_directory(directory, extensions=None, dry_run=False):
    """
    Recursively process all files in a directory.
    """
    if extensions is None:
        extensions = {'.cpp', '.hpp', '.h', '.cc', '.cxx', '.py'}
    
    directory = Path(directory)
    if not directory.exists():
        print(f"Error: Directory '{directory}' does not exist")
        return 0, 0
    
    files_processed = 0
    files_modified = 0
    
    for file_path in directory.rglob('*'):
        if file_path.is_file() and file_path.suffix in extensions:
            files_processed += 1
            if process_file(file_path, dry_run):
                files_modified += 1
    
    return files_processed, files_modified

# Default directories to process
DEFAULT_DIRECTORIES = ['core', 'engine', 'runtime', 'editor']

def main():
    parser = argparse.ArgumentParser(
        description='Remove all logging macros from Lupine Engine codebase (C++ and Python)'
    )
    parser.add_argument(
        'directories',
        nargs='*',
        default=DEFAULT_DIRECTORIES,
        help=f'Directories to process (default: {" ".join(DEFAULT_DIRECTORIES)})'
    )
    parser.add_argument(
        '--dry-run',
        action='store_true',
        help='Show what would be changed without modifying files'
    )
    parser.add_argument(
        '--extensions',
        nargs='+',
        default=['.cpp', '.hpp', '.h', '.cc', '.cxx', '.py'],
        help='File extensions to process (default: .cpp .hpp .h .cc .cxx .py)'
    )
    
    args = parser.parse_args()
    
    print("=" * 60)
    print("Lupine Engine - Log Statement Remover")
    print("=" * 60)
    if args.dry_run:
        print("DRY RUN MODE - No files will be modified")
        print("=" * 60)
    
    total_processed = 0
    total_modified = 0
    
    extensions = set(args.extensions)
    
    for directory in args.directories:
        print(f"\nProcessing directory: {directory}")
        print("-" * 60)
        processed, modified = process_directory(directory, extensions, args.dry_run)
        total_processed += processed
        total_modified += modified
        print(f"Files processed: {processed}, Files modified: {modified}")
    
    print("\n" + "=" * 60)
    print(f"Total files processed: {total_processed}")
    print(f"Total files modified: {total_modified}")
    print("=" * 60)
    
    if args.dry_run:
        print("\nThis was a DRY RUN. Run without --dry-run to apply changes.")

if __name__ == '__main__':
    main()

