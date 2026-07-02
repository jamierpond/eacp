#include "IconPacker.h"

#include <cstdio>
#include <fstream>
#include <string>

using namespace eacp::IconPacker;

namespace
{
std::optional<Bytes> readFile(const std::string& path)
{
    auto stream = std::ifstream {path, std::ios::binary};
    if (!stream)
        return std::nullopt;

    return Bytes {std::istreambuf_iterator<char>(stream),
                  std::istreambuf_iterator<char>()};
}

bool writeFile(const std::string& path, const Bytes& data)
{
    auto stream = std::ofstream {path, std::ios::binary};
    if (!stream)
        return false;

    stream.write(reinterpret_cast<const char*>(data.data()),
                 static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(stream);
}

int fail(const std::string& message)
{
    std::fprintf(stderr, "IconPacker: %s\n", message.c_str());
    return 1;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 4)
        return fail("usage: IconPacker <ico|icns> <output> <input.png>...");

    auto format = std::string {argv[1]};
    auto outputPath = std::string {argv[2]};

    auto pngs = std::vector<Bytes> {};
    for (auto index = 3; index < argc; ++index)
    {
        auto png = readFile(argv[index]);
        if (!png)
            return fail("cannot read " + std::string {argv[index]});

        if (!getSquarePngSize(*png))
            return fail(std::string {argv[index]}
                        + " is not a square PNG (ico needs sizes up to 256, "
                          "icns 16/32/64/128/256/512/1024)");

        pngs.push_back(std::move(*png));
    }

    auto packed = std::optional<Bytes> {};
    if (format == "ico")
        packed = packIco(pngs);
    else if (format == "icns")
        packed = packIcns(pngs);
    else
        return fail("unknown format '" + format + "', expected ico or icns");

    if (!packed)
        return fail("unsupported PNG dimensions for format " + format);

    if (!writeFile(outputPath, *packed))
        return fail("cannot write " + outputPath);

    return 0;
}
