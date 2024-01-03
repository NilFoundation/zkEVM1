#ifndef EVMONE_FPRINTER_H
#define EVMONE_FPRINTER_H

#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>

struct PrintfHandler
{
    virtual uint64_t GetArgNumber() = 0;
    virtual const char* GetArgString() = 0;
    virtual void Puts(const char* data) = 0;
    virtual void Putc(char ch) = 0;
};


class PrintfParser
{
public:
    PrintfParser(PrintfHandler* handler) : handler_(handler) {}

    static int Parse(const char* format, PrintfHandler& handler)
    {
        return PrintfParser(&handler).Parse(format);
    }

    int Parse(const char* format)
    {
        flags_ = decltype(flags_){};
        fmt_ = format;
        unsigned length = 0;

        for (char ch = FetchChar(); ch != '\0'; ch = FetchChar())
        {
            if (ch != '%')
            {
                handler_->Putc(ch);
                continue;
            }
            while (true)
            {
                ch = FetchChar();
                switch (ch)
                {
                case '-':
                    flags_.left_justify = true;
                    continue;
                case ' ':
                case '+':
                    flags_.sign_symbol = ch;
                    continue;
                case '#':
                    flags_.hash_sign = true;
                    continue;
                }
                break;
            }
            if (ch == '0')
            {
                flags_.padding_char = '0';
                ch = FetchChar();
            }

            if (std::isdigit(ch))
            {
                char* end;
                flags_.width =
                    static_cast<decltype(flags_.width)>(std::strtoul(fmt_ - 1, &end, 10));
                fmt_ = end;
                ch = FetchChar();
            }

            if (ch == '.')
            {
                return raise_error("unsupported");
            }
            for (; ch == 'l'; ch = FetchChar())
            {
                length++;
            }
            if (length > 2)
            {
                return raise_error("Too many length symbols");
            }

            switch (ch)
            {
            case 'i':
            case 'd':
                if (length > 0)
                {
                    process_integer<long long>();
                }
                else
                {
                    process_integer<int>();
                }
                break;
            case 'u':
                if (length > 0)
                {
                    process_integer<unsigned long long>();
                }
                else
                {
                    process_integer<unsigned>();
                }
                break;
            case 'p':
                flags_.padding_char = '0';
                flags_.hash_sign = true;
                length = 2;
                [[fallthrough]];
            case 'x':
                flags_.base = 16;
                if (length > 0)
                {
                    process_integer<unsigned long long>();
                }
                else
                {
                    process_integer<unsigned>();
                }
                break;
            case 's':
            {
                auto str = handler_->GetArgString();
                handler_->Puts(str);
                break;
            }
            default:
                return raise_error("Unsupported specifier");
            }
        }
        return 0;
    }

    const char* GetErrorMsg() const { return error_msg_; }

private:
    char FetchChar() { return *fmt_++; }

    template <typename T>
    int process_integer()
    {
        static char digits[] = "0123456789abcdef0123456789ABCDEF";
        std::array<char, 64> buffer;
        auto val = static_cast<T>(handler_->GetArgNumber());
        std::pair<const char*, unsigned> prefix = {"", 0};

        if (val != 0 && flags_.hash_sign)
        {
            if (flags_.base == 8)
            {
                prefix = {"0", 1};
            }
            else if (flags_.base == 16)
            {
                prefix = {"0x", 2};
            }
        }

        char sign_char = flags_.sign_symbol;
        if constexpr (std::is_signed_v<T>)
        {
            if (val < 0)
            {
                sign_char = '-';
                val = -val;
            }
        }

        *buffer.rbegin() = '\0';
        char* p_end = &*(buffer.rbegin() + 1);
        char* p = &*(buffer.rbegin() + 1);
        do
        {
            *p-- = digits[(val % flags_.base) + flags_.capitals];
            val /= flags_.base;
        } while (val != 0);
        flags_.width -= p_end - p;
        if (sign_char != 0)
        {
            flags_.width--;
        }
        flags_.width -= prefix.second;

        if (flags_.padding_char == ' ' && !flags_.left_justify)
        {
            while (--flags_.width >= 0)
            {
                handler_->Putc(' ');
            }
        }
        if (sign_char)
        {
            handler_->Putc(sign_char);
        }
        if (prefix.second)
        {
            handler_->Puts(prefix.first);
        }
        if (flags_.padding_char == '0')
        {
            while (--flags_.width >= 0)
            {
                handler_->Putc('0');
            }
        }
        handler_->Puts(p + 1);

        if (flags_.left_justify)
        {
            while (--flags_.width >= 0)
            {
                handler_->Putc(' ');
            }
        }

        return 0;
    }

    int raise_error(const char* msg)
    {
        error_msg_ = msg;
        return -1;
    }

private:
    struct
    {
        int16_t width = 0;
        char sign_symbol = 0;
        char padding_char = ' ';
        uint8_t base = 10;
        uint8_t capitals = 0;
        bool left_justify = false;
        bool hash_sign = false;
    } flags_;
    const char* error_msg_{nullptr};
    const char* fmt_{nullptr};
    PrintfHandler* handler_{nullptr};
};


#endif  // EVMONE_FPRINTER_H
