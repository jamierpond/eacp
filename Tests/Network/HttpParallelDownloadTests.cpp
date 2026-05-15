#include <eacp/Core/Threads/EventLoop.h>
#include <eacp/Network/HTTP/Http.h>
#include <eacp/Network/HTTPServer/HttpServer.h>
#include <NanoTest/NanoTest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>

using namespace nano;
using eacp::HTTP::DownloadProgress;
using eacp::HTTP::Request;
using eacp::HTTP::Response;
using eacp::HTTP::Server;
using eacp::HTTP::ServerOptions;
using eacp::HTTP::ServerThreadingMode;
using eacp::Threads::callAsync;
using eacp::Threads::stopEventLoop;

namespace
{
std::string baseUrl(int port)
{
    return "http://127.0.0.1:" + std::to_string(port);
}

std::string tempPath(const std::string& name)
{
    auto path = std::filesystem::temp_directory_path()
              / ("eacp-parallel-" + name);
    std::error_code ec;
    std::filesystem::remove(path, ec);
    return path.string();
}

std::string readFile(const std::string& path)
{
    auto in = std::ifstream(path, std::ios::binary);
    auto ss = std::stringstream();
    ss << in.rdbuf();
    return ss.str();
}

std::string makePayload(std::size_t size)
{
    auto out = std::string();
    out.reserve(size);
    for (auto i = std::size_t {0}; i < size; ++i)
        out.push_back((char) ('A' + (i % 26)));
    return out;
}

struct RangeStats
{
    std::atomic<int> headCount {0};
    std::atomic<int> rangeCount {0};
    std::atomic<int> fullGetCount {0};
};

Response handleRangeRequest(const Request& req,
                            const std::string& body,
                            RangeStats& stats,
                            bool advertiseRanges)
{
    if (req.type == "HEAD")
    {
        stats.headCount.fetch_add(1);
        auto res = Response();
        res.statusCode = 200;
        res.headers["Content-Length"] = std::to_string(body.size());
        if (advertiseRanges)
            res.headers["Accept-Ranges"] = "bytes";
        return res;
    }

    auto rangeHeader = req.getHeader("Range");
    if (rangeHeader.empty() || !advertiseRanges)
    {
        stats.fullGetCount.fetch_add(1);
        auto res = Response();
        res.statusCode = 200;
        res.content = body;
        return res;
    }

    auto prefix = std::string("bytes=");
    if (rangeHeader.substr(0, prefix.size()) != prefix)
    {
        auto res = Response();
        res.statusCode = 400;
        return res;
    }

    auto rest = rangeHeader.substr(prefix.size());
    auto dash = rest.find('-');
    auto start = std::stoll(rest.substr(0, dash));
    auto end = std::stoll(rest.substr(dash + 1));

    stats.rangeCount.fetch_add(1);
    auto res = Response();
    res.statusCode = 206;
    res.content = body.substr((std::size_t) start,
                              (std::size_t) (end - start + 1));
    res.headers["Content-Range"] = "bytes " + std::to_string(start) + "-"
                                 + std::to_string(end) + "/"
                                 + std::to_string(body.size());
    return res;
}

struct ParallelOutcome
{
    Response response;
    bool eventLoopFinished = false;
};

void performDownload(Server& server,
                     const Request& clientRequest,
                     const std::string& destPath,
                     ParallelOutcome& out,
                     std::chrono::milliseconds timeout = std::chrono::seconds(15))
{
    auto worker = std::thread();
    auto stopped = eacp::Threads::runEventLoopFor(
        timeout,
        [&]
        {
            worker = std::thread(
                [&]
                {
                    out.response = clientRequest.downloadTo(destPath);
                    callAsync([] { stopEventLoop(); });
                });
        });

    if (worker.joinable())
        worker.join();

    out.eventLoopFinished = stopped;
    server.stop();
}

Server makePoolServer()
{
    auto opts = ServerOptions {};
    opts.threading = ServerThreadingMode::ThreadPool;
    opts.threadPoolSize = 8;
    return Server(opts);
}
} // namespace

auto tParallelDownloadReassembles =
    test("HttpParallelDownload/assemblesFileFromRangeWorkers") = []
{
    auto body = makePayload(2 * 1024 * 1024 + 137);
    auto stats = RangeStats();

    auto server = makePoolServer();
    check(server.listen(0,
                        [&](const Request& req)
                        { return handleRangeRequest(req, body, stats, true); }));
    auto port = server.boundPort();

    auto progress = DownloadProgress();
    auto req = Request(baseUrl(port) + "/big");
    req.progress = &progress;
    req.parallelChunks = 4;

    auto out = ParallelOutcome();
    auto destination = tempPath("assembled.bin");
    performDownload(server, req, destination, out);

    check(out.eventLoopFinished);
    check(out.response.error.empty());
    check(out.response.statusCode == 200);
    check(stats.headCount.load() == 1);
    check(stats.rangeCount.load() == 4);
    check(stats.fullGetCount.load() == 0);
    check(progress.done.load());
    check(progress.totalBytes.load() == (std::int64_t) body.size());
    check(progress.bytesReceived.load() == (std::int64_t) body.size());
    check(readFile(destination) == body);
};

auto tParallelFallsBackWhenNoRange =
    test("HttpParallelDownload/fallsBackWhenServerLacksAcceptRanges") = []
{
    auto body = makePayload(2 * 1024 * 1024 + 9);
    auto stats = RangeStats();

    auto server = makePoolServer();
    check(server.listen(0,
                        [&](const Request& req)
                        { return handleRangeRequest(req, body, stats, false); }));
    auto port = server.boundPort();

    auto progress = DownloadProgress();
    auto req = Request(baseUrl(port) + "/big");
    req.progress = &progress;
    req.parallelChunks = 4;

    auto out = ParallelOutcome();
    auto destination = tempPath("fallback.bin");
    performDownload(server, req, destination, out);

    check(out.eventLoopFinished);
    check(out.response.error.empty());
    check(stats.rangeCount.load() == 0);
    check(stats.fullGetCount.load() == 1);
    check(progress.done.load());
    check(readFile(destination) == body);
};

auto tParallelFallsBackOnSmallFile =
    test("HttpParallelDownload/fallsBackWhenTotalBelowThreshold") = []
{
    auto body = makePayload(8 * 1024);
    auto stats = RangeStats();

    auto server = makePoolServer();
    check(server.listen(0,
                        [&](const Request& req)
                        { return handleRangeRequest(req, body, stats, true); }));
    auto port = server.boundPort();

    auto req = Request(baseUrl(port) + "/small");
    req.parallelChunks = 4;

    auto out = ParallelOutcome();
    auto destination = tempPath("small.bin");
    performDownload(server, req, destination, out);

    check(out.eventLoopFinished);
    check(out.response.error.empty());
    check(stats.rangeCount.load() == 0);
    check(stats.fullGetCount.load() == 1);
    check(readFile(destination) == body);
};

auto tParallelChunksOneSkipsProbe =
    test("HttpParallelDownload/parallelChunksOneIssuesNoHeadProbe") = []
{
    auto body = makePayload(2 * 1024 * 1024);
    auto stats = RangeStats();

    auto server = makePoolServer();
    check(server.listen(0,
                        [&](const Request& req)
                        { return handleRangeRequest(req, body, stats, true); }));
    auto port = server.boundPort();

    auto req = Request(baseUrl(port) + "/single");
    req.parallelChunks = 1;

    auto out = ParallelOutcome();
    auto destination = tempPath("single.bin");
    performDownload(server, req, destination, out);

    check(out.eventLoopFinished);
    check(stats.headCount.load() == 0);
    check(stats.rangeCount.load() == 0);
    check(stats.fullGetCount.load() == 1);
    check(readFile(destination) == body);
};
