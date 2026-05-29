#include <eacp/Core/Utils/Files.h>
#include <eacp/Core/Utils/HttpRange.h>
#include <eacp/Core/Utils/Strings.h>
#include <NanoTest/NanoTest.h>

#include <filesystem>
#include <fstream>

using namespace nano;

namespace
{
std::filesystem::path tempRoot()
{
    auto path = std::filesystem::temp_directory_path()
              / "eacp-utils-tests-is-under-root";
    auto ec = std::error_code {};
    std::filesystem::remove_all(path, ec);
    std::filesystem::create_directories(path / "root" / "sub", ec);
    std::filesystem::create_directories(path / "outside", ec);
    std::ofstream(path / "root" / "sub" / "file.txt") << "in";
    std::ofstream(path / "outside" / "file.txt") << "out";
    return path;
}
} // namespace

auto tPercentDecodeKeepsPlainText = test("Strings/percentDecode/keepsPlainText") =
    []
{
    check(eacp::Strings::percentDecode("hello-world") == "hello-world");
};

auto tPercentDecodeDecodesHexEscapes =
    test("Strings/percentDecode/decodesHexEscapes") = []
{
    check(eacp::Strings::percentDecode("hello%20world%2Fok") == "hello world/ok");
};

auto tPercentDecodeLeavesMalformedEscapes =
    test("Strings/percentDecode/leavesMalformedEscapes") = []
{
    check(eacp::Strings::percentDecode("%") == "%");
    check(eacp::Strings::percentDecode("%2") == "%2");
    check(eacp::Strings::percentDecode("%zz") == "%zz");
};

auto tIsUnderRootAcceptsDescendant = test("Files/isUnderRoot/acceptsDescendant") =
    []
{
    auto base = tempRoot();
    check(eacp::Files::isUnderRoot(base / "root" / "sub" / "file.txt",
                                   base / "root"));
};

auto tIsUnderRootRejectsSibling = test("Files/isUnderRoot/rejectsSibling") = []
{
    auto base = tempRoot();
    check(!eacp::Files::isUnderRoot(base / "outside" / "file.txt",
                                    base / "root"));
};

auto tIsUnderRootHandlesDotDotPath =
    test("Files/isUnderRoot/handlesDotDotPath") = []
{
    auto base = tempRoot();
    check(eacp::Files::isUnderRoot(
        base / "root" / "sub" / ".." / "sub" / "file.txt", base / "root"));
};

auto tByteRangeProbe = test("Http/parseByteRange/probe") = []
{
    auto r = eacp::Http::parseByteRange("bytes=0-1", 5000);
    check(r.has_value());
    check(r->start == 0 && r->end == 1);
};

auto tByteRangeSuffix = test("Http/parseByteRange/suffix") = []
{
    auto r = eacp::Http::parseByteRange("bytes=-500", 5000);
    check(r.has_value());
    check(r->start == 4500 && r->end == 4999);
};

auto tByteRangeOpenEnded = test("Http/parseByteRange/openEnded") = []
{
    auto r = eacp::Http::parseByteRange("bytes=4999-", 5000);
    check(r.has_value());
    check(r->start == 4999 && r->end == 4999);
};

auto tByteRangePastEnd = test("Http/parseByteRange/pastEnd") = []
{
    check(!eacp::Http::parseByteRange("bytes=5000-", 5000).has_value());
};

auto tByteRangeMalformed = test("Http/parseByteRange/malformed") = []
{
    check(!eacp::Http::parseByteRange("", 5000).has_value());
    check(!eacp::Http::parseByteRange("bytes=0-1,2-3", 5000).has_value());
    check(!eacp::Http::parseByteRange("bytes=abc", 5000).has_value());
    check(!eacp::Http::parseByteRange("bytes=0-1", 0).has_value());
};
