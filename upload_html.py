#!/usr/bin/env python3
"""
ESP32 HTML/CSS Live Upload Tool
Uploads index.html and style.css to ESP32 via HTTP API (no USB required!)

Usage:
    python upload_html.py                    # Upload HTML once
    python upload_html.py --watch            # Watch for changes and auto-upload
    python upload_html.py --css              # Upload CSS once
    python upload_html.py --all              # Upload both HTML and CSS
    python upload_html.py --file custom.html # Upload specific file
"""

import requests
import time
import sys
import os
from pathlib import Path
from datetime import datetime

# Configuration
ESP32_IP = "192.168.1.81"
API_ENDPOINT = f"http://{ESP32_IP}/api/uploadFile"
DEFAULT_HTML = "data/index.html"
DEFAULT_CSS = "data/style.css"

def upload_file(file_path, target_name=None):
    """Upload a file to ESP32 via HTTP POST"""
    if not os.path.exists(file_path):
        print(f"❌ Error: File not found: {file_path}")
        return False
    
    # Determine target filename (default to original name)
    if target_name is None:
        target_name = os.path.basename(file_path)
    
    file_size = os.path.getsize(file_path)
    
    try:
        print(f"📤 Uploading {file_path} ({file_size} bytes) to ESP32...")
        start_time = time.time()
        
        with open(file_path, 'rb') as f:
            files = {'file': (target_name, f)}
            response = requests.post(API_ENDPOINT, files=files, timeout=10)
        
        elapsed = time.time() - start_time
        
        if response.status_code == 200:
            print(f"✅ Upload successful! ({elapsed:.2f}s)")
            print(f"🌐 View at: http://{ESP32_IP}/")
            return True
        else:
            print(f"❌ Upload failed: {response.status_code}")
            print(f"   Response: {response.text}")
            return False
            
    except requests.exceptions.ConnectionError:
        print(f"❌ Connection error: Cannot reach ESP32 at {ESP32_IP}")
        print(f"   Check that ESP32 is powered on and connected to WiFi")
        return False
    except Exception as e:
        print(f"❌ Upload error: {e}")
        return False

def watch_file(file_path):
    """Watch file for changes and auto-upload"""
    print(f"👀 Watching {file_path} for changes...")
    print(f"   Press Ctrl+C to stop")
    print()
    
    last_mtime = 0
    
    try:
        while True:
            try:
                current_mtime = os.path.getmtime(file_path)
                
                if current_mtime != last_mtime and last_mtime != 0:
                    print(f"\n🔄 Change detected at {datetime.now().strftime('%H:%M:%S')}")
                    upload_file(file_path)
                    print()
                
                last_mtime = current_mtime
                time.sleep(0.5)  # Check every 500ms
                
            except FileNotFoundError:
                print(f"⚠️  File deleted, waiting for it to reappear...")
                time.sleep(1)
                last_mtime = 0
                
    except KeyboardInterrupt:
        print(f"\n\n👋 Stopped watching")

def main():
    """Main entry point"""
    import argparse
    
    parser = argparse.ArgumentParser(
        description='Upload HTML/CSS files to ESP32 without USB cable',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python upload_html.py                    # Upload HTML once
  python upload_html.py --watch            # Auto-upload HTML on changes
  python upload_html.py --css              # Upload CSS once
  python upload_html.py --all              # Upload both HTML and CSS
  python upload_html.py --watch --css      # Watch CSS for changes
  python upload_html.py --file custom.html # Upload specific file
        """
    )
    
    parser.add_argument('--watch', '-w', action='store_true',
                        help='Watch file for changes and auto-upload')
    parser.add_argument('--file', '-f',
                        help=f'File to upload (overrides --css/--html)')
    parser.add_argument('--css', '-c', action='store_true',
                        help='Upload CSS file (style.css)')
    parser.add_argument('--all', '-a', action='store_true',
                        help='Upload both HTML and CSS')
    parser.add_argument('--ip', default=ESP32_IP,
                        help=f'ESP32 IP address (default: {ESP32_IP})')
    
    args = parser.parse_args()
    
    # Use provided IP
    target_ip = args.ip
    target_endpoint = f"http://{target_ip}/api/uploadFile"
    
    # Determine file(s) to upload
    if args.file:
        file_path = args.file
        files_to_upload = [file_path]
    elif args.all:
        files_to_upload = [DEFAULT_HTML, DEFAULT_CSS]
    elif args.css:
        files_to_upload = [DEFAULT_CSS]
    else:
        files_to_upload = [DEFAULT_HTML]
    
    print("=" * 60)
    print("  ESP32 HTML/CSS Live Upload Tool")
    print("=" * 60)
    print(f"  Target: {target_ip}")
    print(f"  Files:  {', '.join(files_to_upload)}")
    print("=" * 60)
    print()
    
    if args.watch:
        if len(files_to_upload) > 1:
            print("❌ Error: --watch only supports single file")
            print("   Use --watch with --css OR --html (default), not --all")
            sys.exit(1)
        watch_file_with_endpoint(files_to_upload[0], target_endpoint)
    else:
        all_success = True
        for file_path in files_to_upload:
            success = upload_file_with_endpoint(file_path, target_endpoint)
            if not success:
                all_success = False
            if len(files_to_upload) > 1:
                print()  # Spacing between multiple uploads
        sys.exit(0 if all_success else 1)

def upload_file_with_endpoint(file_path, endpoint, target_name=None):
    """Upload a file to ESP32 via HTTP POST using specific endpoint"""
    if not os.path.exists(file_path):
        print(f"❌ Error: File not found: {file_path}")
        return False
    
    # Determine target filename (default to original name)
    if target_name is None:
        target_name = os.path.basename(file_path)
    
    file_size = os.path.getsize(file_path)
    
    try:
        print(f"📤 Uploading {file_path} ({file_size} bytes) to ESP32...")
        start_time = time.time()
        
        with open(file_path, 'rb') as f:
            files = {'file': (target_name, f)}
            response = requests.post(endpoint, files=files, timeout=10)
        
        elapsed = time.time() - start_time
        
        if response.status_code == 200:
            print(f"✅ Upload successful! ({elapsed:.2f}s)")
            ip_match = endpoint.split("//")[1].split(":")[0] if "//" in endpoint else "ESP32"
            print(f"🌐 View at: http://{ip_match}/")
            return True
        else:
            print(f"❌ Upload failed: {response.status_code}")
            print(f"   Response: {response.text}")
            return False
            
    except requests.exceptions.ConnectionError:
        ip_match = endpoint.split("//")[1].split("/")[0] if "//" in endpoint else "ESP32"
        print(f"❌ Connection error: Cannot reach ESP32 at {ip_match}")
        print(f"   Check that ESP32 is powered on and connected to WiFi")
        return False
    except Exception as e:
        print(f"❌ Upload error: {e}")
        return False

def watch_file_with_endpoint(file_path, endpoint):
    """Watch file for changes and auto-upload using specific endpoint"""
    print(f"👀 Watching {file_path} for changes...")
    print(f"   Press Ctrl+C to stop")
    print()
    
    last_mtime = 0
    
    try:
        while True:
            try:
                current_mtime = os.path.getmtime(file_path)
                
                if current_mtime != last_mtime and last_mtime != 0:
                    print(f"\n🔄 Change detected at {datetime.now().strftime('%H:%M:%S')}")
                    upload_file_with_endpoint(file_path, endpoint)
                    print()
                
                last_mtime = current_mtime
                time.sleep(0.5)  # Check every 500ms
                
            except FileNotFoundError:
                print(f"⚠️  File deleted, waiting for it to reappear...")
                time.sleep(1)
                last_mtime = 0
                
    except KeyboardInterrupt:
        print(f"\n\n👋 Stopped watching")

if __name__ == '__main__':
    main()
