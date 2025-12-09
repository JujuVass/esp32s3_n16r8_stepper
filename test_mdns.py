#!/usr/bin/env python3
"""
mDNS vs IP Reliability Test Script
Alternates between mDNS (HTTP request) and direct IP ping every 5 seconds
Uses real HTTP requests to simulate browser behavior
"""

import socket
import time
import subprocess
import requests
from datetime import datetime

HOSTNAME = "esp32-stepper.local"
URL_MDNS = f"http://{HOSTNAME}/css/styles.css"
URL_IP = "http://192.168.1.81/css/styles.css"
EXPECTED_IP = "192.168.1.81"
INTERVAL = 5  # seconds

def test_http_mdns():
    """Test HTTP request via mDNS hostname (like browser)"""
    try:
        start = time.time()
        response = requests.get(URL_MDNS, timeout=5)
        elapsed = (time.time() - start) * 1000  # ms
        return response.status_code == 200, elapsed
    except Exception as e:
        return False, str(e)[:40]

def test_http_ip():
    """Test HTTP request via direct IP"""
    try:
        start = time.time()
        response = requests.get(URL_IP, timeout=5)
        elapsed = (time.time() - start) * 1000  # ms
        return response.status_code == 200, elapsed
    except Exception as e:
        return False, str(e)[:40]

def ping_ip(ip):
    """Ping IP address, return success and time"""
    try:
        start = time.time()
        # Windows ping: -n 1 = 1 packet, -w 1000 = 1s timeout
        result = subprocess.run(
            ['ping', '-n', '1', '-w', '1000', ip],
            capture_output=True,
            text=True,
            timeout=5
        )
        elapsed = (time.time() - start) * 1000  # ms
        return result.returncode == 0, elapsed
    except Exception as e:
        return False, str(e)

def main():
    print("=" * 65)
    print("  mDNS vs IP Reliability Test (HTTP)")
    print("=" * 65)
    print(f"  mDNS URL: {URL_MDNS}")
    print(f"  IP URL:   {URL_IP}")
    print(f"  Interval: {INTERVAL}s (alternating)")
    print("=" * 65)
    print()
    print("  Press Ctrl+C to stop and show summary")
    print()
    
    mdns_success = 0
    mdns_fail = 0
    mdns_total_time = 0
    
    ip_success = 0
    ip_fail = 0
    ip_total_time = 0
    
    test_mdns = True  # Alternate between mDNS and IP
    
    try:
        while True:
            timestamp = datetime.now().strftime('%H:%M:%S')
            
            if test_mdns:
                # Test HTTP via mDNS hostname
                success, elapsed = test_http_mdns()
                if success:
                    mdns_success += 1
                    if isinstance(elapsed, float):
                        mdns_total_time += elapsed
                    print(f"[{timestamp}] mDNS ✅ {HOSTNAME} ({elapsed:.0f}ms)")
                else:
                    mdns_fail += 1
                    print(f"[{timestamp}] mDNS ❌ FAILED: {elapsed}")
            else:
                # Test HTTP via direct IP
                success, elapsed = test_http_ip()
                if success:
                    ip_success += 1
                    if isinstance(elapsed, float):
                        ip_total_time += elapsed
                    print(f"[{timestamp}] IP   ✅ {EXPECTED_IP} ({elapsed:.0f}ms)")
                else:
                    ip_fail += 1
                    print(f"[{timestamp}] IP   ❌ FAILED: {elapsed}")
            
            test_mdns = not test_mdns  # Alternate
            time.sleep(INTERVAL)
            
    except KeyboardInterrupt:
        print()
        print()
        print("=" * 65)
        print("  Summary")
        print("=" * 65)
        
        # mDNS stats
        mdns_total = mdns_success + mdns_fail
        if mdns_total > 0:
            mdns_rate = (mdns_success / mdns_total) * 100
            print(f"  mDNS ({HOSTNAME}):")
            print(f"    Tests:   {mdns_total}")
            print(f"    Success: {mdns_success} ({mdns_rate:.1f}%)")
            print(f"    Failed:  {mdns_fail} ({100-mdns_rate:.1f}%)")
            if mdns_success > 0:
                print(f"    Avg:     {mdns_total_time/mdns_success:.0f}ms")
        
        print()
        
        # IP stats
        ip_total = ip_success + ip_fail
        if ip_total > 0:
            ip_rate = (ip_success / ip_total) * 100
            print(f"  IP Ping ({EXPECTED_IP}):")
            print(f"    Tests:   {ip_total}")
            print(f"    Success: {ip_success} ({ip_rate:.1f}%)")
            print(f"    Failed:  {ip_fail} ({100-ip_rate:.1f}%)")
            if ip_success > 0:
                print(f"    Avg:     {ip_total_time/ip_success:.0f}ms")
        
        print("=" * 65)

if __name__ == '__main__':
    main()
