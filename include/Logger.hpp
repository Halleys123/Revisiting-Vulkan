#pragma once
// Logger.hpp - small header-only singleton logger (C++17). No .cpp file needed.
//
// Default layout (LogLayout::Engine), file:line right-aligned to the terminal edge:
//   [  0.605s] [  INFO  ] isPhysicalDeviceSuitable  Queue Family details for NVIDIA GeForce RTX 3050   VulkanRenderer.cpp:272
//   [  0.605s] [ ERROR  ] isPhysicalDeviceSuitable  AMD Radeon(TM) Graphics: Vulkan 1.2.196 ...       VulkanRenderer.cpp:226
//   [  0.605s] [ NOTICE ] createSwapchain           A long message is wrapped at the terminal edge
//                                                   with a hanging indent instead of breaking at column 0
//
// Old table layout (LogLayout::Columns):
//   [  0.605s] [  INFO  ] VulkanRenderer.cpp:272   isPhysicalDeviceSuitable | Queue Family details ...
//
// Both: centered level tags, per-function colors (kept away from the level colors), messages
// wrapped to the terminal width.
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
//   setLayout(LogLayout::Engine | Columns)
//   setTimeMode(TimeMode::Off | Uptime | WallClock)
//   setShowSource(bool)            file:line (Engine: right side, Columns: source columns)
//   setShowFunction(bool)          function column
//   setSourceMinLevel(level)       only show source info for levels >= level
//   setSourceForceLevel(level)     Engine layout: if file:line doesn't fit on a line, levels >= this
//                                  (default Warn) still get it on their own line, lower levels omit it
//   setFuncWidth(n), setFileWidth(n)   column widths (default 24 / 24, file width is Columns only)
//   setColorFunctions(bool)        per-function colors on/off
//   setFunctionPalette({...})      colors used for function names (LogStyle: fg + bold), handed out in
//                                  order of first appearance, so the first N functions always differ
//   setWrap(bool)                  wrap long messages at the terminal edge (default on)
//   setLineWidth(n)                force a line width, 0 = detect the terminal (default)
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
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
    #include <sys/ioctl.h>
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

enum class LogLayout : uint8_t {
    Engine,    // time, tag, function, message ........ file:line (right-aligned)
    Columns    // time, tag, file:line, function, | message
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
    void setLayout(LogLayout layout)      { std::lock_guard<std::mutex> l(mutex_); layout_ = layout; }
    void setTimeMode(TimeMode mode)       { std::lock_guard<std::mutex> l(mutex_); timeMode_ = mode; }
    void setShowSource(bool on)           { std::lock_guard<std::mutex> l(mutex_); showSource_ = on; }
    void setShowFunction(bool on)         { std::lock_guard<std::mutex> l(mutex_); showFunc_ = on; }
    void setSourceMinLevel(LogLevel lvl)  { std::lock_guard<std::mutex> l(mutex_); sourceMinLevel_ = lvl; }
    void setSourceForceLevel(LogLevel lvl){ std::lock_guard<std::mutex> l(mutex_); sourceForceLevel_ = lvl; }
    void setFileWidth(size_t w)           { std::lock_guard<std::mutex> l(mutex_); fileWidth_ = w < 4 ? 4 : w; }
    void setFuncWidth(size_t w)           { std::lock_guard<std::mutex> l(mutex_); funcWidth_ = w < 4 ? 4 : w; }
    void setColorFunctions(bool on)       { std::lock_guard<std::mutex> l(mutex_); colorFuncs_ = on; }
    void setFunctionPalette(std::initializer_list<LogStyle> styles) {
        std::lock_guard<std::mutex> l(mutex_);
        setFunctionPaletteLocked(styles);
    }
    void setWrap(bool on)                 { std::lock_guard<std::mutex> l(mutex_); wrap_ = on; }
    void setLineWidth(size_t cols)        { std::lock_guard<std::mutex> l(mutex_); lineWidth_ = cols; }

    // Redirect output (e.g. a log file). Colors are auto-disabled if it isn't a terminal.
    void setOutput(FILE* out) {
        std::lock_guard<std::mutex> lock(mutex_);
        out_ = out ? out : stdout;
        isTty_ = LOGGER_ISATTY(out_) != 0;
        useColor_ = isTty_;
    }

    // ---- logging --------------------------------------------------------------

    // Without source info (source fields are left blank).
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
        isTty_ = LOGGER_ISATTY(out_) != 0;
        useColor_ = isTty_;

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

        // Function-name colors. Deliberately NOT red, green, purple, blue or amber, because those are the
        // level colors; each hue also comes in a bold variant, giving 8 distinct looks.
        const LogColor cyan  = LogColor::rgb( 80, 225, 235);
        const LogColor lime  = LogColor::rgb(205, 235,  90);
        const LogColor white = LogColor::rgb(245, 245, 245);
        const LogColor tan   = LogColor::rgb(225, 200, 150);
        setFunctionPaletteLocked({
            {cyan}, {lime}, {white}, {tan},
            {cyan, Colors::None, true}, {lime, Colors::None, true}, {white, Colors::None, true}, {tan, Colors::None, true},
        });
    }
    ~Logger() = default;

    void setFunctionPaletteLocked(std::initializer_list<LogStyle> styles) {
        funcEscapes_.clear();
        funcIndex_.clear(); // colors get reassigned from scratch
        for (const LogStyle& st : styles) funcEscapes_.push_back(buildEscape(st.fg, Colors::None, st.bold));
    }

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

        // 2) Build the whole output in one buffer and write it once.
        std::lock_guard<std::mutex> lock(mutex_);
        const LevelInfo& info = levels_[level];
        if (!info.used) return;

        const bool haveSrc = src && src->file && level >= sourceMinLevel_;
        const size_t termW = outputWidth();
        const size_t usable = termW > 1 ? termW - 1 : 0; // keep the last column free (avoids auto-wrap quirks)

        std::string& head = head_; head.clear(); // everything before the message on the first line
        std::string& cont = cont_; cont.clear(); // same, for continuation lines
        size_t col = 0;                          // visible width of `head`

        // time
        if (timeMode_ != TimeMode::Off) {
            char tb[48];
            int tn = formatTime(tb, sizeof(tb));
            if (useColor_) head += kDim;
            head.append(tb, static_cast<size_t>(tn));
            if (useColor_) head += kReset;
            head += ' ';
            col += static_cast<size_t>(tn) + 1;
        }

        // [ TAG ] centered in a fixed-width field
        {
            size_t pad   = tagWidth_ - info.name.size();
            size_t left  = pad / 2;
            size_t right = pad - left;
            if (useColor_) head += info.tagEscape;
            head += '[';
            head.append(left, ' ');
            head += info.name;
            head.append(right, ' ');
            head += ']';
            if (useColor_) head += kReset;
            head += ' ';
            col += tagWidth_ + 3;
        }

        // file:line text, shared by both layouts
        char loc[192];
        size_t locLen = 0;
        if (haveSrc) {
            int ln = std::snprintf(loc, sizeof(loc), "%s:%d", baseName(src->file), src->line);
            locLen = ln < 0 ? 0 : (static_cast<size_t>(ln) >= sizeof(loc) ? sizeof(loc) - 1 : static_cast<size_t>(ln));
        }

        if (layout_ == LogLayout::Engine) {
            if (showFunc_) {
                appendFuncField(head, haveSrc ? src->func : nullptr, funcWidth_);
                head.append(2, ' ');
                col += funcWidth_ + 2;
            }
            cont.append(col, ' ');
        } else { // Columns
            if (showSource_) {
                if (haveSrc) {
                    if (useColor_) head += kDim;
                    appendFit(head, loc, locLen, fileWidth_, /*keepEnd=*/true);
                    if (useColor_) head += kReset;
                } else {
                    head.append(fileWidth_, ' ');
                }
                head += ' ';
                col += fileWidth_ + 1;

                if (showFunc_) {
                    appendFuncField(head, haveSrc ? src->func : nullptr, funcWidth_);
                    head += ' ';
                    col += funcWidth_ + 1;
                }

                cont.append(col, ' ');
                if (useColor_) { head += kDim; cont += kDim; }
                head += '|';
                cont += '|';
                if (useColor_) { head += kReset; cont += kReset; }
                head += ' ';
                cont += ' ';
                col += 2;
            } else {
                cont.append(col, ' ');
            }
        }
        const size_t msgCol = col;

        // 3) Split the message into lines, then wrap each at the terminal edge.
        const size_t avail = (wrap_ && usable > msgCol + 20) ? usable - msgCol : 0; // 0 = unlimited
        auto& segs = segs_;
        segs.clear();
        const char* p   = msg;
        const char* end = msg + len;
        for (;;) {
            const char* nl = (end > p) ? static_cast<const char*>(std::memchr(p, '\n', static_cast<size_t>(end - p))) : nullptr;
            const char* le = nl ? nl : end;
            if (le > p && le[-1] == '\r') --le;

            if (avail == 0 || static_cast<size_t>(le - p) <= avail) {
                segs.emplace_back(p, static_cast<size_t>(le - p));
            } else {
                const char* s = p;
                while (static_cast<size_t>(le - s) > avail) {
                    size_t brk = avail; // hard cut unless there's a good break point in the second half of the window
                    for (size_t i = avail; i > avail / 2; --i) {
                        const char d = s[i - 1];
                        if (s[i] == ' ' || d == '|' || d == ',' || d == '/' || d == '\\') { brk = i; break; }
                    }
                    segs.emplace_back(s, brk);
                    s += brk;
                    while (s < le && *s == ' ') ++s;
                }
                if (s < le) segs.emplace_back(s, static_cast<size_t>(le - s));
            }
            if (!nl) break;
            p = nl + 1;
        }

        // Engine layout: where does file:line go? First line if it fits, else last line, else its own line.
        const bool wantSrc = layout_ == LogLayout::Engine && showSource_ && haveSrc && locLen > 0;
        long attach = -1;
        if (wantSrc) {
            auto fits = [&](size_t i) { return usable == 0 || msgCol + segs[i].second + 2 + locLen <= usable; };
            if (fits(0))                       attach = 0;
            else if (fits(segs.size() - 1))    attach = static_cast<long>(segs.size() - 1);
            else if (level >= sourceForceLevel_) attach = -2; // important lines always get their location, on their own line
            else                               attach = -1; // otherwise leave it out rather than add a line
        }

        // 4) Assemble.
        const bool tintMsg = useColor_ && info.colorMessage;
        std::string& out = lineBuf_;
        out.clear();
        for (size_t i = 0; i < segs.size(); ++i) {
            out += (i == 0) ? head : cont;
            if (tintMsg) out += info.msgEscape;
            out.append(segs[i].first, segs[i].second);
            if (tintMsg) out += kReset;

            if (attach == static_cast<long>(i)) {
                size_t gap = usable ? usable - (msgCol + segs[i].second) - locLen : 3;
                out.append(gap, ' ');
                if (useColor_) out += kDim;
                out.append(loc, locLen);
                if (useColor_) out += kReset;
            }
            out += '\n';
        }
        if (attach == -2) {
            out.append(msgCol, ' ');
            if (useColor_) out += kDim;
            out += "at ";
            out.append(loc, locLen);
            if (useColor_) out += kReset;
            out += '\n';
        }

        std::fwrite(out.data(), 1, out.size(), out_);
        if (level >= Level::Error) std::fflush(out_); // don't lose errors on a crash
    }

    // Width to wrap at. 0 = don't wrap (not a terminal, wrapping off, or width unknown).
    size_t outputWidth() const {
        if (!wrap_) return 0;
        if (lineWidth_ != 0) return lineWidth_;
        if (!isTty_) return 0;
#if defined(_WIN32)
        HANDLE h = GetStdHandle(out_ == stderr ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (h != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(h, &csbi)) {
            return static_cast<size_t>(csbi.srWindow.Right - csbi.srWindow.Left + 1);
        }
        return 0;
#else
        struct winsize ws;
        if (ioctl(fileno(out_), TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) return ws.ws_col;
        return 0;
#endif
    }

    // Appends exactly `width` visible characters: padded with spaces if short,
    // cut with a '~' if long (keepEnd keeps the tail, e.g. the line number).
    static void appendFit(std::string& out, const char* s, size_t len, size_t width, bool keepEnd) {
        if (len <= width) {
            out.append(s, len);
            out.append(width - len, ' ');
        } else if (keepEnd) {
            out += '~';
            out.append(s + (len - (width - 1)), width - 1);
        } else {
            out.append(s, width - 1);
            out += '~';
        }
    }

    // Function name in its own color, fixed width. nullptr = blank field.
    void appendFuncField(std::string& out, const char* func, size_t width) {
        if (!func) {
            out.append(width, ' ');
            return;
        }
        const bool tint = useColor_ && colorFuncs_ && !funcEscapes_.empty();
        if (tint) out += funcEscape(func);
        appendFit(out, func, std::strlen(func), width, /*keepEnd=*/false);
        if (tint) out += kReset;
    }

    // Colors are handed out in order of first appearance, so the first N different functions
    // always get N different colors (N = palette size); after that they repeat.
    const std::string& funcEscape(const char* func) {
        std::string_view key(func);
        auto it = funcIndex_.find(key);
        if (it == funcIndex_.end()) it = funcIndex_.emplace(std::string(key), funcIndex_.size()).first;
        return funcEscapes_[it->second % funcEscapes_.size()];
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
    std::string                            lineBuf_;          // reused buffers, guarded by mutex_
    std::string                            head_;
    std::string                            cont_;
    std::vector<std::pair<const char*, size_t>> segs_;
    std::vector<std::string>               funcEscapes_;      // ANSI escape per palette entry
    std::map<std::string, size_t, std::less<>> funcIndex_;    // function name -> palette slot
    std::chrono::steady_clock::time_point  start_;
    FILE*                                  out_            = stdout;
    size_t                                 tagWidth_       = 0;   // longest level name
    size_t                                 fileWidth_      = 24;  // "file.cpp:123" column (Columns layout)
    size_t                                 funcWidth_      = 24;  // function name column
    size_t                                 lineWidth_      = 0;   // 0 = detect terminal width
    LogLayout                              layout_         = LogLayout::Engine;
    TimeMode                               timeMode_       = TimeMode::Uptime;
    LogLevel                               sourceMinLevel_ = Level::Trace; // show source info for all levels
    LogLevel                               sourceForceLevel_ = Level::Warn; // Engine: >= this always shows file:line
    bool                                   isTty_          = false;
    bool                                   useColor_       = true;
    bool                                   showSource_     = true;
    bool                                   showFunc_       = true;
    bool                                   colorFuncs_     = true;
    bool                                   wrap_           = true;
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
