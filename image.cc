//
// Created by Taisuke Miyazaki on 2022/08/16.
//

#include "gazosan.h"

namespace gazosan {

cv::Rect ImageSegment::rect_from(const cv::Point &upper_left) const {
    return cv::Rect(upper_left, cv::Point(upper_left.x + area.width, upper_left.y + area.height));
}

void load_image(Context &ctx) {
    Timer t(ctx, "load image");

    tbb::task_group tg;
    tg.run([&]() {
        if (!ctx.arg.new_file.empty()) {
            ctx.new_file.reset(MappedFile<Context>::must_open(ctx, ctx.arg.new_file));
            ctx.new_color_mat = decode_from_mapped_file(*ctx.new_file, cv::IMREAD_COLOR);
            cv::cvtColor(ctx.new_color_mat, ctx.new_gray_mat, cv::COLOR_BGR2GRAY);
        }
    });

    tg.run([&]() {
        if (!ctx.arg.old_file.empty()) {
            ctx.old_file.reset(MappedFile<Context>::must_open(ctx, ctx.arg.old_file));
            ctx.old_color_mat = decode_from_mapped_file(*ctx.old_file, cv::IMREAD_COLOR);
            cv::cvtColor(ctx.old_color_mat, ctx.old_gray_mat, cv::COLOR_BGR2GRAY);
        }
    });
    tg.wait();
}

cv::Mat decode_from_mapped_file(const MappedFile<Context>& mapped_file, int flags = cv::IMREAD_UNCHANGED) {
    return cv::imdecode(cv::Mat(1, mapped_file.size, CV_8UC1, mapped_file.data), flags);
}

std::variant<bool, std::string>
check_histogram_differential(Context &ctx) {
    Timer t(ctx, "check histogram differential");

    auto preprocess_image = [&](const cv::Mat& color_mat) {
        Timer t2(ctx, "preprocess image");
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

    if (ctx.old_color_mat.empty() || ctx.new_color_mat.empty()) {
        return "failed to decode image file";
    }

    cv::Mat hist_old_mat = preprocess_image(ctx.old_color_mat);
    cv::Mat hist_new_mat = preprocess_image(ctx.new_color_mat);
    return cv::compareHist(hist_old_mat, hist_new_mat, 1) - 0.00001 <= 1e-13;
}

std::vector<cv::Rect> split_segments(Context& ctx, const cv::Mat& gray_mat, const cv::Mat& color_mat, i32 threshold) {
    Timer t(ctx, "split segments");

    Timer t_image(ctx, "image processing", &t);
    // binarization
    cv::Mat bin_mat;
    cv::threshold(gray_mat, bin_mat, threshold, 255, cv::THRESH_BINARY);

    // morphology
    cv::Mat grd_mat;
    // If the Size is too small, each part will be too detailed, so we use 3x3.
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
        cv::drawContours(markers, contours, idx, cv::Scalar::all(++labels), -1, cv::LINE_8, hierarchy, INT_MAX);
    }
    markers = markers + 1;
    // "watershed" regard 0 as unknown, so set un-labeled area to 1
    cv::watershed(color_mat, markers);

    t_image.stop();

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

    Timer t_grouping(ctx, "grouping", &t);
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

void detect_segments(Context &ctx) {
    Timer t(ctx, "detect segments");

    auto compute_descriptor = [&ctx](const cv::Mat& img) -> std::optional<cv::Mat> {
        if (img.empty()) return std::nullopt;

        std::vector<cv::KeyPoint> keypoint;
        cv::Mat descriptor;
        ctx.algorithm->detect(img, keypoint);
        if (keypoint.empty()) return std::nullopt;

        ctx.algorithm->compute(img, keypoint, descriptor);
        descriptor.convertTo(descriptor, CV_32F);
        return descriptor;
    };

    auto do_detect = [&](const cv::Mat& gray_mat, const cv::Mat& color_mat, std::vector<ImageSegment>& result) {
        Timer t2(ctx, "do detect", &t);
        auto segment_tmp = split_segments(ctx, gray_mat, color_mat, ctx.arg.bin_threshold);

        for (auto segment : segment_tmp) {
            auto roi = gray_mat(segment);
            result.emplace_back(segment, roi);
        }

        Timer t4(ctx, "compute descriptors", &t2);
        tbb::parallel_for_each(result, [&](ImageSegment& segment) {
            if (auto desc = compute_descriptor(segment.roi))
                segment.descriptor = *desc;
        });
        t4.stop();
    };

    tbb::task_group tg;
    tg.run([&]() {
        do_detect(ctx.old_gray_mat, ctx.old_color_mat, ctx.old_segments);
    });
    tg.run([&]() {
        do_detect(ctx.new_gray_mat, ctx.new_color_mat, ctx.new_segments);
    });
    tg.wait();
}

void save_segments(Context &ctx) {
    auto do_save = [&](const std::string& prefix, const cv::Mat &base_mat, const std::vector<ImageSegment> &segments) {
        for (int i = 0; const auto &image_segment: segments) {
            i++;

            auto file_name = ctx.arg.output_name + "/" + prefix + "/" + std::to_string(i) + ".png";
            cv::imwrite(file_name, base_mat(image_segment.area));
        }
    };

    do_save("old", ctx.old_color_mat, ctx.old_segments);
    do_save("new", ctx.new_color_mat, ctx.new_segments);
}


bool descriptor_match(Context& ctx, const cv::Mat& descriptor1, const cv::Mat& descriptor2) {
    auto matcher = cv::DescriptorMatcher::create(cv::DescriptorMatcher::FLANNBASED);

    std::vector<cv::DMatch> matched, match12, match21;
    matcher->match(descriptor1, descriptor2, match12);
    matcher->match(descriptor2, descriptor1, match21);

    if (ctx.arg.cross_check) {
        for (auto forward : match12) {
            cv::DMatch backward = match21[forward.trainIdx];
            if (backward.trainIdx == forward.queryIdx)
                matched.push_back(forward);
        }

        std::sort(matched.begin(), matched.end());
        return !matched.empty() && matched[matched.size() / 2].distance <= 1.0f;
    }

    std::sort(match12.begin(), match12.end());
    std::sort(match21.begin(), match21.end());

    // descriptor1 -> descriptor2 match or descriptor2 -> descriptor1 match
    float threshold = 1.0f;
    return (!match12.empty() && match12[match12.size() / 2].distance <= threshold) ||
           (!match21.empty() && match21[match21.size() / 2].distance <= threshold);
}

void create_diff_image(Context& ctx) {
    Timer t(ctx, "create diff image");

    cv::Mat result = decode_from_mapped_file(*ctx.old_file, cv::IMREAD_COLOR);

    // this is not parallel for atomicity.
    for (auto& image_segment1 : ctx.old_segments) {
        tbb::parallel_for_each(ctx.new_segments, [&](ImageSegment& image_segment2) {
            if (image_segment1.descriptor.empty() ||
                image_segment2.descriptor.empty() ||
                image_segment2.matched ||
                !descriptor_match(ctx, image_segment1.descriptor, image_segment2.descriptor))
                return;

            image_segment1.matched = true;
            image_segment2.matched = true;

            // find `new` parts from `old` image
            cv::Mat ret;
            cv::Point min_point;
            cv::matchTemplate(ctx.old_gray_mat, image_segment2.roi, ret, cv::TM_SQDIFF);
            cv::minMaxLoc(ret, nullptr, nullptr, &min_point, nullptr);
            // Note: パーツのマッチングから対応する位置関係を取得できないか？

            auto area = image_segment2.area;
            auto rect = image_segment2.rect_from(min_point);
            cv::rectangle(result, rect, CV_RGB(255, 0, 0), 1);
            for (int i = 0; i < rect.height; i++) {
                for (int j = 0; j < rect.width; j++) {
                    auto old_cropped = ctx.old_color_mat.at<cv::Vec3b>(i + rect.y, j + rect.x);
                    auto new_cropped = ctx.new_color_mat.at<cv::Vec3b>(i + area.y, j + area.x);

                    if (old_cropped != new_cropped) {
                        result.at<cv::Vec3b>(i + rect.y, j + rect.x) = cv::Vec3b(0, 0, 255);
                    }
                }
            }
        });
    }
    Timer t2(ctx, "write");
    cv::imwrite(ctx.arg.output_name + "_diff.png", result);

    auto draw_not_matched = [&](cv::Mat result, const std::vector<ImageSegment>& segments) {
        tbb::parallel_for_each(segments, [&](const ImageSegment& image_segment) {
            if (image_segment.matched)
                return;

            cv::rectangle(result, image_segment.area, CV_RGB(0, 255, 0), 2);
        });
    };

    if (ctx.arg.create_change_image) {
        Timer t3(ctx, "create added and deleted image");
        tbb::task_group tg;
        tg.run([&]() {
            cv::Mat deleted = decode_from_mapped_file(*ctx.old_file, cv::IMREAD_COLOR);
            draw_not_matched(deleted, ctx.old_segments);
            cv::imwrite(ctx.arg.output_name + "_deleted.png", deleted);
        });
        tg.run([&]() {
            cv::Mat added = decode_from_mapped_file(*ctx.new_file, cv::IMREAD_COLOR);
            draw_not_matched(added, ctx.new_segments);
            cv::imwrite(ctx.arg.output_name + "_added.png", added);
        });
        tg.wait();
    }
}


} // namespace gazosan
