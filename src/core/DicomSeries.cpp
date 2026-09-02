#include "DicomSeries.h"

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <dcmtk/dcmimgle/dcmimage.h>
#include <opencv2/imgproc.hpp>

#include <algorithm>

// ---------------------------------------------------------------------------
// 排序键：五级回退链（对应 Python _sort_dicom_files）
// InstanceNumber -> AcquisitionNumber -> SOPInstanceUID -> 文件名末尾数字 -> 文件大小
// ---------------------------------------------------------------------------
long DicomSeries::sortKeyFor(const std::string& filePath, DcmFileFormat& dcm)
{
    DcmDataset* dataset = dcm.getDataset();

    // 1) InstanceNumber (0020,0013)
    long v = 0;
    if (dataset->findAndGetLongInt(DCM_InstanceNumber, v).good())
        return v;

    // 2) AcquisitionNumber (0020,0012)
    if (dataset->findAndGetLongInt(DCM_AcquisitionNumber, v).good())
        return v;

    // 3) SOPInstanceUID 数字部分（最后一段若为数字则取整）
    OFString sopUid;
    if (dataset->findAndGetOFString(DCM_SOPInstanceUID, sopUid).good() && !sopUid.empty()) {
        const QString uid = QString::fromUtf8(sopUid.c_str());
        const int lastDot = uid.lastIndexOf(QLatin1Char('.'));
        if (lastDot >= 0) {
            bool okNum = false;
            const long uidNum = uid.mid(lastDot + 1).toLong(&okNum);
            if (okNum)
                return uidNum;
        }
    }

    // 4) 文件名末尾数字
    const QString fileName = QFileInfo(QString::fromStdString(filePath)).fileName();
    const QRegularExpression re(QStringLiteral("\\d+"));
    QRegularExpressionMatchIterator it = re.globalMatch(fileName);
    QString lastNum;
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        lastNum = m.captured(0);
    }
    if (!lastNum.isEmpty()) {
        bool okNum = false;
        const long v2 = lastNum.toLong(&okNum);
        if (okNum)
            return v2;
    }

    // 5) 文件大小兜底
    QFile f(QString::fromStdString(filePath));
    if (f.open(QIODevice::ReadOnly))
        return static_cast<long>(f.size());
    return 0;
}

// ---------------------------------------------------------------------------
// 传输语法回退（对应 Python 的 force 读取链）
// 自动探测 -> 显式小端 -> 隐式小端 -> 显式大端
// ---------------------------------------------------------------------------
bool DicomSeries::openFile(const std::string& path, DcmFileFormat& dcm)
{
    const E_TransferSyntax fallbacks[] = {EXS_Unknown,
                                          EXS_LittleEndianExplicit,
                                          EXS_LittleEndianImplicit,
                                          EXS_BigEndianExplicit};
    for (E_TransferSyntax xfer : fallbacks) {
        dcm.clear();
        if (dcm.loadFile(path.c_str(), xfer).good())
            return true;
    }
    return false;
}

void DicomSeries::extractInfo(DcmFileFormat& dcm, DcmDataset* dataset)
{
    if (infoFilled_ || dataset == nullptr)
        return;

    auto ofString = [dataset](const DcmTagKey& key, QString* out) {
        OFString v;
        if (dataset->findAndGetOFString(key, v).good() && !v.empty())
            *out = QString::fromUtf8(v.c_str());
    };

    ofString(DCM_PatientID, &info_.patientId);
    ofString(DCM_PatientName, &info_.patientName);
    ofString(DCM_StudyDate, &info_.studyDate);
    ofString(DCM_StudyDescription, &info_.studyDescription);
    ofString(DCM_SeriesDescription, &info_.seriesDescription);
    ofString(DCM_Modality, &info_.modality);

    // Pixel Spacing（0028,0030）：[行间距, 列间距]
    // PixelSpacing 为 DS VR（字符串），按序号取值可正确完成转换
    Float64 spacingY = 0.0;
    if (dataset->findAndGetFloat64(DCM_PixelSpacing, spacingY, 0).good())
        info_.pixelSpacingY = spacingY;
    Float64 spacingX = 0.0;
    if (dataset->findAndGetFloat64(DCM_PixelSpacing, spacingX, 1).good())
        info_.pixelSpacingX = spacingX;

    ofString(DCM_SOPClassUID, &info_.sopClassUid);

    // SamplesPerPixel / PhotometricInterpretation 判色
    Uint16 samples = 1;
    dataset->findAndGetUint16(DCM_SamplesPerPixel, samples);
    OFString photo;
    dataset->findAndGetOFString(DCM_PhotometricInterpretation, photo);
    info_.isColor = (samples >= 3 || photo == "RGB" || photo == "YBR_FULL" ||
                     photo == "YBR_FULL_422" || photo == "PALETTE COLOR");

    // 模态分类
    const QString mod = info_.modality.toUpper();
    info_.isUltrasound = (mod == QLatin1String("US") || mod == QLatin1String("XA"));
    info_.isCtLike = (mod == QLatin1String("CT") || mod == QLatin1String("CTA"));

    // 帧数 / 尺寸
    dataset->findAndGetUint16(DCM_Rows, *reinterpret_cast<Uint16*>(&info_.height));
    dataset->findAndGetUint16(DCM_Columns, *reinterpret_cast<Uint16*>(&info_.width));

    // 传输语法
    E_TransferSyntax xfer = dcm.getDataset()->getOriginalXfer();
    info_.transferSyntax = QString::fromUtf8(DcmXfer(xfer).getXferName());

    infoFilled_ = true;
}

// ---------------------------------------------------------------------------
// 目录加载（对应 Python load_dicom_series）
// ---------------------------------------------------------------------------
std::shared_ptr<DicomSeries> DicomSeries::fromDirectory(const QString& dirPath)
{
    if (dirPath.isEmpty())
        return nullptr;

    // 收集候选文件（常见 DICOM 扩展名 + 无扩展名）
    QStringList nameFilters;
    const auto extensions = {QStringLiteral("*.dcm"), QStringLiteral("*.DCM"),
                             QStringLiteral("*.dicom"), QStringLiteral("*.dic"),
                             QStringLiteral("")};
    for (const auto& e : extensions)
        nameFilters << e;

    QDirIterator it(dirPath, QDir::Files, QDirIterator::Subdirectories);
    std::vector<std::string> candidates;
    while (it.hasNext()) {
        const QString path = it.next();
        if (QFileInfo(path).size() < 128) // DICOM 头最小尺寸，过滤明显非 DICOM 文件
            continue;
        candidates.push_back(path.toStdString());
    }
    if (candidates.empty())
        return nullptr;

    auto series = std::shared_ptr<DicomSeries>(new DicomSeries());

    // 解析 + 排序（稳定排序保证同键文件保持遍历顺序）
    struct Entry {
        std::string path;
        long key = 0;
        int framesInFile = 1;
    };
    std::vector<Entry> entries;
    entries.reserve(candidates.size());

    for (const auto& path : candidates) {
        DcmFileFormat dcm;
        if (!openFile(path, dcm))
            continue;

        DcmDataset* dataset = dcm.getDataset();
        if (dataset == nullptr)
            continue;

        // 必须有像素数据
        if (dataset->tagExistsWithValue(DCM_PixelData) == 0)
            continue;

        // 提取元信息（首个有效文件）
        series->extractInfo(dcm, dataset);

        // 多帧文件展开
        long frames = 1;
        if (!dataset->findAndGetLongInt(DCM_NumberOfFrames, frames).good() || frames < 1)
            frames = 1;

        entries.push_back({path, sortKeyFor(path, dcm), static_cast<int>(frames)});
    }

    if (entries.empty())
        return nullptr;

    std::stable_sort(entries.begin(), entries.end(),
                     [](const Entry& a, const Entry& b) { return a.key < b.key; });

    for (const auto& e : entries) {
        series->files_.push_back(e.path);
        for (int f = 0; f < e.framesInFile; ++f)
            series->frameRefs_.push_back({e.path, f});
    }

    series->info_.frames = static_cast<int>(series->frameRefs_.size());
    return series;
}

QString DicomSeries::fileNameAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(files_.size()))
        return {};
    return QFileInfo(QString::fromStdString(files_[index])).fileName();
}

// ---------------------------------------------------------------------------
// 帧解码（DcmFileFormat 元数据缓存 + DicomImage 按帧解码）
// ---------------------------------------------------------------------------
bool DicomSeries::ensureFileCached(int frame) const
{
    if (frame < 0 || frame >= static_cast<int>(frameRefs_.size()))
        return false;

    const FrameRef& ref = frameRefs_[frame];
    if (cachedPath_ == ref.path)
        return true;

    DcmFileFormat dcm;
    if (!openFile(ref.path, dcm))
        return false;

    cached_ = std::move(dcm);
    cachedPath_ = ref.path;
    return true;
}

cv::Mat DicomSeries::loadFrameGray32(int frame)
{
    if (!ensureFileCached(frame))
        return {};

    const FrameRef& ref = frameRefs_[frame];
    DicomImage img(ref.path.c_str(), 0, static_cast<unsigned long>(ref.frameInFile), 1);
    if (img.getStatus() != EIS_Normal || img.getFrameCount() < 1)
        return {};

    const int w = static_cast<int>(img.getWidth());
    const int h = static_cast<int>(img.getHeight());
    if (w <= 0 || h <= 0)
        return {};

    // 彩色转灰度
    if (!img.isMonochrome()) {
        const size_t count = static_cast<size_t>(w) * h * 3;
        std::vector<Uint8> rgb(count);
        if (img.getOutputData(rgb.data(), count, 8, 0) == 0)
            return {};
        cv::Mat rgbMat(h, w, CV_8UC3, rgb.data());
        cv::Mat gray8;
        cv::cvtColor(rgbMat, gray8, cv::COLOR_RGB2GRAY);
        cv::Mat gray32;
        gray8.convertTo(gray32, CV_32FC1);
        return gray32;
    }

    // 单色：16 位 + Rescale
    DcmDataset* ds = cached_.getDataset();
    double slope = 1.0, intercept = 0.0;
    if (ds) {
        ds->findAndGetFloat64(DCM_RescaleSlope, slope);
        ds->findAndGetFloat64(DCM_RescaleIntercept, intercept);
    }

    const size_t count = static_cast<size_t>(w) * h;
    std::vector<Uint16> raw(count);
    if (img.getOutputData(raw.data(), count * sizeof(Uint16), 16, 0) == 0)
        return {};

    cv::Mat out(h, w, CV_32FC1);
    const float fs = static_cast<float>(slope);
    const float fi = static_cast<float>(intercept);
    float* dst = out.ptr<float>();
    for (size_t i = 0; i < count; ++i)
        dst[i] = static_cast<float>(raw[i]) * fs + fi;
    return out;
}

cv::Mat DicomSeries::loadFramePixels(int frame)
{
    if (!ensureFileCached(frame))
        return {};

    const FrameRef& ref = frameRefs_[frame];
    DicomImage img(ref.path.c_str(), 0, static_cast<unsigned long>(ref.frameInFile), 1);
    if (img.getStatus() != EIS_Normal || img.getFrameCount() < 1)
        return {};

    const int w = static_cast<int>(img.getWidth());
    const int h = static_cast<int>(img.getHeight());
    if (w <= 0 || h <= 0)
        return {};

    if (!img.isMonochrome()) {
        const size_t count = static_cast<size_t>(w) * h * 3;
        std::vector<Uint8> rgb(count);
        if (img.getOutputData(rgb.data(), count, 8, 0) == 0)
            return {};
        cv::Mat rgbMat(h, w, CV_8UC3, rgb.data());
        cv::Mat bgr;
        cv::cvtColor(rgbMat, bgr, cv::COLOR_RGB2BGR);
        return bgr;
    }

    // 单色：返回原始物理值归一化的 8 位灰度（供导出/减影使用）
    cv::Mat gray32 = loadFrameGray32(frame);
    if (gray32.empty())
        return {};
    double mn, mx;
    cv::minMaxLoc(gray32, &mn, &mx);
    cv::Mat out;
    if (mx > mn)
        gray32.convertTo(out, CV_8UC1, 255.0 / (mx - mn), -mn * 255.0 / (mx - mn));
    else
        out = cv::Mat(gray32.size(), CV_8UC1, cv::Scalar(0));
    return out;
}
