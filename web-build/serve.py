#!/usr/bin/env python3
"""
Lupine Engine - Simple Web Server for Testing

This script starts a local HTTP server to test the web build.
WASM files require specific MIME types and CORS headers.

Usage:
    python serve.py              # Serve from build-web/bin
    python serve.py path/to/dir  # Serve from specified directory
    python serve.py --port 8000  # Use different port
"""

import http.server
import socketserver
import os
import sys
import argparse
import webbrowser
from functools import partial

class LupineHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    """HTTP handler with proper MIME types for WASM and CORS headers."""

    def __init__(self, *args, directory=None, **kwargs):
        self.directory = directory
        super().__init__(*args, directory=directory, **kwargs)

    extensions_map = {
        '': 'application/octet-stream',
        '.html': 'text/html',
        '.js': 'application/javascript',
        '.mjs': 'application/javascript',
        '.wasm': 'application/wasm',
        '.css': 'text/css',
        '.json': 'application/json',
        '.png': 'image/png',
        '.jpg': 'image/jpeg',
        '.jpeg': 'image/jpeg',
        '.gif': 'image/gif',
        '.svg': 'image/svg+xml',
        '.ico': 'image/x-icon',
        '.mp3': 'audio/mpeg',
        '.ogg': 'audio/ogg',
        '.wav': 'audio/wav',
        '.ttf': 'font/ttf',
        '.woff': 'font/woff',
        '.woff2': 'font/woff2',
        '.data': 'application/octet-stream',  # Emscripten data files
        '.mem': 'application/octet-stream',   # Memory init files
        '.symbols': 'text/plain',             # Debug symbols
    }

    def end_headers(self):
        # Add CORS headers for cross-origin requests
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', '*')
        # Cache control for development
        self.send_header('Cache-Control', 'no-cache, no-store, must-revalidate')
        super().end_headers()

    def do_OPTIONS(self):
        """Handle preflight requests."""
        self.send_response(200)
        self.end_headers()

    def log_message(self, format, *args):
        """Custom logging with colors."""
        method = args[0].split()[0] if args else ""
        status = args[1] if len(args) > 1 else ""

        # Color code by status
        if status.startswith('2'):
            color = '\033[92m'  # Green
        elif status.startswith('3'):
            color = '\033[93m'  # Yellow
        elif status.startswith('4') or status.startswith('5'):
            color = '\033[91m'  # Red
        else:
            color = '\033[0m'

        reset = '\033[0m'
        print(f"{color}[{method}]{reset} {format % args}")


def find_build_output():
    """Find the web build output directory."""
    script_dir = os.path.dirname(os.path.abspath(__file__))

    # Check common locations
    candidates = [
        os.path.join(script_dir, 'build-web', 'bin'),
        os.path.join(script_dir, 'build-web', 'bin', 'Release'),
        os.path.join(script_dir, 'build-web', 'bin', 'Debug'),
        os.path.join(script_dir, 'bin'),
    ]

    for path in candidates:
        if os.path.isdir(path):
            # Check for index.html
            if os.path.isfile(os.path.join(path, 'index.html')):
                return path

    return None


def main():
    parser = argparse.ArgumentParser(
        description='Lupine Engine Web Server for Testing',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python serve.py                    # Serve from auto-detected build directory
  python serve.py ./my-build         # Serve from specific directory
  python serve.py --port 3000        # Use port 3000
  python serve.py --no-browser       # Don't open browser automatically
        """
    )
    parser.add_argument('directory', nargs='?', help='Directory to serve (default: auto-detect)')
    parser.add_argument('--port', '-p', type=int, default=8080, help='Port to listen on (default: 8080)')
    parser.add_argument('--no-browser', action='store_true', help='Don\'t open browser automatically')
    parser.add_argument('--host', default='localhost', help='Host to bind to (default: localhost)')

    args = parser.parse_args()

    # Determine directory to serve
    if args.directory:
        serve_dir = os.path.abspath(args.directory)
    else:
        serve_dir = find_build_output()
        if not serve_dir:
            print("Error: Could not find web build output.")
            print("Run build-web.ps1 first, or specify a directory:")
            print("  python serve.py path/to/build")
            sys.exit(1)

    if not os.path.isdir(serve_dir):
        print(f"Error: Directory not found: {serve_dir}")
        sys.exit(1)

    # Check for index.html
    index_path = os.path.join(serve_dir, 'index.html')
    if not os.path.isfile(index_path):
        print(f"Warning: No index.html found in {serve_dir}")

    # Create handler with directory
    handler = partial(LupineHTTPRequestHandler, directory=serve_dir)

    # Start server
    url = f"http://{args.host}:{args.port}/"

    print()
    print("=" * 50)
    print("  Lupine Engine Web Server")
    print("=" * 50)
    print(f"  Directory: {serve_dir}")
    print(f"  URL:       {url}")
    print()
    print("  Press Ctrl+C to stop")
    print("=" * 50)
    print()

    # Open browser
    if not args.no_browser:
        webbrowser.open(url)

    # Start serving
    try:
        with socketserver.TCPServer((args.host, args.port), handler) as httpd:
            httpd.serve_forever()
    except KeyboardInterrupt:
        print("\n\nServer stopped.")
    except OSError as e:
        if e.errno == 98 or e.errno == 10048:  # Port in use (Linux/Windows)
            print(f"Error: Port {args.port} is already in use.")
            print(f"Try a different port: python serve.py --port {args.port + 1}")
        else:
            raise


if __name__ == '__main__':
    main()
