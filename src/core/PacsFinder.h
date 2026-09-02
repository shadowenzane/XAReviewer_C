#pragma once

#include <QString>
#include <QStringList>

#include <vector>

// PACS C-FIND 查询参数与结果
struct PacsQuery {
    QString host = QStringLiteral("127.0.0.1");
    int port = 11112;
    QString remoteAet = QStringLiteral("AE");   // 远端 AE 标题
    QString localAet = QStringLiteral("XAREVIEWER"); // 本地 AE 标题
    QString patientId;
    QString patientName;
    QString studyDate;     // YYYYMMDD-YYYYMMDD
    QString modality;
    int timeoutSeconds = 30;
};

struct PacsStudyResult {
    QString patientId;
    QString patientName;
    QString studyDate;
    QString studyTime;
    QString studyDescription;
    QString modality;
    QString studyUid;
    int seriesCount = 0;
};

// PACS 查询结果模型（供表格展示）
using PacsResultList = std::vector<PacsStudyResult>;

// DICOM 网络层：C-FIND SCU（STUDY 级）
class PacsFinder
{
public:
    // 同步执行 C-FIND；错误信息写入 errorMessage（网络失败等）
    // 结果至少包含 StudyInstanceUID（用于后续 C-MOVE/C-GET）
    static PacsResultList findStudies(const PacsQuery& query, QString* errorMessage = nullptr);
};
