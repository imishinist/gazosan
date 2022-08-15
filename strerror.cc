//
// Created by Taisuke Miyazaki on 2022/08/16.
//

#include "gazo-san.h"

namespace gazosan {

std::string_view errno_string() {
    static thread_local char buf[200];
    strerror_r(errno, buf, sizeof(buf));
    return buf;
}

} // namespace gazosan