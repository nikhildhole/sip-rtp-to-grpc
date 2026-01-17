import asyncio
import wave
from dataclasses import dataclass

CHUNK = 323  # number of frames per packet

@dataclass(frozen=True)
class types_struct:
    uuid:    bytes = b'\x01'
    audio:   bytes = b'\x10'
    silence: bytes = b'\x02'
    hangup:  bytes = b'\x00'
    error:   bytes = b'\xff'

types = types_struct()

connections = set()  # track active connections

def split_data(data):
    if len(data) < 3:
        print('[AUDIOSOCKET WARNING] The data received was less than 3 bytes.')
        return b'\x00', 0, bytes(320)
    return data[:1], int.from_bytes(data[1:3], 'big'), data[3:]

async def handle_client(reader, writer):
    addr = writer.get_extra_info("peername")
    connections.add(addr)
    print(f"V2 Client connected: {addr}. Total connections: {len(connections)}")

    try:
        while True:
            data = await reader.read(CHUNK + 3)
            if not data:
                print("Client disconnected.")
                break

            type_, length, payload = split_data(data)
            if type_ == types.uuid:
                print(f"V2 Client connected: {addr}. Total connections: {len(connections)} with uuid {payload.hex()}")
            if type_ == types.audio:
                response = type_ + length.to_bytes(2, "big") + payload[:length]
                writer.write(response)
                await writer.drain()

    except Exception as e:
        print(f"Error: {e}")

    finally:
        connections.remove(addr)
        writer.close()
        await writer.wait_closed()
        print(f"V2 Connection closed. Active: {len(connections)}")

async def main():
    server = await asyncio.start_server(handle_client, "0.0.0.0", 9000)
    print("V2 Server listening on 0.0.0.0:9000")
    async with server:
        await server.serve_forever()

if __name__ == "__main__":
    asyncio.run(main())