# File Structure

## Source Layout (`src/`)

| Path | Purpose |
| :--- | :--- |
| `main.cpp` | Entry point. Parses args and starts the app. |
| **`app/`** | Application infrastructure. |
| `app/GatewayApp` | Main application class. Initializes components and runs the main loop. |
| `app/Config` | Singleton wrapping `gateway.yaml` loading. |
| `app/Logger` | Thread-safe logging utility. |
| **`sip/`** | SIP Protocol handling. |
| `sip/SipServer` | UDP Socket manager for SIP (port 5060). |
| `sip/SipParser` | Parses raw bytes into `SipMessage` objects. |
| `sip/SipTransaction` | Tracks state of transactions (Try, Proceeding, Completed). |
| `sip/SipDialog` | Represents a confirmed dialog/call state. |
| **`rtp/`** | Real-time Transport Protocol. |
| `rtp/RtpServer` | Manages the dynamic pool of UDP ports for media (20000-30000). |
| `rtp/RtpPacket` | Zero-copy view of RTP header and payload. |
| `rtp/Dtmf4733` | Detects in-band DTMF digits (RFC 4733). |
| **`media/`** | Audio processing. |
| `media/MediaPipeline` | Orchestrates flow of audio chunks through stages. |
| `media/stages/` | Individual processors (Echo, gRPC Bridge, Recorder). |
| **`call/`** | Business Logic. |
| `call/CallSession` | The "Glue" class. Connects a SIP Dialog to an RTP Port and a Bot Client. |
| `call/CallRegistry` | Global lookup table for active calls. |
| **`grpc/`** | External Interface. |
| `grpc/VoiceBotClient` | Async wrapper around the generated gRPC stub. |

## Configuration (`config/`)

*   `gateway.yaml`: Runtime settings for ports, IPs, enabled features, and logging.

## Protocol Buffers (`proto/`)

*   `voicebot.proto`: Defines the API contract between the Gateway and the AI Bot.
