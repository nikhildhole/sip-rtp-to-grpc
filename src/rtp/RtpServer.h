#include <functional>
#include <map>
#include <memory>
#include <netinet/in.h>
#include <vector>

#include "RtpPacket.h"
#include "RtpWorker.h"

class RtpServer {
public:
  using PacketHandler = std::function<void(int localPort, const RtpPacket &,
                                           const sockaddr_in &)>;
  using RtcpHandler = std::function<void(int localPort, const RtcpPacket &,
                                           const sockaddr_in &)>;

  static RtpServer &instance();

  // Initialize with port range and optional thread count
  void init(int startPort, int endPort, int threadCount = 0);

  // Allocate a port for a new call (Round-robin across workers)
  int allocatePort();
  void releasePort(int port);

  // Set callback (propagated to all workers)
  void setPacketHandler(PacketHandler handler);
  void setRtcpHandler(RtcpHandler handler);

  // Send (Delegates to appropriate worker)
  void send(int localPort, const RtpPacket &packet, const sockaddr_in &dest);

private:
  RtpServer() = default;

  std::vector<std::unique_ptr<RtpWorker>> workers_;
  int nextWorker_ = 0; // Round-robin index
  int startPort_ = 0;
  int endPort_ = 0;
  
  std::mutex mutex_; // Protects worker vector/index if needed
};
