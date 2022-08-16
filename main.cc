//
// Created by Taisuke Miyazaki on 2022/08/15.
//

#include "gazo-san.h"

using namespace gazosan;

std::vector<cv::Rect>
split_segments(const MappedFile<Context>& mapped_file, i32 threshold) {
    // binarization
    cv::Mat bin_mat;
    cv::Mat gray_mat = decode_from_mapped_file(mapped_file, cv::IMREAD_GRAYSCALE);
    cv::threshold(gray_mat, bin_mat, threshold, 255, cv::THRESH_BINARY);

    // morphology
    cv::Mat grd_mat;
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3,3));
    cv::morphologyEx(bin_mat, grd_mat, cv::MORPH_GRADIENT, kernel, cv::Point(-1,-1), 7);

    // list of contour
    // [[[x1, y1],[x2, y2], ...]]
    // 00100
    // 01100
    // 01001
    // [ [[2, 0], [1, 1], [2, 1], [1, 2]],
    //   [[4, 2]]]
    std::vector<std::vector<cv::Point>> contours;

    // cv::Vec4i => [next, previous, first_child, parent]
    // PETR_CCOMP example:
    // [0:[ 1 -1 -1 -1]
    //  1:[ 3 0 2 -1]
    //  2:[-1 -1 -1 1]
    //  3:[ 4 1 -1 -1]
    //  4:[-1 3 -1 -1]]
    // "-": next, "=": child
    // 0 ---> 1 ---> 3 ---> 4
    //        + ===> 2
    std::vector<cv::Vec4i> hierarchy;
    grd_mat.convertTo(grd_mat, CV_32SC1, 1.0);
    cv::findContours(grd_mat, contours, hierarchy, cv::RETR_CCOMP, cv::CHAIN_APPROX_SIMPLE);
    if (contours.empty()) {
        return {};
    }

    int labels = 0;
    cv::Mat markers = cv::Mat::zeros(grd_mat.rows, grd_mat.cols, CV_32SC1);
    for (int idx = 0; idx >= 0; idx = hierarchy[idx][0]) {
        // hierarchy index correspond with contours
        cv::drawContours(markers, contours, idx, cv::Scalar::all(++labels), -1, 8, hierarchy, INT_MAX);
    }
    markers = markers + 1;
    // "watershed" regard 0 as unknown, so set un-labeled area to 1
    cv::Mat color_mat = decode_from_mapped_file(mapped_file, cv::IMREAD_COLOR);
    cv::watershed(color_mat, markers);

    // overwrite markers
    auto bfs = [&](int sx, int sy, auto&& bfs) -> std::optional<cv::Rect> {
        cv::Point minP(sx, sy), maxP(sx, sy);

        std::queue<std::pair<int, int>> Q;
        Q.push(std::make_pair(sx, sy));
        while (!Q.empty()) {
            auto [x, y] = Q.front(); Q.pop();

            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = x + dx, ny = y + dy;

                    // skip myself
                    if (dx == 0 && dy == 0) continue;
                    // boundary check
                    if (nx < 0 || nx >= markers.cols) continue;
                    if (ny < 0 || ny >= markers.rows) continue;

                    int label = markers.at<int>(ny, nx);
                    // border: -1, background: 1, already processed: 0
                    if (label <= 1) continue;
                    // mark processed
                    markers.at<int>(ny, nx) = 0;

                    minP.x = std::min(minP.x, nx);
                    minP.y = std::min(minP.y, ny);
                    maxP.x = std::max(maxP.x, nx);
                    maxP.y = std::max(maxP.y, ny);

                    Q.push(std::make_pair(nx, ny));
                }
            }
        }
        if ((minP.x == sx && minP.y == sy) && (maxP.x == sx && maxP.y == sy)) {
            return std::nullopt;
        }
        return cv::Rect(minP, maxP);
    };

    // grouping pixels and calculate group rectangle
    std::vector<cv::Rect> segments;
    for (int y = 0; y < markers.rows; y++) {
        for (int x = 0; x < markers.cols; x++) {
            int label = markers.at<int>(y, x);
            if (label <= 1) continue;

            if (auto rect = bfs(x, y, bfs))
                segments.push_back(*rect);
        }
    }

    return segments;
}

int main(int argc, char **argv) {
    Context ctx;
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

    auto segments = split_segments(*ctx.old_file, ctx.arg.bin_threshold);
    cv::Mat color_mat = decode_from_mapped_file(*ctx.old_file, cv::IMREAD_COLOR);
    for (auto segment : segments) {
        cv::rectangle(color_mat, segment, cv::Scalar(255, 0, 0), 2, 8);
    }
    cv::imwrite("old_segments.png", color_mat);

    return 0;
}

