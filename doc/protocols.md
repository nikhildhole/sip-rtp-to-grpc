# Protocol Reference

## 1. SIP (Signaling)

The gateway acts as a **SIP UAS** (User Agent Server).

| Method | Behavior |
| :--- | :--- |
| `INVITE` | Accepts audio calls. Negotiates PCMU/PCMA. |
| `ACK` | Confirms connection. |
| `BYE` | Terminates call. |
| `CANCEL` | Cancels pending INVITE. |
| `REFER` | Blind transfer support (accepts, notifies, ends). |
| `OPTIONS` | Health check (responds 200 OK). |

### Responses
*   `100 Trying`: Immediate response to INVITE.
*   `200 OK`: SDP Answer with selected codec.
*   `486 Busy Here`: When `max_calls` limit is reached.
*   `488 Not Acceptable Here`: No matching codec found.

## 2. SDP (Negotiation)

Offers are accepted if they contain:
*   `m=audio` line.
*   Supported codecs: **PCMU/8000** (PT=0), **PCMA/8000** (PT=8).
*   Telephone Events: **telephone-event/8000** (RFC 4733).

Answer logic:
1.  Prefer PCMU if offered.
2.  Else PCMA.
3.  Reject if neither.

## 3. gRPC (Voice Bot Integration)

The `VoiceBot` service uses bidirectional streaming.

### Service Definition
```protobuf
service VoiceBot {
    rpc StreamCall (stream CallEvent) returns (stream CallAction);
}
```

### Gateway -> Bot Messages (`CallEvent`)
1.  **Config**: Sent immediately on connect. Contains Sample Rate (8000) and Codec (PCMU/A).
2.  **AudioChunk**: Raw PCM bytes (usually 20ms chunks).
3.  **DtmfEvent**: Digit detected (`0-9`, `*`, `#`).
4.  **Control**: Hangup signal.

### Bot -> Gateway Messages (`CallAction`)
1.  **AudioChunk**: Audio to play back to the caller.
2.  **BotControl**: Commands to Gateway (HANGUP, TRANSFER).
