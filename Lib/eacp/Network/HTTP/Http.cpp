#include "Http.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

namespace eacp::HTTP
{

namespace
{
bool equalsCaseInsensitive(const std::string& a, const std::string& b)
{
    if (a.size() != b.size())
        return false;

    for (auto i = size_t {0}; i < a.size(); ++i)
        if (std::tolower((unsigned char) a[i]) != std::tolower((unsigned char) b[i]))
            return false;

    return true;
}
} // namespace

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
        if (equalsCaseInsensitive(k, key))
            return true;
    return false;
}

std::string Request::getHeader(const std::string& key) const
{
    for (auto& [k, v]: headers)
        if (equalsCaseInsensitive(k, key))
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
        if (equalsCaseInsensitive(k, key))
            return v;
    return {};
}

bool acceptsRangeBytes(const std::string& value)
{
    auto lower = std::string();
    lower.reserve(value.size());
    for (auto c: value)
        lower.push_back((char) std::tolower((unsigned char) c));
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

    auto rangeOk = headRes.error.empty()
                && headRes.statusCode >= 200 && headRes.statusCode < 300
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

    auto chunks = std::vector<std::string>(N);
    auto threads = std::vector<std::thread>();
    auto firstError = std::string();
    auto errorMutex = std::mutex();

    auto recordError = [&](std::string msg)
    {
        auto lock = std::lock_guard(errorMutex);
        if (firstError.empty())
            firstError = std::move(msg);
    };

    threads.reserve((size_t) N);
    for (auto i = 0; i < N; ++i)
    {
        threads.emplace_back(
            [&, i]
            {
                if (progress && progress->cancel.load())
                {
                    recordError("cancelled");
                    return;
                }

                auto worker = req;
                worker.parallelChunks = 1;
                worker.type = "GET";
                worker.progress = nullptr;
                worker.headers["Range"] = "bytes=" + std::to_string(ranges[i].start)
                                        + "-" + std::to_string(ranges[i].end);

                auto res = httpRequest(worker);

                if (!res.error.empty())
                {
                    recordError(res.error);
                    return;
                }
                if (res.statusCode != 206)
                {
                    recordError("Range request returned status "
                                + std::to_string(res.statusCode));
                    return;
                }

                auto expected = (size_t) (ranges[i].end - ranges[i].start + 1);
                if (res.content.size() != expected)
                {
                    recordError("Range chunk size mismatch");
                    return;
                }

                if (progress)
                    progress->bytesReceived.fetch_add((std::int64_t) res.content.size());
                chunks[i] = std::move(res.content);
            });
    }

    for (auto& t: threads)
        t.join();

    auto result = Response();

    if (progress && progress->cancel.load() && firstError.empty())
        firstError = "cancelled";

    if (!firstError.empty())
    {
        result.error = firstError;
        return result;
    }

    auto out = std::ofstream(filePath, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        result.error = "Failed to open destination file";
        return result;
    }
    for (const auto& c: chunks)
        out.write(c.data(), (std::streamsize) c.size());
    out.close();
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