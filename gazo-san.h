//
// Created by Taisuke Miyazaki on 2022/08/15.
//

#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <sstream>
#include <mutex>

#include <cstdlib>
#include <unistd.h>

namespace gazosan {

template <typename C>
class SyncOut {
public:
    SyncOut(C &ctx, std::ostream &out = std::cout) : out(out) {}

    ~SyncOut() {
        std::scoped_lock lock(mu);
        out << ss.str() << "\n";
    }

    template <class T> SyncOut &operator<<(T &&val) {
        ss << std::forward<T>(val);
        return *this;
    }

    static inline std::mutex mu;

private:
    std::ostream &out;
    std::stringstream ss;
};

template <typename C>
static std::string add_color(C &ctx, std::string msg) {
    if (ctx.arg.color_diagnostics)
        return "gazo-san: \033[0;1;31m" + msg + ":\033[0m ";
    return "gazo-san: " + msg + ": ";
}

template <typename C>
class Fatal {
public:
    Fatal(C &ctx) : out(ctx, std::cerr) {
        out << add_color(ctx, "fatal");
    }

    [[noreturn]] ~Fatal() {
        out.~SyncOut();
        _exit(1);
    }

    template <class T> Fatal &operator<<(T &&val) {
        out << std::forward<T>(val);
        return *this;
    }
private:
    SyncOut<C> out;
};

typedef struct Context {
    Context() = default;

    Context(const Context &) = delete;

    struct {
        bool color_diagnostics = false;
        std::string new_file;
        std::string old_file;
        std::string output_name;
    } arg;
    std::vector<std::string_view> cmdline_args;
} Context;


void parse_args(Context &ctx);

} // namespace gazosan
