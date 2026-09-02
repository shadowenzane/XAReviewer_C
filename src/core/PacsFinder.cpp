#include "PacsFinder.h"

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/dcmnet/scu.h>

#include <QDate>

namespace {

QString ofStrToQ(const OFString& s)
{
    return QString::fromUtf8(s.c_str());
}

void setQueryTag(DcmDataset* dataset, const DcmTagKey& key, const QString& value)
{
    if (!value.isEmpty())
        dataset->putAndInsertString(key, value.toUtf8().constData());
}

} // namespace

PacsResultList PacsFinder::findStudies(const PacsQuery& query, QString* errorMessage)
{
    PacsResultList results;
    if (errorMessage)
        errorMessage->clear();

    DcmSCU scu;
    scu.setAETitle(OFString(query.localAet.toUtf8().constData()));
    scu.setPeerAETitle(OFString(query.remoteAet.toUtf8().constData()));
    scu.setPeerHostName(OFString(query.host.toUtf8().constData()));
    scu.setPeerPort(static_cast<unsigned short>(query.port));
    scu.setDIMSETimeout(static_cast<int>(query.timeoutSeconds));
    scu.setACSETimeout(static_cast<int>(query.timeoutSeconds));

    // Presentation Context：Study Root Q/R + Little Endian 显式/隐式
    OFList<OFString> xferSyntaxes;
    xferSyntaxes.push_back(UID_LittleEndianExplicitTransferSyntax);
    xferSyntaxes.push_back(UID_LittleEndianImplicitTransferSyntax);
    scu.addPresentationContext(UID_FINDStudyRootQueryRetrieveInformationModel, xferSyntaxes);

    // 建立关联
    OFCondition cond = scu.initNetwork();
    if (cond.bad()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("网络初始化失败: %1")
                                 .arg(QString::fromUtf8(cond.text()));
        return results;
    }
    cond = scu.negotiateAssociation();
    if (cond.bad()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("PACS 连接失败（%1:%2，AET=%3）: %4")
                                 .arg(query.host)
                                 .arg(query.port)
                                 .arg(query.remoteAet)
                                 .arg(QString::fromUtf8(cond.text()));
        return results;
    }

    // C-FIND 请求（STUDY 级）
    DcmDataset request;
    setQueryTag(&request, DCM_PatientID, query.patientId);
    setQueryTag(&request, DCM_PatientName, query.patientName);
    setQueryTag(&request, DCM_StudyDate, query.studyDate);
    setQueryTag(&request, DCM_Modality, query.modality);
    // 返回标签
    request.putAndInsertString(DCM_QueryRetrieveLevel, "STUDY");
    request.putAndInsertString(DCM_StudyInstanceUID, "");
    request.putAndInsertString(DCM_StudyDescription, "");
    request.putAndInsertString(DCM_NumberOfStudyRelatedSeries, "");
    request.putAndInsertString(DCM_StudyTime, "");

    OFList<QRResponse*> responses;
    cond = scu.sendFINDRequest(0, &request, &responses);

    if (cond.bad()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("C-FIND 查询失败: %1")
                                 .arg(QString::fromUtf8(cond.text()));
        for (auto* r : responses)
            delete r;
        scu.releaseAssociation();
        return results;
    }

    for (const auto* resp : responses) {
        if (resp == nullptr || resp->m_dataset == nullptr)
            continue;
        DcmDataset* ds = resp->m_dataset;

        PacsStudyResult r;
        OFString v;
        if (ds->findAndGetOFString(DCM_PatientID, v).good())
            r.patientId = ofStrToQ(v);
        if (ds->findAndGetOFString(DCM_PatientName, v).good())
            r.patientName = ofStrToQ(v);
        if (ds->findAndGetOFString(DCM_StudyDate, v).good())
            r.studyDate = ofStrToQ(v);
        if (ds->findAndGetOFString(DCM_StudyTime, v).good())
            r.studyTime = ofStrToQ(v);
        if (ds->findAndGetOFString(DCM_StudyDescription, v).good())
            r.studyDescription = ofStrToQ(v);
        if (ds->findAndGetOFString(DCM_Modality, v).good())
            r.modality = ofStrToQ(v);
        if (ds->findAndGetOFString(DCM_StudyInstanceUID, v).good())
            r.studyUid = ofStrToQ(v);
        if (!r.studyUid.isEmpty())
            results.push_back(r);
    }

    for (auto* r : responses)
        delete r;
    scu.releaseAssociation();

    return results;
}
