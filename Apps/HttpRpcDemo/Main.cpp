#include <eacp/Network/HTTPRpc/RpcClient.h>

#include "schema.client.h"

#include <iostream>

int main(int argc, char** argv)
{
    auto baseUrl = std::string {"http://localhost:8088/rpc"};
    if (argc > 1)
        baseUrl = argv[1];

    try
    {
        auto rpcClient = eacp::HTTP::Rpc::Client {baseUrl};
        auto client = MiroClient::Client {rpcClient.asInvoker()};

        auto reply = client.ping();
        std::cout << "ping -> pong=" << (reply.pong ? "true" : "false")
                  << " serverTimeMs=" << reply.serverTimeMs << "\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "RPC call failed: " << e.what() << "\n";
        return 1;
    }
}
