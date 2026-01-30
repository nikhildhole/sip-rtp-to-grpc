import asyncio
import wave
import socket

OUT_WAV = "debug_pcm.wav"

async def handle_client(reader, writer):
    addr = writer.get_extra_info('peername')
    print(f"Client connected from {addr}")

    wf = wave.open(OUT_WAV, "wb")
    wf.setnchannels(1)       # mono
    wf.setsampwidth(2)       # PCM16 = 2 bytes
    wf.setframerate(8000)    # 8 kHz

    try:
        while True:
            try:
                header = await reader.readexactly(3)
            except asyncio.IncompleteReadError:
                break

            msg_type = header[0]
            length = (header[1] << 8) | header[2]

            try:
                payload = await reader.readexactly(length)
            except asyncio.IncompleteReadError:
                break

            if msg_type == 0x00:
                print(f"Received Terminate (0x00) for {addr}, closing connection...")
                break

            if msg_type == 0x01:
                print(f"Received UUID: {payload.decode('utf-8', errors='ignore')}")
                # Transfer logic disabled for loop prevention

            if msg_type == 0x02:
                print(f"Received Transfer Payload: {payload.decode('utf-8', errors='ignore')}")

            if msg_type == 0x10:
                # Audio data
                if wf:
                    wf.writeframes(payload)
                # Echo audio back
                writer.write(header + payload)
                await writer.drain()

    except Exception as e:
        print(f"Error handling client {addr}: {e}")
    finally:
        print(f"Cleaning up connection for {addr}")
        wf.close()
        writer.close()
        try:
            await asyncio.wait_for(writer.wait_closed(), timeout=2.0)
        except:
            pass

async def main():
    server = await asyncio.start_server(handle_client, "0.0.0.0", 9000)
    print("Listening on 9000")
    async with server:
        await server.serve_forever()

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nServer stopping...")
