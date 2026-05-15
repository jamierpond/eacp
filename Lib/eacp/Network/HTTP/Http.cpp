#include "Http.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <eacp/Core/Utils/Strings.h>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <random>
#include <sstream>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace eacp::HTTP
{

void Response::setContent(const std::string& contentToUse,
                          const std::string& contentType)
{
    content = contentToUse;
    headers["Content-Type"] = contentType;
}

void Response::setHeader(const std::string& key, const std::string& value)
{
    headers[key] = value;
}

void Response::setRedirect(const std::string& url, int status)
{
    statusCode = status;
    headers["Location"] = url;
}

bool Request::hasHeader(const std::string& key) const
{
    for (auto& [k, v]: headers)
        if (Strings::equalsCaseInsensitive(k, key))
            return true;
    return false;
}

std::string Request::getHeader(const std::string& key) const
{
    for (auto& [k, v]: headers)
        if (Strings::equalsCaseInsensitive(k, key))
            return v;
    return {};
}

bool Request::hasParam(const std::string& key) const
{
    return params.contains(key);
}

std::string Request::getParam(const std::string& key) const
{
    auto it = params.find(key);
    return it == params.end() ? std::string() : it->second;
}

std::string Request::pathWithoutQuery() const
{
    auto q = url.find('?');
    if (q == std::string::npos)
        return url;
    return url.substr(0, q);
}

std::string generateBoundary()
{
    auto rd = std::random_device();
    auto gen = std::mt19937(rd());
    auto dist = std::uniform_int_distribution<>(0, 15);
    auto chars = "0123456789abcdef";

    auto ss = std::stringstream();
    ss << "----eacp";
    for (int i = 0; i < 16; i++)
        ss << chars[dist(gen)];

    return ss.str();
}

std::string filenameFromPath(const std::string& path)
{
    auto pos = path.find_last_of("/\\");
    if (pos != std::string::npos)
        return path.substr(pos + 1);
    return path;
}

Request::Request(const std::string& urlToUse)
    : url(urlToUse)
{
}

Request Request::post(const std::string& urlToUse, const std::string& bodyToUse)
{
    auto req = Request(urlToUse);
    req.type = "POST";
    req.body = bodyToUse;
    return req;
}

Request& Request::addFormField(const std::string& name, const std::string& value)
{
    formFields.add({name, value});
    type = "POST";
    return *this;
}

Request& Request::addFileField(const std::string& fieldName,
                               const std::string& filePath,
                               const std::string& contentType)
{
    auto fileName = filenameFromPath(filePath);
    fileFields.add({fieldName, filePath, contentType, fileName});
    type = "POST";
    return *this;
}

void buildMultipartBody(Request& req)
{
    if (req.formFields.empty() && req.fileFields.empty())
        return;

    auto boundary = generateBoundary();
    req.headers["Content-Type"] = "multipart/form-data; boundary=" + boundary;

    auto ss = std::stringstream();

    for (const auto& field: req.formFields)
    {
        ss << "--" << boundary << "\r\n";
        ss << "Content-Disposition: form-data; name=\"" << field.name
           << "\"\r\n\r\n";
        ss << field.value << "\r\n";
    }

    for (const auto& file: req.fileFields)
    {
        auto stream = std::ifstream(file.filePath, std::ios::binary);
        auto content = std::string(std::istreambuf_iterator<char>(stream),
                                   std::istreambuf_iterator<char>());

        ss << "--" << boundary << "\r\n";
        ss << "Content-Disposition: form-data; name=\"" << file.fieldName
           << "\"; filename=\"" << file.fileName << "\"\r\n";
        ss << "Content-Type: " << file.contentType << "\r\n\r\n";
        ss << content << "\r\n";
    }

    ss << "--" << boundary << "--\r\n";
    req.body = ss.str();
}

Response Request::perform() const
{
    auto prepared = *this;
    buildMultipartBody(prepared);
    return httpRequest(prepared);
}

namespace
{
constexpr std::int64_t kMinParallelTotal = 1024 * 1024;
constexpr int kMaxParallelChunks = 8;

struct ChunkRange
{
    std::int64_t start;
    std::int64_t end;
};

std::string getHeaderCI(const std::map<std::string, std::string>& headers,
                        const std::string& key)
{
    for (const auto& [k, v]: headers)
        if (Strings::equalsCaseInsensitive(k, key))
            return v;

    return {};
}

bool acceptsRangeBytes(const std::string& value)
{
    auto lower = Strings::toLower(value);
    return lower.find("bytes") != std::string::npos;
}
} // namespace

Response downloadFileParallel(const Request& req, const std::string& filePath)
{
    auto* progress = req.progress;

    auto head = req;
    head.type = "HEAD";
    head.parallelChunks = 1;
    head.progress = nullptr;
    head.headers.erase("Range");
    auto headRes = httpRequest(head);

    auto rangeOk =
        headRes.error.empty() && headRes.statusCode >= 200
        && headRes.statusCode < 300
        && acceptsRangeBytes(getHeaderCI(headRes.headers, "Accept-Ranges"));

    auto total = std::int64_t(-1);
    if (rangeOk)
    {
        auto cl = getHeaderCI(headRes.headers, "Content-Length");
        if (!cl.empty())
        {
            try
            {
                total = std::stoll(cl);
            }
            catch (...)
            {
                total = -1;
            }
        }
    }

    auto N = std::min(req.parallelChunks, kMaxParallelChunks);

    if (!rangeOk || total < kMinParallelTotal || N < 2)
        return downloadFile(req, filePath);

    auto chunkSize = total / N;
    auto ranges = std::vector<ChunkRange>(N);
    for (auto i = 0; i < N; ++i)
    {
        ranges[i].start = i * chunkSize;
        ranges[i].end = (i == N - 1) ? total - 1 : (i + 1) * chunkSize - 1;
    }

    if (progress)
        progress->totalBytes.store(total);

    auto chunkProgress = std::vector<DownloadProgress>(N);
    auto chunkPaths = std::vector<std::string>(N);
    for (auto i = 0; i < N; ++i)
        chunkPaths[i] = filePath + ".part" + std::to_string(i);

    auto cleanupTempFiles = [&]
    {
        for (const auto& p: chunkPaths)
        {
            auto ec = std::error_code();
            std::filesystem::remove(p, ec);
        }
    };

    auto firstError = std::string();
    auto errorMutex = std::mutex();
    auto recordError = [&](std::string msg)
    {
        auto lock = std::lock_guard(errorMutex);
        if (firstError.empty())
            firstError = std::move(msg);
    };

    auto threads = std::vector<std::thread>();
    threads.reserve((size_t) N);
    for (auto i = 0; i < N; ++i)
    {
        threads.emplace_back(
            [&, i]
            {
                auto worker = req;
                worker.parallelChunks = 1;
                worker.type = "GET";
                worker.progress = &chunkProgress[i];
                worker.headers["Range"] = "bytes=" + std::to_string(ranges[i].start)
                                          + "-" + std::to_string(ranges[i].end);

                auto res = downloadFile(worker, chunkPaths[i]);

                if (!res.error.empty())
                {
                    recordError(res.error);
                    return;
                }
                if (res.statusCode != 206)
                    recordError("Range request returned status "
                                + std::to_string(res.statusCode));
            });
    }

    auto sumChunkBytes = [&]
    {
        auto sum = std::int64_t(0);
        for (const auto& cp: chunkProgress)
            sum += cp.bytesReceived.load();
        return sum;
    };

    auto aggregatorStop = std::atomic<bool>(false);
    auto aggregator = std::thread(
        [&]
        {
            while (!aggregatorStop.load())
            {
                if (progress)
                {
                    progress->bytesReceived.store(sumChunkBytes());

                    if (progress->cancel.load())
                        for (auto& cp: chunkProgress)
                            cp.cancel.store(true);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
        });

    for (auto& t: threads)
        t.join();

    aggregatorStop.store(true);
    aggregator.join();

    if (progress)
        progress->bytesReceived.store(sumChunkBytes());

    auto result = Response();

    if (progress && progress->cancel.load() && firstError.empty())
        firstError = "cancelled";

    if (!firstError.empty())
    {
        cleanupTempFiles();
        result.error = firstError;
        return result;
    }

    auto out = std::ofstream(filePath, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        cleanupTempFiles();
        result.error = "Failed to open destination file";
        return result;
    }
    for (const auto& p: chunkPaths)
    {
        auto in = std::ifstream(p, std::ios::binary);
        if (!in)
        {
            out.close();
            cleanupTempFiles();
            result.error = "Failed to read chunk file";
            return result;
        }
        out << in.rdbuf();
    }
    out.close();
    cleanupTempFiles();
    if (!out)
    {
        result.error = "Failed to write destination file";
        return result;
    }

    result.statusCode = 200;
    result.headers = headRes.headers;
    return result;
}

Response Request::downloadTo(const std::string& filePath) const
{
    if (parallelChunks > 1)
    {
        auto res = downloadFileParallel(*this, filePath);
        if (progress)
            progress->done.store(true);
        return res;
    }
    return downloadFile(*this, filePath);
}

namespace
{
int hexValue(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}
} // namespace

std::string urlDecode(const std::string& encoded)
{
    auto result = std::string();
    result.reserve(encoded.size());

    for (auto i = size_t {0}; i < encoded.size(); ++i)
    {
        auto c = encoded[i];
        if (c == '+')
        {
            result.push_back(' ');
        }
        else if (c == '%' && i + 2 < encoded.size())
        {
            auto hi = hexValue(encoded[i + 1]);
            auto lo = hexValue(encoded[i + 2]);
            if (hi < 0 || lo < 0)
            {
                result.push_back(c);
                continue;
            }
            result.push_back((char) ((hi << 4) | lo));
            i += 2;
        }
        else
        {
            result.push_back(c);
        }
    }

    return result;
}

std::map<std::string, std::string> parseQueryString(const std::string& query)
{
    auto result = std::map<std::string, std::string>();

    auto pos = size_t {0};
    while (pos < query.size())
    {
        auto amp = query.find('&', pos);
        auto end = amp == std::string::npos ? query.size() : amp;
        auto pair = query.substr(pos, end - pos);

        if (!pair.empty())
        {
            auto eq = pair.find('=');
            if (eq == std::string::npos)
                result[urlDecode(pair)] = "";
            else
                result[urlDecode(pair.substr(0, eq))] =
                    urlDecode(pair.substr(eq + 1));
        }

        if (amp == std::string::npos)
            break;
        pos = amp + 1;
    }

    return result;
}
} // namespace eacp::HTTP