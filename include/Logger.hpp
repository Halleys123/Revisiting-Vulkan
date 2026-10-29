#pragma once
// Logger.hpp - small header-only singleton logger (C++17). No .cpp file needed.
//
// Output format:
//   [  0.012s] [INFO    ] message                      (Device.cpp:84 pickDevice)
//   [  0.013s] [WARN    ] first line of a multi-line message
//                         continuation lines are aligned under the message
//
// Compile-time switch:
//   - Enabled by default in debug builds, compiled out when NDEBUG is defined.
//   - Override with -DLOGGER_ENABLED=1 (force on) or -DLOGGER_ENABLED=0 (force off).
//   - When disabled, every LOG_* macro expands to ((void)0): no call, and the
//     arguments are not even evaluated.
//
// Runtime filtering (lock-free check, done before any formatting):
//   setMinLevel(lo) / setMaxLevel(hi) / setLevelRange(lo, hi) / setEnabledLevels({1,3,5})
//   enableLevel(l) / disableLevel(l) / enableAll() / disableAll()
//
// Output options:
//   setTimeMode(TimeMode::Off | Uptime | WallClock)
//   setShowSource(bool)            file:line of the LOG_* call
//   setShowFunction(bool)          function name of the LOG_* call
//   setSourceMinLevel(level)       only show source info for levels >= level
//   setColorEnabled(bool), setOutput(FILE*)

#include <array>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <initializer_list>
#include <mutex>
#include <string>

#ifndef LOGGER_ENABLED
    #ifdef NDEBUG
        #define LOGGER_ENABLED 0
    #else
        #define LOGGER_ENABLED 1
    #endif
#endif

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <io.h>
    #include <windows.h>
    #define LOGGER_ISATTY(f) _isatty(_fileno(f))
#else
    #include <unistd.h>
    #define LOGGER_ISATTY(f) isatty(fileno(f))
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define LOGGER_PRINTF(fmtIdx, argIdx) __attribute__((format(printf, fmtIdx, argIdx)))
#else
    #define LOGGER_PRINTF(fmtIdx, argIdx)
#endif

// ----------------------------------------------------------------------------
// Colors and styles
// ----------------------------------------------------------------------------

struct LogColor {
    enum class Kind : uint8_t { Default, Palette, RGB };
    Kind    kind = Kind::Default;
    uint8_t r = 0, g = 0, b = 0; // for Palette, r holds the 0-255 index

    static constexpr LogColor none() { return LogColor{}; }
    static constexpr LogColor palette(uint8_t index) {
        LogColor c; c.kind = Kind::Palette; c.r = index; return c;
    }
    static constexpr LogColor rgb(uint8_t r, uint8_t g, uint8_t b) {
        LogColor c; c.kind = Kind::RGB; c.r = r; c.g = g; c.b = b; return c;
    }
};

namespace Colors {
    inline constexpr LogColor None          = LogColor::none();

    // Basic 16 colors. NOTE: these are remapped by your terminal theme, so they
    // can look different (or unreadable) from machine to machine.
    inline constexpr LogColor Black         = LogColor::palette(0);
    inline constexpr LogColor Red           = LogColor::palette(1);
    inline constexpr LogColor Green         = LogColor::palette(2);
    inline constexpr LogColor Yellow        = LogColor::palette(3);
    inline constexpr LogColor Blue          = LogColor::palette(4);
    inline constexpr LogColor Magenta       = LogColor::palette(5);
    inline constexpr LogColor Cyan          = LogColor::palette(6);
    inline constexpr LogColor White         = LogColor::palette(7);
    inline constexpr LogColor BrightRed     = LogColor::palette(9);
    inline constexpr LogColor BrightGreen   = LogColor::palette(10);
    inline constexpr LogColor BrightYellow  = LogColor::palette(11);
    inline constexpr LogColor BrightBlue    = LogColor::palette(12);
    inline constexpr LogColor BrightMagenta = LogColor::palette(13);
    inline constexpr LogColor BrightCyan    = LogColor::palette(14);
    inline constexpr LogColor BrightWhite   = LogColor::palette(15);

    // Fixed colors (same on every terminal). The built-in levels use these.
    inline constexpr LogColor DarkGray      = LogColor::palette(243);
    inline constexpr LogColor Gray          = LogColor::palette(250);
    inline constexpr LogColor Periwinkle    = LogColor::rgb(110, 170, 255);
    inline constexpr LogColor Mint          = LogColor::rgb( 80, 220, 120);
    inline constexpr LogColor Lavender      = LogColor::rgb(210, 150, 255);
    inline constexpr LogColor Amber         = LogColor::rgb(255, 200,  60);
    inline constexpr LogColor Coral         = LogColor::rgb(255,  90,  90);
    inline constexpr LogColor DarkRed       = LogColor::rgb(170,  25,  25);
    inline constexpr LogColor DarkMagenta   = LogColor::rgb(150,  40, 160);
    inline constexpr LogColor Orange        = LogColor::palette(208);
    inline constexpr LogColor Pink          = LogColor::palette(213);
    inline constexpr LogColor Purple        = LogColor::palette(135);
    inline constexpr LogColor Teal          = LogColor::palette(37);
}

struct LogStyle {
    LogColor fg           = Colors::None;
    LogColor bg           = Colors::None;
    bool     bold         = false; // applies to the [TAG]
    bool     colorMessage = false; // also color the message text with fg (no bg)
};

// ----------------------------------------------------------------------------
// Levels
// ----------------------------------------------------------------------------
// A level is a small integer id (0..31). Built-ins are ordered by severity.
// Custom levels from Logger::registerLevel() get the next free id (9, 10, ...).

using LogLevel = uint8_t;

namespace Level {
    inline constexpr LogLevel Trace    = 0;
    inline constexpr LogLevel Debug    = 1;
    inline constexpr LogLevel Info     = 2;
    inline constexpr LogLevel Success  = 3;
    inline constexpr LogLevel Notice   = 4;
    inline constexpr LogLevel Warn     = 5;
    inline constexpr LogLevel Error    = 6;
    inline constexpr LogLevel Critical = 7;
    inline constexpr LogLevel Fatal    = 8;
}

// Where a log call came from. Filled in automatically by the LOG_* macros.
struct LogSource {
    const char* file = nullptr;
    int         line = 0;
    const char* func = nullptr;
};

enum class TimeMode : uint8_t {
    Off,
    Uptime,    // seconds since the logger was created: [  0.012s]
    WallClock  // local time:                           [14:03:22.123]
};

// ----------------------------------------------------------------------------
// Logger
// ----------------------------------------------------------------------------

class Logger {
public:
    static constexpr size_t kMaxLevels = 32; // matches the 32-bit enable mask

    static Logger& get() {
        static Logger instance;
        return instance;
    }

    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&)                 = delete;
    Logger& operator=(Logger&&)      = delete;

    // ---- level registration / styling ---------------------------------------

    // Add a new level. Returns its id (new levels start enabled).
    // If all slots are used, falls back to Level::Info.
    LogLevel registerLevel(const char* name, const LogStyle& style = {Colors::White}) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (size_t i = 0; i < kMaxLevels; ++i) {
            if (!levels_[i].used) {
                setLevelLocked(static_cast<LogLevel>(i), name, style);
                enabledMask_.fetch_or(1u << i, std::memory_order_relaxed);
                return static_cast<LogLevel>(i);
            }
        }
        return Level::Info;
    }

    // Restyle an existing level (built-in or custom).
    void setLevelStyle(LogLevel level, const LogStyle& style) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (level < kMaxLevels && levels_[level].used) {
            setLevelLocked(level, nullptr, style);
        }
    }

    // ---- level filtering ------------------------------------------------------

    void enableLevel(LogLevel l)  { if (l < kMaxLevels) enabledMask_.fetch_or(1u << l,  std::memory_order_relaxed); }
    void disableLevel(LogLevel l) { if (l < kMaxLevels) enabledMask_.fetch_and(~(1u << l), std::memory_order_relaxed); }
    void enableAll()              { enabledMask_.store(~0u, std::memory_order_relaxed); }
    void disableAll()             { enabledMask_.store(0u,  std::memory_order_relaxed); }

    void setMinLevel(LogLevel lo)                { setMask(maskRange(lo, kMaxLevels - 1)); }
    void setMaxLevel(LogLevel hi)                { setMask(maskRange(0, hi)); }
    void setLevelRange(LogLevel lo, LogLevel hi) { setMask(maskRange(lo, hi)); }

    // Show exactly these levels and nothing else.
    void setEnabledLevels(std::initializer_list<LogLevel> levels) {
        uint32_t mask = 0;
        for (LogLevel l : levels) if (l < kMaxLevels) mask |= (1u << l);
        setMask(mask);
    }

    bool isEnabled(LogLevel l) const {
        return l < kMaxLevels && ((enabledMask_.load(std::memory_order_relaxed) >> l) & 1u);
    }

    // ---- output config --------------------------------------------------------

    void setColorEnabled(bool on)         { std::lock_guard<std::mutex> l(mutex_); useColor_ = on; }
    void setTimeMode(TimeMode mode)       { std::lock_guard<std::mutex> l(mutex_); timeMode_ = mode; }
    void setShowSource(bool on)           { std::lock_guard<std::mutex> l(mutex_); showSource_ = on; }
    void setShowFunction(bool on)         { std::lock_guard<std::mutex> l(mutex_); showFunc_ = on; }
    void setSourceMinLevel(LogLevel lvl)  { std::lock_guard<std::mutex> l(mutex_); sourceMinLevel_ = lvl; }

    // Redirect output (e.g. a log file). Colors are auto-disabled if it isn't a terminal.
    void setOutput(FILE* out) {
        std::lock_guard<std::mutex> lock(mutex_);
        out_ = out ? out : stdout;
        useColor_ = LOGGER_ISATTY(out_) != 0;
    }

    // ---- logging --------------------------------------------------------------

    // Without source info.
    void log(LogLevel level, const char* fmt, ...) LOGGER_PRINTF(3, 4) {
        if (!isEnabled(level)) return; // cheap early-out before any formatting
        va_list args;
        va_start(args, fmt);
        emit(level, nullptr, fmt, args);
        va_end(args);
    }

    // With source info (this is what the LOG_* macros call).
    void logAt(LogLevel level, const LogSource& src, const char* fmt, ...) LOGGER_PRINTF(4, 5) {
        if (!isEnabled(level)) return;
        va_list args;
        va_start(args, fmt);
        emit(level, &src, fmt, args);
        va_end(args);
    }

private:
    struct LevelInfo {
        std::string name;       // "INFO"
        std::string tagEscape;  // ANSI for the [TAG]
        std::string msgEscape;  // ANSI for the message (empty unless colorMessage)
        bool        colorMessage = false;
        bool        used = false;
    };

    static constexpr const char* kReset = "\033[0m";
    static constexpr const char* kDim   = "\033[38;5;244m";

    Logger() : start_(std::chrono::steady_clock::now()) {
        enableVirtualTerminal();
        useColor_ = LOGGER_ISATTY(out_) != 0;

        //                                     fg                 bg                   bold   colorMessage
        setLevelLocked(Level::Trace,    "TRACE",    {Colors::DarkGray});
        setLevelLocked(Level::Debug,    "DEBUG",    {Colors::Gray});
        setLevelLocked(Level::Info,     "INFO",     {Colors::Periwinkle});
        setLevelLocked(Level::Success,  "SUCCESS",  {Colors::Mint});
        setLevelLocked(Level::Notice,   "NOTICE",   {Colors::Lavender});
        setLevelLocked(Level::Warn,     "WARN",     {Colors::Amber,       Colors::None,        false, true});
        setLevelLocked(Level::Error,    "ERROR",    {Colors::Coral,       Colors::None,        true,  true});
        setLevelLocked(Level::Critical, "CRITICAL", {Colors::BrightWhite, Colors::DarkRed,     true,  true});
        setLevelLocked(Level::Fatal,    "FATAL",    {Colors::BrightWhite, Colors::DarkMagenta, true,  true});
        enabledMask_.store(~0u, std::memory_order_relaxed);
    }
    ~Logger() = default;

    void emit(LogLevel level, const LogSource* src, const char* fmt, va_list args) {
        // 1) Format the message outside the lock. Stack buffer first, heap only for long messages.
        char stackBuf[1024];
        std::string heapBuf;
        const char* msg = stackBuf;

        va_list copy;
        va_copy(copy, args);
        int n = std::vsnprintf(stackBuf, sizeof(stackBuf), fmt, args);
        if (n >= static_cast<int>(sizeof(stackBuf))) {
            heapBuf.resize(static_cast<size_t>(n) + 1);
            std::vsnprintf(heapBuf.data(), heapBuf.size(), fmt, copy);
            msg = heapBuf.c_str();
        }
        va_end(copy);
        if (n < 0) return;

        // Trailing newlines in the message are dropped; we add exactly one at the end.
        size_t len = static_cast<size_t>(n);
        while (len > 0 && (msg[len - 1] == '\n' || msg[len - 1] == '\r')) --len;

        // 2) Build the whole line in one buffer and write it once.
        std::lock_guard<std::mutex> lock(mutex_);
        const LevelInfo& info = levels_[level];
        if (!info.used) return;

        std::string& out = lineBuf_;
        out.clear();
        size_t indent = 0; // visible width of the prefix, used to align continuation lines

        if (timeMode_ != TimeMode::Off) {
            char tb[48];
            int tn = formatTime(tb, sizeof(tb));
            if (useColor_) out += kDim;
            out.append(tb, static_cast<size_t>(tn));
            if (useColor_) out += kReset;
            out += ' ';
            indent += static_cast<size_t>(tn) + 1;
        }

        if (useColor_) out += info.tagEscape;
        out += '[';
        out += info.name;
        out.append(tagWidth_ - info.name.size(), ' ');
        out += ']';
        if (useColor_) out += kReset;
        out += ' ';
        indent += tagWidth_ + 3;

        const bool addSource = src && src->file && showSource_ && level >= sourceMinLevel_;
        const bool tintMsg   = useColor_ && info.colorMessage;

        const char* p   = msg;
        const char* end = msg + len;
        bool first = true;
        for (;;) {
            const char* nl = (end > p) ? static_cast<const char*>(std::memchr(p, '\n', static_cast<size_t>(end - p))) : nullptr;
            const char* lineEnd = nl ? nl : end;
            if (lineEnd > p && lineEnd[-1] == '\r') --lineEnd;

            if (!first) out.append(indent, ' ');
            if (tintMsg) out += info.msgEscape;
            out.append(p, static_cast<size_t>(lineEnd - p));
            if (tintMsg) out += kReset;

            if (first && addSource) appendSource(out, *src);

            out += '\n';
            first = false;
            if (!nl) break;
            p = nl + 1;
        }

        std::fwrite(out.data(), 1, out.size(), out_);
        if (level >= Level::Error) std::fflush(out_); // don't lose errors on a crash
    }

    // "  (Device.cpp:84 pickDevice)" in dim gray
    void appendSource(std::string& out, const LogSource& src) const {
        if (useColor_) out += kDim;
        out += "  (";
        out += baseName(src.file);
        char num[16];
        std::snprintf(num, sizeof(num), ":%d", src.line);
        out += num;
        if (showFunc_ && src.func) {
            out += ' ';
            out += src.func;
        }
        out += ')';
        if (useColor_) out += kReset;
    }

    static const char* baseName(const char* path) {
        const char* base = path;
        for (const char* p = path; *p; ++p) {
            if (*p == '/' || *p == '\\') base = p + 1;
        }
        return base;
    }

    int formatTime(char* buf, size_t size) const {
        if (timeMode_ == TimeMode::Uptime) {
            double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
            return std::snprintf(buf, size, "[%7.3fs]", secs);
        }
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        int ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000);
        std::tm tmv{};
#if defined(_WIN32)
        localtime_s(&tmv, &t);
#else
        localtime_r(&t, &tmv);
#endif
        return std::snprintf(buf, size, "[%02d:%02d:%02d.%03d]", tmv.tm_hour, tmv.tm_min, tmv.tm_sec, ms);
    }

    void setMask(uint32_t m) { enabledMask_.store(m, std::memory_order_relaxed); }

    // Bits lo..hi set, inclusive.
    static uint32_t maskRange(size_t lo, size_t hi) {
        if (hi >= kMaxLevels) hi = kMaxLevels - 1;
        if (lo > hi) return 0;
        size_t width = hi - lo + 1;
        uint32_t bits = (width >= 32) ? ~0u : ((1u << width) - 1u);
        return bits << lo;
    }

    // Builds the ANSI escapes once, so logging itself does no work for them.
    void setLevelLocked(LogLevel id, const char* name, const LogStyle& style) {
        LevelInfo& info = levels_[id];
        if (name) {
            info.name = name;
            if (info.name.size() > tagWidth_) tagWidth_ = info.name.size();
        }
        info.tagEscape    = buildEscape(style.fg, style.bg, style.bold);
        info.msgEscape    = buildEscape(style.fg, Colors::None, false);
        info.colorMessage = style.colorMessage;
        info.used = true;
    }

    static void appendColor(std::string& s, bool foreground, const LogColor& c) {
        char buf[32];
        switch (c.kind) {
            case LogColor::Kind::Palette:
                std::snprintf(buf, sizeof(buf), "%d;5;%d", foreground ? 38 : 48, c.r);
                break;
            case LogColor::Kind::RGB:
                std::snprintf(buf, sizeof(buf), "%d;2;%d;%d;%d", foreground ? 38 : 48, c.r, c.g, c.b);
                break;
            default:
                return;
        }
        if (!s.empty()) s += ';';
        s += buf;
    }

    static std::string buildEscape(const LogColor& fg, const LogColor& bg, bool bold) {
        std::string params;
        if (bold) params += '1';
        appendColor(params, true,  fg);
        appendColor(params, false, bg);
        if (params.empty()) return {};
        return "\033[" + params + "m";
    }

    static void enableVirtualTerminal() {
#if defined(_WIN32)
        for (DWORD id : {STD_OUTPUT_HANDLE, STD_ERROR_HANDLE}) {
            HANDLE h = GetStdHandle(id);
            DWORD mode = 0;
            if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode)) {
                SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
        }
#endif
    }

    std::array<LevelInfo, kMaxLevels>      levels_{};
    std::mutex                             mutex_;
    std::atomic<uint32_t>                  enabledMask_{~0u};
    std::string                            lineBuf_;          // reused, guarded by mutex_
    std::chrono::steady_clock::time_point  start_;
    FILE*                                  out_            = stdout;
    size_t                                 tagWidth_       = 0;   // longest level name
    TimeMode                               timeMode_       = TimeMode::Uptime;
    LogLevel                               sourceMinLevel_ = Level::Trace; // show source for all levels
    bool                                   useColor_       = true;
    bool                                   showSource_     = true;
    bool                                   showFunc_       = true;
};

// ----------------------------------------------------------------------------
// Macros - use these in your code so logging can be compiled out entirely.
// They capture file / line / function automatically, so call sites don't change.
// ----------------------------------------------------------------------------

#if LOGGER_ENABLED
    #define LOG_AT(level, ...)  ::Logger::get().logAt((level), ::LogSource{__FILE__, __LINE__, __func__}, __VA_ARGS__)
    #define LOG_TRACE(...)      LOG_AT(::Level::Trace,    __VA_ARGS__)
    #define LOG_DEBUG(...)      LOG_AT(::Level::Debug,    __VA_ARGS__)
    #define LOG_INFO(...)       LOG_AT(::Level::Info,     __VA_ARGS__)
    #define LOG_SUCCESS(...)    LOG_AT(::Level::Success,  __VA_ARGS__)
    #define LOG_NOTICE(...)     LOG_AT(::Level::Notice,   __VA_ARGS__)
    #define LOG_WARN(...)       LOG_AT(::Level::Warn,     __VA_ARGS__)
    #define LOG_ERROR(...)      LOG_AT(::Level::Error,    __VA_ARGS__)
    #define LOG_CRITICAL(...)   LOG_AT(::Level::Critical, __VA_ARGS__)
    #define LOG_FATAL(...)      LOG_AT(::Level::Fatal,    __VA_ARGS__)
#else
    #define LOG_AT(level, ...)  ((void)0)
    #define LOG_TRACE(...)      ((void)0)
    #define LOG_DEBUG(...)      ((void)0)
    #define LOG_INFO(...)       ((void)0)
    #define LOG_SUCCESS(...)    ((void)0)
    #define LOG_NOTICE(...)     ((void)0)
    #define LOG_WARN(...)       ((void)0)
    #define LOG_ERROR(...)      ((void)0)
    #define LOG_CRITICAL(...)   ((void)0)
    #define LOG_FATAL(...)      ((void)0)
#endif
