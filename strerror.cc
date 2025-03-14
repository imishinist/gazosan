//
// Created by Taisuke Miyazaki on 2022/08/16.
//

#include "gazosan.h"

namespace gazosan {

std::string_view errno_string()
{
    thread_local char buf[200];
    auto _ = strerror_r(errno, buf, sizeof(buf));
    return buf;
}

} // namespace gazosan
