//
// Created by Taisuke Miyazaki on 2022/08/15.
//

#include "gazosan.h"

#include <tbb/global_control.h>

using namespace gazosan;

namespace gazosan {

std::size_t get_default_thread_count()
{
    constexpr std::size_t default_thread = 16;
    const std::size_t n = tbb::global_control::active_value(tbb::global_control::max_allowed_parallelism);
    return std::min(n, default_thread);
}

} // namespace gazosan

int main(const int argc, char** argv)
{
    Context ctx;
    const Timer t_all(ctx, "all");
    for (int i = 0; i < argc; i++)
        ctx.cmdline_args.emplace_back(argv[i]);
    parse_args(ctx);

    tbb::global_control tbb_cont(tbb::global_control::max_allowed_parallelism, ctx.arg.thread_count);

    load_image(ctx);

    const std::variant<bool, std::string> diff_check = check_histogram_differential(ctx);
    if (!std::holds_alternative<bool>(diff_check)) {
        const std::string err = std::get<std::string>(diff_check);
        Fatal(ctx) << err;
    }

    if (std::get<bool>(diff_check)) {
        Fatal(ctx) << "two images are same";
    }

    detect_segments(ctx);
    // save_segments(ctx);

    create_diff_image(ctx);

    t_all.stop();
    if (ctx.arg.perf)
        print_timer_records(ctx.timer_records);

    return 0;
}
