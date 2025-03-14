#include "gazosan.h"

#include <sys/resource.h>
#include <sys/time.h>

namespace gazosan {
static i64 now_nsec()
{
    timespec t{};
    clock_gettime(CLOCK_MONOTONIC, &t);
    return static_cast<i64>(t.tv_sec) * 1000000000 + t.tv_nsec;
}

static std::pair<i64, i64> get_usage()
{
    auto to_nsec = [](const struct timeval t) -> i64 {
        return static_cast<i64>(t.tv_sec) * 1000000000 + t.tv_usec * 1000;
    };

    rusage ru{};
    getrusage(RUSAGE_SELF, &ru);
    return { to_nsec(ru.ru_utime), to_nsec(ru.ru_stime) };
}

TimerRecord::TimerRecord(std::string name, TimerRecord* parent)
    : name(std::move(name))
    , parent(parent)
{
    start = now_nsec();
    std::tie(user, sys) = get_usage();
    if (parent)
        parent->children.push_back(this);
}

void TimerRecord::stop()
{
    if (stopped)
        return;
    stopped = true;

    auto [user2, sys2] = get_usage();

    end = now_nsec();
    user = user2 - user;
    sys = sys2 - sys;
}

static void print_rec(TimerRecord& rec, const i64 indent) // NOLINT(*-no-recursion)
{
    printf(" %8.3f %8.3f %8.3f  %s%s\n",
        static_cast<double>(rec.user) / 1000000000,
        static_cast<double>(rec.sys) / 1000000000,
        (static_cast<double>(rec.end) - static_cast<double>(rec.start)) / 1000000000,
        std::string(indent * 2, ' ').c_str(),
        rec.name.c_str());

    std::ranges::sort(rec.children, [](const TimerRecord* a, const TimerRecord* b) {
        return a->start < b->start;
    });

    for (TimerRecord* child : rec.children)
        print_rec(*child, indent + 1);
}

void print_timer_records(tbb::concurrent_vector<std::unique_ptr<TimerRecord>>& records)
{
    for (const auto & rec : records)
        rec->stop();

    for (i64 i = 0; i < records.size(); i++) {
        TimerRecord& inner = *records[i];
        if (inner.parent)
            continue;

        for (i64 j = i - 1; j >= 0; j--) {
            TimerRecord& outer = *records[j];
            if (outer.start <= inner.start && inner.end <= outer.end) {
                inner.parent = &outer;
                outer.children.push_back(&inner);
                break;
            }
        }
    }

    std::cout << "     User   System     Real  Name\n";

    for (std::unique_ptr<TimerRecord>& rec : records)
        if (!rec->parent)
            print_rec(*rec, 0);

    std::cout << std::flush;
}

} // namespace gazosan
