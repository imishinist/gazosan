//
// Created by Taisuke Miyazaki on 2022/08/15.
//

#include "gazo-san.h"

namespace gazosan {

static const char helpmsg[] = R"(
Options:
  -new <FILE>                 new image file path
  -old <FILE>                 old image file path
  -o, --output <NAME>         output prefix name (default: diff)
  -threshold <NUMBER>         binary threshold

  -h, --help                  report usage information
)";

void parse_args(Context &ctx) {
    std::vector<std::string_view> &args = ctx.cmdline_args;

    int i = 1;
    while (i < args.size()) {
        std::string_view arg;

        auto read_arg = [&](std::string name) {
            if (args[i] == name) {
                if (args.size() <= i + 1)
                    Fatal(ctx) << "option -" << name << ": argument missing";
                arg = args[i + 1];
                i += 2;
                return true;
            }
            return false;
        };
        auto read_flag = [&](std::string name) {
            if (args[i] == name) {
                i++;
                return true;
            }
            return false;
        };

        if (read_flag("-h") || read_flag("--help")) {
            std::cout << "Usage: " << ctx.cmdline_args[0]
                      << " [options]\n" << helpmsg;
            exit(0);
        }
        if (read_arg("-color-diagnostics") ||
            read_arg("--color-diagnostics")) {
            ctx.arg.color_diagnostics = true;
        } else if (read_arg("-new")) {
            ctx.arg.new_file = arg;
        } else if (read_arg("-old")) {
            ctx.arg.old_file = arg;
        } else if (read_arg("-threshold")) {
            ctx.arg.bin_threshold = std::stoi(std::string(arg));
        } else if (read_arg("-o") || read_arg("--output")) {
            ctx.arg.output_name = arg;
        } else {
            if (args[i][0] == '-')
                Fatal(ctx) << "unknown command line option: " << args[i];
            i++;
        }
    }
    if (ctx.arg.new_file.empty())
        Fatal(ctx) << "\"-new\" option is required";
    if (ctx.arg.old_file.empty())
        Fatal(ctx) << "\"-old\" option is required";
    if (ctx.arg.bin_threshold == 0)
        ctx.arg.bin_threshold = 200;
    if (ctx.arg.output_name.empty()) {
        ctx.arg.output_name = "diff";
    }
}

} // namespace gazosan