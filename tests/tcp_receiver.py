import asyncio
import time
import json
import argparse
from dataclasses import dataclass, field
from typing import Dict, List

@dataclass
class StreamStats:
    conn_id: str
    start_time: float
    last_packet_time: float = 0
    bytes_received: int = 0
    packet_count: int = 0
    jitter_sum: float = 0
    last_delta: float = 0

class TcpLoadReceiver:
    def __init__(self, host='0.0.0.0', port=9000):
        self.host = host
        self.port = port
        self.stats: Dict[str, StreamStats] = {}
        self.active_connections = 0

    async def handle_client(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
        addr = writer.get_extra_info('peername')
        conn_id = f"{addr[0]}:{addr[1]}"
        self.active_connections += 1
        
        now = time.time()
        stats = StreamStats(conn_id=conn_id, start_time=now, last_packet_time=now)
        self.stats[conn_id] = stats
        
        print(f"New connection from {conn_id}")

        try:
            while True:
                # Assuming PCMA sample size or some buffer size
                data = await reader.read(160) # 20ms of PCMA 8kHz mono is 160 bytes
                if not data:
                    break
                
                now = time.time()
                delta = now - stats.last_packet_time
                if stats.packet_count > 0:
                    # Simple jitter calculation (variation in arrival time)
                    stats.jitter_sum += abs(delta - stats.last_delta)
                
                stats.last_delta = delta
                stats.last_packet_time = now
                stats.bytes_received += len(data)
                stats.packet_count += 1

        except Exception as e:
            print(f"Error handling {conn_id}: {e}")
        finally:
            self.active_connections -= 1
            print(f"Connection closed: {conn_id}")
            writer.close()
            await writer.wait_closed()

    async def stats_reporter(self):
        while True:
            await asyncio.sleep(5)
            if not self.stats:
                continue
                
            total_bytes = sum(s.bytes_received for s in self.stats.values())
            total_packets = sum(s.packet_count for s in self.stats.values())
            
            report = {
                "timestamp": time.time(),
                "active_connections": self.active_connections,
                "total_bytes": total_bytes,
                "total_packets": total_packets,
                "streams": []
            }
            
            for cid, s in self.stats.items():
                duration = time.time() - s.start_time
                if duration > 0:
                    bps = s.bytes_received / duration
                    pps = s.packet_count / duration
                    avg_jitter = (s.jitter_sum / s.packet_count) * 1000 if s.packet_count > 0 else 0
                    
                    report["streams"].append({
                        "id": cid,
                        "bytes": s.bytes_received,
                        "bps": bps,
                        "pps": pps,
                        "avg_jitter_ms": avg_jitter,
                        "status": "active" if cid in self.stats else "closed" # Simplified
                    })
            
            print(f"--- Report ---")
            print(f"Active: {self.active_connections} | Total Bps: {total_bytes / (time.time() - min(s.start_time for s in self.stats.values())):.2f}")
            # In a real scenario, we might write this to a file or expose via HTTP
            with open("test_metrics.json", "w") as f:
                json.dump(report, f, indent=2)

    async def run(self):
        server = await asyncio.start_server(self.handle_client, self.host, self.port)
        addr = server.sockets[0].getsockname()
        print(f'Serving on {addr}')

        async with server:
            await asyncio.gather(server.serve_forever(), self.stats_reporter())

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=9000)
    args = parser.parse_args()
    
    receiver = TcpLoadReceiver(port=args.port)
    try:
        asyncio.run(receiver.run())
    except KeyboardInterrupt:
        pass
