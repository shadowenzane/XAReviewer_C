#pragma once

#include <QDialog>

#include "../core/PacsFinder.h"

class QLineEdit;
class QSpinBox;
class QTableWidget;
class QPushButton;
class QProgressDialog;

// PACS C-FIND 查询对话框（对应 Python PacsQueryDialog）
// 服务器参数 + 查询条件 + 结果表
class PacsQueryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PacsQueryDialog(QWidget* parent = nullptr);

    // 选中行的 StudyInstanceUID（无选中返回空）
    QString selectedStudyUid() const { return selectedStudyUid_; }

private slots:
    void runQuery();

private:
    void buildUi();
    void fillResults(const PacsResultList& results);

    // 服务器参数
    QLineEdit* hostEdit_ = nullptr;
    QSpinBox* portSpin_ = nullptr;
    QLineEdit* remoteAetEdit_ = nullptr;
    QLineEdit* localAetEdit_ = nullptr;

    // 查询条件
    QLineEdit* patientIdEdit_ = nullptr;
    QLineEdit* patientNameEdit_ = nullptr;
    QLineEdit* studyDateEdit_ = nullptr;
    QLineEdit* modalityEdit_ = nullptr;

    QTableWidget* resultTable_ = nullptr;
    QPushButton* queryButton_ = nullptr;

    QString selectedStudyUid_;
};
