#pragma once

#include <QString>

#include <opencv2/core.hpp>

// 图像处理管线：伪彩 LUT / 超声增强 / ITK 高斯平滑
class ImagePipeline
{
public:
    // 可用的伪彩映射名（10 种，对应 Python 伪彩表）
    static QStringList availableColorMaps();

    // 8 位灰度 -> 伪彩 BGR；未知映射名返回灰度三通道
    static cv::Mat applyPseudoColor(const cv::Mat& gray8, const QString& colorMap);

    // 超声增强：CLAHE（clipLimit=3.0, tile=8x8），对应 Python enhance_ultrasound
    static cv::Mat enhanceUltrasound(const cv::Mat& gray8);

    // ITK 递归高斯平滑（输入/输出 CV_32FC1）
    static cv::Mat itkGaussianSmooth(const cv::Mat& gray32, double sigma);
};
