#pragma once

#include <QString>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dctk.h>

#include <memory>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

// 患者与序列元信息（对应 Python 版 PatientInfo 提取逻辑）
struct PatientInfo {
    QString patientId;
    QString patientName;
    QString studyDate;
    QString studyDescription;
    QString seriesDescription;
    QString modality;
    double pixelSpacingX = 0.0; // mm/px
    double pixelSpacingY = 0.0;
    bool isColor = false;
    bool isUltrasound = false;
    bool isCtLike = false; // CT/CTA 类：自动窗兜底 800/100
    QString sopClassUid;
    int frames = 0;
    int width = 0;
    int height = 0;
    QString transferSyntax;
};

// DICOM 序列：目录加载、五级回退排序、按帧解码
class DicomSeries
{
public:
    // 扫描目录（递归）加载全部 DICOM 文件并排序；失败返回 nullptr
    static std::shared_ptr<DicomSeries> fromDirectory(const QString& dirPath);

    int frameCount() const { return static_cast<int>(frameRefs_.size()); }
    const PatientInfo& patientInfo() const { return info_; }
    QString fileNameAt(int index) const;

    // 第 frame 帧灰度（物理值，float32），应用 Rescale slope/intercept
    cv::Mat loadFrameGray32(int frame);

    // 第 frame 帧显示用像素（彩色 BGR 或 8 位灰度）
    cv::Mat loadFramePixels(int frame);

private:
    struct FrameRef {
        std::string path;     // 文件路径
        int frameInFile = 0;  // 多帧文件内的帧号
    };

    static long sortKeyFor(const std::string& filePath, DcmFileFormat& dcm);
    static bool openFile(const std::string& path, DcmFileFormat& dcm);
    void extractInfo(DcmFileFormat& dcm, DcmDataset* dataset);
    bool ensureFileCached(int frame) const;

    // 按顺序排列后的文件列表（单帧文件一一对应；多帧展开为 frameRefs_）
    std::vector<std::string> files_;
    std::vector<FrameRef> frameRefs_;
    PatientInfo info_;
    bool infoFilled_ = false;

    // 元数据缓存：避免重复解析 dataset
    mutable DcmFileFormat cached_;
    mutable std::string cachedPath_;
};
