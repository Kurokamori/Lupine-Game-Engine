#!/usr/bin/env python3
"""
Script to remove all logging macros from Lupine Engine codebase.
Handles both single-line and multi-line macro invocations.
"""

import os
import re
import argparse
from pathlib import Path

# All logging macros from Logger.hpp
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

def remove_log_statements(content):
    """
    Remove all logging macro invocations from the content.
    Handles both single-line and multi-line macros.
    """
    # Create regex pattern for all log macros
    macro_pattern = '|'.join(re.escape(macro) for macro in LOG_MACROS)
    
    # Pattern to match log macro calls (including multi-line)
    # Matches: LOG_XXX(...) where ... can span multiple lines
    pattern = rf'\b({macro_pattern})\s*\([^;]*?\);'
    
    # Remove all matches
    modified_content = re.sub(pattern, '', content, flags=re.DOTALL)
    
    # Clean up multiple consecutive blank lines (more than 2)
    modified_content = re.sub(r'\n\s*\n\s*\n+', '\n\n', modified_content)
    
    return modified_content

def process_file(file_path, dry_run=False):
    """
    Process a single file to remove log statements.
    Returns True if file was modified, False otherwise.
    """
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            original_content = f.read()
        
        modified_content = remove_log_statements(original_content)
        
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
        extensions = {'.cpp', '.hpp', '.h', '.cc', '.cxx'}
    
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

def main():
    parser = argparse.ArgumentParser(
        description='Remove all logging macros from Lupine Engine codebase'
    )
    parser.add_argument(
        'directories',
        nargs='+',
        help='Directories to process (e.g., core runtime engine)'
    )
    parser.add_argument(
        '--dry-run',
        action='store_true',
        help='Show what would be changed without modifying files'
    )
    parser.add_argument(
        '--extensions',
        nargs='+',
        default=['.cpp', '.hpp', '.h', '.cc', '.cxx'],
        help='File extensions to process (default: .cpp .hpp .h .cc .cxx)'
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

