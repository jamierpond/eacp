// Pure-logic tests for the file-based rendezvous (Endpoint.h). No networking
// or event loop — just the write/read/remove round-trip and its edge cases.

#include <eacp/InterAppCommunication/Endpoint.h>

#include <NanoTest/NanoTest.h>

#include <string>

using namespace nano;
namespace Ipc = eacp::Ipc;

namespace
{
constexpr auto npos = std::string::npos;
} // namespace

auto tPathIsStableAndEncodesName =
    test("Ipc.Endpoint/pathIsStableAndEncodesName") = []
{
    auto first = Ipc::endpointPath("alpha");
    auto second = Ipc::endpointPath("alpha");

    check(first == second); // deterministic for one name
    check(first.find("alpha") != npos); // encodes the name
    check(first.find("eacp-") != npos); // shared prefix
    check(first.find(".endpoint") != npos); // shared suffix

    check(Ipc::endpointPath("beta") != first); // distinct names, distinct paths
};

auto tWriteThenReadRoundTrips = test("Ipc.Endpoint/writeThenReadRoundTrips") = []
{
    auto name = std::string {"iac-test-roundtrip"};
    Ipc::removeEndpoint(name);

    Ipc::writeEndpoint(name, "http://127.0.0.1:9876/rpc");

    auto read = Ipc::readEndpoint(name);
    check(read.has_value());
    check(*read == "http://127.0.0.1:9876/rpc");

    Ipc::removeEndpoint(name);
};

auto tReadAbsentReturnsNullopt = test("Ipc.Endpoint/readAbsentReturnsNullopt") = []
{
    auto name = std::string {"iac-test-absent"};
    Ipc::removeEndpoint(name);

    check(!Ipc::readEndpoint(name).has_value());
};

auto tRemoveDeletesEndpoint = test("Ipc.Endpoint/removeDeletesEndpoint") = []
{
    auto name = std::string {"iac-test-remove"};
    Ipc::writeEndpoint(name, "http://127.0.0.1:1234/rpc");
    check(Ipc::readEndpoint(name).has_value());

    Ipc::removeEndpoint(name);
    check(!Ipc::readEndpoint(name).has_value());
};

auto tRemoveAbsentIsNoop = test("Ipc.Endpoint/removeAbsentIsNoop") = []
{
    // Removing something that was never written must not throw.
    Ipc::removeEndpoint("iac-test-never-existed");
    check(true);
};

auto tWriteOverwritesPreviousValue =
    test("Ipc.Endpoint/writeOverwritesPreviousValue") = []
{
    auto name = std::string {"iac-test-overwrite"};

    Ipc::writeEndpoint(name, "http://127.0.0.1:11111/rpc");
    Ipc::writeEndpoint(name, "http://127.0.0.1:2/rpc"); // shorter -> checks trunc

    auto read = Ipc::readEndpoint(name);
    check(read.has_value());
    check(*read == "http://127.0.0.1:2/rpc");

    Ipc::removeEndpoint(name);
};

auto tEmptyValueReadsAsNullopt = test("Ipc.Endpoint/emptyValueReadsAsNullopt") = []
{
    auto name = std::string {"iac-test-empty"};

    // A blank file is treated as "no endpoint", not an empty URL.
    Ipc::writeEndpoint(name, "");
    check(!Ipc::readEndpoint(name).has_value());

    Ipc::removeEndpoint(name);
};
