# High Concurrency Deployment Guide

This guide details the OS-level tuning required to run the `sip-rtp-gateway` at high scale (10,000+ concurrent calls) on Linux (Ubuntu/RHEL).

## 1. Increase File Descriptors
Each concurrent call uses at least 2 file descriptors (1 UDP for RTP, 1 TCP for gRPC).
*   **Default**: 1024 (Limits you to ~500 calls).
*   **Target**: 65535 or higher.

**Temporary (Current Session):**
```bash
ulimit -n 65535
```

**Permanent:**
Edit `/etc/security/limits.conf`:
```conf
* hard nofile 65535
* soft nofile 65535
root hard nofile 65535
root soft nofile 65535
```

## 2. Kernel Network Tuning
VoIP generates massive Packet Per Second (PPS) load. You must increase kernel buffers to prevent packet drops.

Create/Edit `/etc/sysctl.conf`:
```conf
# Increase max OS receive buffer size to 25MB
net.core.rmem_max = 26214400
net.core.wmem_max = 26214400

# Set default buffer size
net.core.rmem_default = 26214400
net.core.wmem_default = 26214400

# Increase backlog for incoming packets
net.core.netdev_max_backlog = 5000

# Increase Port Range
net.ipv4.ip_local_port_range = 10000 65000
```

Apply changes:
```bash
sudo sysctl -p
```

## 3. Application Configuration
Ensure your `gateway.yaml` is configured to use all available resources.

```yaml
# ...
max_calls: 10000  # Unlock software limit
rtp_port_start: 20000
rtp_port_end: 40000 # Needs 2 ports per call (even number start)
# ...
```

## 4. Hardware Recommendations (10k Calls)
*   **CPU**: 16+ Cores (E.g., AWS c5.4xlarge).
*   **RAM**: 16 GB+ (VoIP is light on RAM, but safe buffer is good).
*   **Network**: 10 Gbps Interface (Standard 1Gbps nic will saturate at ~870 Mbps).
