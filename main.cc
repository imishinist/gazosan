//
// Created by Taisuke Miyazaki on 2022/08/15.
//

#include "gazo-san.h"

using namespace gazosan;

int main(int argc, char **argv) {
    Context ctx;
    for (int i = 0; i < argc; i++)
        ctx.cmdline_args.emplace_back(argv[i]);
    parse_args(ctx);

    return 0;
}

