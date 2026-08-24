#!/usr/bin/env python3

"""
Deauther Dual-Band God Mode - Main Entry Point

This is the main entry point for our deauther system that orchestrates 
the embedded firmware operations with host-side development tools.

Usage:
    main.py [options]

Options:
    --help              Show this help message
    --ui                Start the web UI server (for mobile access)
    --god-mode          Enable God mode for advanced features  
    --verbose           Enable verbose output

Example:
    python main.py --ui --god-mode --verbose
"""

import argparse
import sys
import os
import time
import http.server
import socketserver
from pathlib import Path

# Add src to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'src'))

# Simple HTTP server class for serving the UI
class DeautherHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        # Serve from the UI directory
        self.directory = str(Path("ui").resolve())
        super().__init__(*args, directory=self.directory, **kwargs)
    
    def end_headers(self):
        # Add CORS headers to allow mobile access
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', '*')
        super().end_headers()
    
    def do_OPTIONS(self):
        self.send_response(200)
        self.end_headers()

def start_web_server(port=8080):
    """Start the mobile web UI server"""
    print(f"Starting web UI server on http://localhost:{port}")
    print("Access from any mobile device connected to same network")
    
    # Create and start HTTP server
    with socketserver.TCPServer(("", port), DeautherHTTPRequestHandler) as httpd:
        print(f"Server running at http://localhost:{port}/")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("Web server stopped")

def main():
    """Main entry point for the Deauther Dual-Band God Mode system"""
    
    parser = argparse.ArgumentParser(description='Deauther Dual-Band God Mode')
    
    parser.add_argument('--ui', action='store_true',
                   help='Start the web UI server for mobile access (for development/testing)')
    parser.add_argument('--god-mode', action='store_true',
                   help='Enable God mode for advanced features (activates comprehensive attack modes)')
    parser.add_argument('--verbose', action='store_true',
                   help='Enable verbose output for detailed system information')
    parser.add_argument('--interface', default='wlan0',
                   help='WiFi interface to use (default: wlan0)')
    parser.add_argument('--port', type=int, default=8080,
                   help='Port for web UI server (default: 8080) - used when --ui is enabled')
    
    args = parser.parse_args()
    
    print("=== Deauther Dual-Band God Mode ===")
    print(f"System running on development PC with embedded support")
    print(f"WiFi interface: {args.interface}")
    print(f"God Mode: {'Enabled' if args.god_mode else 'Disabled'}")
    print(f"Verbose: {'Enabled' if args.verbose else 'Disabled'}")
    
    # Handle UI mode - this serves the mobile web interface
    if args.ui:
        print("Starting development web UI server for mobile access...")
        print("Web UI will be accessible from any mobile device on network!")
        print(f"Open http://localhost:{args.port} in your browser")
        
        # Start the web server and let it run indefinitely
        start_web_server(args.port)
        
        # This code should never be reached due to serve_forever(),
        # but keeping it for consistency
        return 0
    else:
        print("\nSystem is running as development host tool")
        print("Ready to coordinate with embedded ESP32-C5 firmware")
        print("Use --ui flag to start web UI for mobile configuration")
        
        if args.god_mode:
            print("\nGod mode enabled - will coordinate with embedded system for comprehensive attacks")
            print("Scanning for available WiFi interfaces...")
            
        # Continuous operation for development tooling
        print("System ready. Press Ctrl+C to exit.")
        print("\nDevelopment host tools operational - ready to support embedded firmware operations")
        
        try:
            # This loop would normally check connection with the embedded device
            while True:
                time.sleep(1)
                if args.verbose:
                    print("System status: Development host active (ready for embedded communication)")
        except KeyboardInterrupt:
            print("\nExiting deauther development system...")
            return 0
    
    return 0

if __name__ == "__main__":
    sys.exit(main())