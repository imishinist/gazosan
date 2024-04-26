//
// Created by Taisuke Miyazaki on 2022/08/15.
//

#pragma once

#include <iostream>
#include <mutex>
#include <numeric>
#include <optional>
#include <queue>
#include <sstream>
#include <stack>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <cstdlib>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <opencv2/features2d.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <tbb/concurrent_vector.h>
#include <tbb/parallel_for_each.h>
#include <tbb/tbb.h>

namespace gazosan {

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

std::string_view errno_string();

template <typename C>
class SyncOut {
public:
    explicit SyncOut(C& ctx, std::ostream& out = std::cout)
        : out(out)
    {
    }

    ~SyncOut()
    {
        std::scoped_lock lock(mu);
        out << ss.str() << "\n";
    }

    template <class T>
    SyncOut& operator<<(T&& val)
    {
        ss << std::forward<T>(val);
        return *this;
    }

    static inline std::mutex mu;

private:
    std::ostream& out;
    std::stringstream ss;
};

template <typename C>
static std::string add_color(C& ctx, std::string msg)
{
    if (ctx.arg.color_diagnostics)
        return "gazo-san: \033[0;1;31m" + msg + ":\033[0m ";
    return "gazo-san: " + msg + ": ";
}

template <typename C>
class Fatal {
public:
    explicit Fatal(C& ctx)
        : out(ctx, std::cerr)
    {
        out << add_color(ctx, "fatal");
    }

    [[noreturn]] ~Fatal()
    {
        out.~SyncOut();
        _exit(1);
    }

    template <class T>
    Fatal& operator<<(T&& val)
    {
        out << std::forward<T>(val);
        return *this;
    }

private:
    SyncOut<C> out;
};

struct TimerRecord {
    TimerRecord(std::string name, TimerRecord* parent = nullptr);
    void stop();

    std::string name;
    TimerRecord* parent;
    tbb::concurrent_vector<TimerRecord*> children;
    i64 start;
    i64 end;
    i64 user;
    i64 sys;
    bool stopped = false;
};

void print_timer_records(tbb::concurrent_vector<std::unique_ptr<TimerRecord>>&);

template <typename C>
class Timer {
public:
    Timer(C& ctx, std::string name, Timer* parent = nullptr)
    {
        record = new TimerRecord(name, parent ? parent->record : nullptr);
        ctx.timer_records.push_back(std::unique_ptr<TimerRecord>(record));
    }

    Timer(const Timer&) = delete;

    ~Timer()
    {
        record->stop();
    }

    void stop()
    {
        record->stop();
    }

private:
    TimerRecord* record;
};

template <typename C>
class MappedFile {
public:
    static MappedFile* open(C& ctx, const std::string& path);
    static MappedFile* must_open(C& ctx, std::string path);

    ~MappedFile();

    std::string_view get_contents()
    {
        return std::string_view((char*)data, size);
    }

    std::string name;
    u8* data = nullptr;
    i64 size = 0;
    i64 mtime = 0;
};

template <typename C>
MappedFile<C>* MappedFile<C>::open(C& ctx, const std::string& path)
{
    int fd;
    fd = ::open(path.c_str(), O_RDONLY);

    if (fd == -1) {
        return nullptr;
    }

    struct stat st { };
    if (fstat(fd, &st) == -1)
        Fatal(ctx) << path << ": fstat failed: " << errno_string();

    auto* mf = new MappedFile;
    mf->name = path;
    mf->size = st.st_size;
#ifdef _WIN32
    // not supported
#elif defined(__APPLE__)
    mf->mtime = (u64)st.st_mtimespec.tv_sec * 1000000000 + st.st_mtimespec.tv_nsec;
#else
    mf->mtime = (u64)st.st_mtim.tv_sec * 1000000000 + st.st_mtim.tv_nsec;
#endif
    if (st.st_size > 0) {
        mf->data = (u8*)mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (mf->data == MAP_FAILED)
            Fatal(ctx) << path << ": mmap failed: " << errno_string();
    }
    close(fd);
    return mf;
}

template <typename C>
MappedFile<C>* MappedFile<C>::must_open(C& ctx, std::string path)
{
    if (MappedFile* mf = MappedFile::open(ctx, path)) {
        return mf;
    }
    Fatal(ctx) << "cannot open " << path << ": " << errno_string();
}

template <typename C>
MappedFile<C>::~MappedFile()
{
    if (size == 0)
        return;

    munmap(data, size);
}

class ImageSegment {
public:
    cv::Rect area;
    cv::Mat descriptor;
    cv::Mat roi; // gray

    bool matched = false;

    ImageSegment(cv::Rect area, cv::Mat roi)
        : area(area)
        , roi(roi) {};
    ImageSegment(cv::Rect area, cv::Mat descriptor, cv::Mat roi)
        : area(area)
        , descriptor(std::move(descriptor))
        , roi(roi) {};

    cv::Rect rect_from(const cv::Point& upper_left) const;
};

typedef struct Context {
    Context() = default;

    Context(const Context&) = delete;

    struct {
        bool color_diagnostics = false;
        std::string new_file;
        std::string old_file;
        std::string output_name;
        bool create_change_image = false;

        i32 bin_threshold = 200;
        bool cross_check = false;

        i64 thread_count = 0;
        bool perf = false;
    } arg;
    std::vector<std::string_view> cmdline_args;
    tbb::concurrent_vector<std::unique_ptr<TimerRecord>> timer_records;

    cv::Ptr<cv::AKAZE> algorithm = cv::AKAZE::create();

    std::unique_ptr<MappedFile<Context>> new_file;
    std::unique_ptr<MappedFile<Context>> old_file;

    cv::Mat new_color_mat;
    cv::Mat old_color_mat;

    cv::Mat new_gray_mat;
    cv::Mat old_gray_mat;

    tbb::concurrent_vector<ImageSegment> new_segments;
    tbb::concurrent_vector<ImageSegment> old_segments;
} Context;

i64 get_default_thread_count();

void parse_args(Context& ctx);
void load_image(Context& ctx);
cv::Mat decode_from_mapped_file(const MappedFile<Context>& mapped_file, int flags);
std::variant<bool, std::string> check_histogram_differential(Context& ctx);

void detect_segments(Context& ctx);
void save_segments(Context& ctx);
bool descriptor_match(Context& ctx, const cv::Mat& descriptor1, const cv::Mat& descriptor2);
void create_diff_image(Context& ctx);

} // namespace gazosan
