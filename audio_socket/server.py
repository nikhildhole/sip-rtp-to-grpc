import asyncio
import wave

OUT_WAV = "debug_pcm.wav"

async def handle_client(reader, writer):
    print("Client connected")

    wf = wave.open(OUT_WAV, "wb")
    wf.setnchannels(1)       # mono
    wf.setsampwidth(2)       # PCM16 = 2 bytes
    wf.setframerate(8000)    # 8 kHz

    try:
        while True:
            header = await reader.readexactly(3)
            msg_type = header[0]
            length = (header[1] << 8) | header[2]

            payload = await reader.readexactly(length)

            # record only audio packets
            if msg_type == 0x01:
                print("Received UUID: " + payload.decode('utf-8'))
                
                # For testing: Transfer the call after 5 seconds
                async def trigger_transfer():
                    await asyncio.sleep(5)
                    transfer_target = "sip:1001@127.0.0.1"
                    print(f"Triggering transfer to {transfer_target}...")
                    payload_bytes = transfer_target.encode('utf-8')
                    header = bytes([0x02, (len(payload_bytes) >> 8) & 0xFF, len(payload_bytes) & 0xFF])
                    writer.write(header + payload_bytes)
                    await writer.drain()
                
                asyncio.create_task(trigger_transfer())

            if msg_type == 0x02:
                print("Received Transfer Payload: " + payload.decode('utf-8'))

            if msg_type == 0x10:
                wf.writeframes(payload)

            # echo back exactly
            if msg_type != 0x02:
                writer.write(header + payload)
                await writer.drain()

    except asyncio.IncompleteReadError:
        print("Client disconnected")
    finally:
        wf.close()
        writer.close()
        await writer.wait_closed()

async def main():
    server = await asyncio.start_server(handle_client, "0.0.0.0", 9000)
    print("Listening on 9000")
    async with server:
        await server.serve_forever()

if __name__ == "__main__":
    asyncio.run(main())
