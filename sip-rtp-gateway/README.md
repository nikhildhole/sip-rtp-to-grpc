# SIP + RTP Gateway

A high-performance C++ media gateway acting as a SIP UAS to bridge RTP audio to a Python VoiceBot via gRPC.

## Layout

- `src/`: Core C++ source
- `config/`: Runtime configuration
- `proto/`: gRPC service definitions

## Dependencies

- gRPC C++
- Protobuf
- yaml-cpp
- CMake >= 3.15

## Build & Run

```bash
mkdir build
cd build
cmake ..
make -j4

# Run
./sip_rtp_gateway --config ../config/gateway.yaml
```
