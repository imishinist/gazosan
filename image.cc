//
// Created by Taisuke Miyazaki on 2022/08/16.
//

#include "gazo-san.h"

namespace gazosan {

void load_image(Context &ctx) {
    if (!ctx.arg.new_file.empty()) {
        ctx.new_file.reset(MappedFile<Context>::must_open(ctx, ctx.arg.new_file));
    }
    if (!ctx.arg.old_file.empty()) {
        ctx.old_file.reset(MappedFile<Context>::must_open(ctx, ctx.arg.old_file));
    }
}

static inline
cv::Mat decode_from_mapped_file(const MappedFile<Context>& mapped_file) {
    return cv::imdecode(cv::Mat(1, mapped_file.size, CV_8UC1, mapped_file.data), cv::IMREAD_UNCHANGED);
}

std::variant<bool, std::string>
check_histogram_differential(Context &ctx) {
    auto preprocess_image = [](const cv::Mat& color_mat) {
        cv::Mat hsv_mat, output;
        cv::cvtColor(color_mat, hsv_mat, cv::COLOR_BGR2HSV);

        int channels[] = {0,1};
        float h_ranges[] = {0, 180};;
        float s_ranges[] = {0, 256};;
        const int hist_size[] = {256, 256};
        const float *ranges[] = {h_ranges, s_ranges};

        cv::calcHist(&hsv_mat, 1, channels, cv::Mat(), output, 2, hist_size, ranges, true, false);
        cv::normalize(output, output, 0, 1, cv::NORM_MINMAX, -1, cv::Mat());
        return output;
    };

    cv::Mat color_old_mat = decode_from_mapped_file(*ctx.old_file);
    cv::Mat color_new_mat = decode_from_mapped_file(*ctx.new_file);
    if (color_old_mat.empty() || color_new_mat.empty()) {
        return "failed to decode image file";
    }

    cv::Mat hist_old_mat = preprocess_image(color_old_mat);
    cv::Mat hist_new_mat = preprocess_image(color_new_mat);
    return cv::compareHist(hist_old_mat, hist_new_mat, 1) - 0.00001 <= 1e-13;
}

} // namespace gazosan