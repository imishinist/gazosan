//
// Created by Taisuke Miyazaki on 2022/08/15.
//

#include "gazo-san.h"

using namespace gazosan;

int main(int argc, char **argv) {
    Context ctx;
    Timer t_all(ctx, "all");

    for (int i = 0; i < argc; i++)
        ctx.cmdline_args.emplace_back(argv[i]);
    parse_args(ctx);

    load_image(ctx);

    std::variant<bool, std::string> diff_check = check_histogram_differential(ctx);
    if (!std::holds_alternative<bool>(diff_check)) {
        std::string err = std::get<std::string>(diff_check);
        Fatal(ctx) << err;
    }

    bool is_same = std::get<bool>(diff_check);
    if (is_same) {
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

