# SIP RTP Gateway - Run Guide

This guide explains how to compile, configure, and run the gateway.

## 1. First Time Setup

### Prerequisites
*   CMake (3.10+)
*   C++ Compiler (GCC/Clang supporting C++17)

### Build
1.  Navigate to the project root:
    ```bash
    cd sip-rtp-gateway
    ```
2.  Create a build directory:
    ```bash
    mkdir build
    cd build
    ```
3.  Configure and Build:
    ```bash
    cmake ..
    # Linux
    make -j$(nproc)
    
    # macOS
    make -j$(sysctl -n hw.ncpu)
    ```

### Run
From the `build` directory:
```bash
./sip_rtp_gateway
```

## 2. Re-Running After Changes

If you modify C++ source files (`.cpp` or `.h`):

1.  Go to the build directory:
    ```bash
    cd build
    ```
2.  Re-compile (only changed files will rebuild):
    ```bash
    # Linux
    make -j$(nproc)
    
    # macOS
    make -j$(sysctl -n hw.ncpu)
    ```
3.  Run again:
    ```bash
    ./sip_rtp_gateway
    ```

**Note**: You do NOT need to run `cmake ..` again unless you added new source files or changed `CMakeLists.txt`.

## 3. Configuration
The application reads from `config/gateway.yaml`.
*   You can pass a custom config path:
    ```bash
    ./sip_rtp_gateway --config /path/to/my_config.yaml
    ```

## 4. Verification
When running, you should see logs indicating workers starting:
```text
[INFO]  Initializing RtpServer with 16 workers...
[INFO]  RtpWorker 0 started [Ports 20000-21000]
...
```
This confirms the **Thread-per-Core** architecture is active.
