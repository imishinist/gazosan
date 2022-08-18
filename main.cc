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

    cv::Mat old_mat = decode_from_mapped_file(*ctx.old_file, cv::IMREAD_GRAYSCALE);
    cv::Mat new_mat = decode_from_mapped_file(*ctx.new_file, cv::IMREAD_GRAYSCALE);

    puts("split segments(binarization, morphology, grouping labels)");
    detect_segments(ctx);
    // save_segments(ctx);

    cv::Mat result = decode_from_mapped_file(*ctx.old_file, cv::IMREAD_COLOR);
    puts("matching crop image");
    for (int i1 = 0; const auto& image_segment1 : ctx.old_segments) {
        i1++;
        printf("Old %d\n", i1);

        for (int i2 = 0; const auto& image_segment2 : ctx.new_segments) {
            i2++;
            if (descriptor_match(image_segment1.descriptor, image_segment2.descriptor)) {
                printf("  New %d\n", i2);
                puts("    Matched");

                // find `new` parts from `old` image
                cv::Mat ret;
                cv::Point min_point;
                cv::matchTemplate(old_mat, new_mat(image_segment2.area), ret, cv::TM_SQDIFF);
                cv::minMaxLoc(ret, nullptr, nullptr, &min_point, nullptr);

                auto rect = image_segment2.rect_from(min_point);
                cv::rectangle(result, rect, CV_RGB(255, 0, 0), 1);
                for (int i = 0; i < rect.height; i++) {
                    for (int j = 0; j < rect.width; j++) {
                        auto old_cropped = ctx.old_color_mat.at<cv::Vec3b>(i + rect.y, j + rect.x);
                        auto new_cropped = ctx.new_color_mat.at<cv::Vec3b>(i + image_segment2.area.y, j + image_segment2.area.x);

                        if (old_cropped != new_cropped) {
                            result.at<cv::Vec3b>(i + rect.y, j + rect.x) = cv::Vec3b(0, 0, 255);
                        }
                    }
                }

                break;
            }
        }
    }
    cv::imwrite("/tmp/foo.png", result);
    return 0;
}

