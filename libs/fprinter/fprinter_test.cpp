#include "fprinter.h"
#include <gtest/gtest.h>
#include <sstream>
#include <variant>
#include <vector>

struct PrintfArg : public std::variant<uint64_t, const char*>
{
    using Base = std::variant<uint64_t, const char*>;

    PrintfArg(const char* v) : Base(v) {}
    template <typename T, typename = typename std::enable_if_t<std::is_integral_v<T>>>
    PrintfArg(T v) : Base(uint64_t(v))
    {}
};

class MyPrintfHandler : public PrintfHandler
{
public:
    template <typename... Args>
    MyPrintfHandler(Args&&... args)
    {
        ((args_.push_back(std::forward<Args>(args))), ...);
    }

    uint64_t GetArgNumber() override { return std::get<uint64_t>(args_[args_pos_++]); }
    const char* GetArgString() override { return std::get<const char*>(args_[args_pos_++]); }
    void Puts(const char* data) override { ss << data; }
    void Putc(char ch) override { ss << ch; }

    std::string ResultString() const { return ss.str(); }

private:
    std::stringstream ss;
    std::vector<PrintfArg> args_;
    unsigned args_pos_{0};
};

template <typename... Args>
void TestPrintf(const char* fmt, Args... args)
{
    MyPrintfHandler handler(std::forward<Args>(args)...);
    auto res = PrintfParser::Parse(fmt, handler);
    ASSERT_TRUE(res == 0);
    static constexpr int REF_STR_SIZE = 1024;
    char ref_str[REF_STR_SIZE];
    auto ref_res = snprintf(ref_str, REF_STR_SIZE, fmt, std::forward<Args>(args)...);
    ASSERT_TRUE(ref_res > 0 && ref_res < REF_STR_SIZE);
    ASSERT_EQ(handler.ResultString(), ref_str);
}

TEST(fprinter, common)
{
    TestPrintf("Test: {%u} {%s}", 12, "Hello World");
    TestPrintf("Test: {%i} {%d} {%u} {%x}", -12, -5, -8, 0x123);
    TestPrintf("Test: {%u}", 0x1234567890abc);
    TestPrintf("Test: {%lu}", 0x1234567890abc);
    TestPrintf("Test: {%llu}", 0x1234567890abc);
    TestPrintf("Test: {%x}", 0x1234567890abc);
    TestPrintf("Test: {%lx}", 0x1234567890abc);
    TestPrintf("Test: {%p}", 0x1234567890abc);
    TestPrintf("Test: {%#x}", 0x1234567890abc);
    TestPrintf("Test: {%4u}", 5);
    TestPrintf("Test: {%04u}", 5);
}
