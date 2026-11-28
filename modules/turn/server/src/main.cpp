#include "turn_server.h"

#include "log.h"

#include <cstdlib>
#include <string>

int main(int argc, char** argv) {
    logging::init_from_env();

    uint16_t port =
        (argc > 1) ? static_cast<uint16_t>(std::strtoul(argv[1], nullptr, 10)) : 3478;
    std::string relay_address = (argc > 2) ? argv[2] : "127.0.0.1";

    stun::turn::server::TurnServer server(relay_address, port);
    return server.run();
}
