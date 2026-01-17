#include "app/GatewayApp.h"
#include "app/SignalHandler.h"
#include <iostream>

int main(int argc, char *argv[]) {
  SignalHandler::init();

  std::string configPath = "../config/gateway.yaml";
  if (argc > 1) {
    std::string arg = argv[1];
    if (arg == "--config" && argc > 2) {
      configPath = argv[2];
    }
  }

  GatewayApp app;
  if (!app.init(configPath)) {
    std::cerr << "Failed to initialize gateway" << std::endl;
    return 1;
  }

  app.run();
  return 0;
}
