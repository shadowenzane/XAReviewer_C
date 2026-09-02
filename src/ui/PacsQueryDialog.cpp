#include "PacsQueryDialog.h"
#include "Theme.h"

#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QThread>
#include <QVBoxLayout>

// 后台查询线程（对应 Python 的 PACS 后台查询）
namespace {

class FindWorker : public QObject
{
    Q_OBJECT

public:
    explicit FindWorker(PacsQuery q, QObject* parent = nullptr)
        : QObject(parent), query_(std::move(q))
    {
    }

    void run()
    {
        QString err;
        const PacsResultList r = PacsFinder::findStudies(query_, &err);
        emit done(r, err);
    }

signals:
    void done(const PacsResultList& results, const QString& error);

private:
    PacsQuery query_;
};

} // namespace

PacsQueryDialog::PacsQueryDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("PACS 查询 (C-FIND)"));
    resize(860, 560);
    buildUi();
}

void PacsQueryDialog::buildUi()
{
    auto* layout = new QVBoxLayout(this);

    // 服务器参数
    auto* serverGroup = new QGroupBox(tr("PACS 服务器"), this);
    auto* serverForm = new QFormLayout(serverGroup);
    hostEdit_ = new QLineEdit(QStringLiteral("127.0.0.1"), serverGroup);
    portSpin_ = new QSpinBox(serverGroup);
    portSpin_->setRange(1, 65535);
    portSpin_->setValue(11112);
    remoteAetEdit_ = new QLineEdit(QStringLiteral("AE"), serverGroup);
    localAetEdit_ = new QLineEdit(QStringLiteral("XAREVIEWER"), serverGroup);

    auto* hostRow = new QHBoxLayout();
    hostRow->addWidget(hostEdit_, 2);
    hostRow->addWidget(new QLabel(tr("端口:"), serverGroup));
    hostRow->addWidget(portSpin_, 1);
    serverForm->addRow(tr("主机:"), hostRow);
    serverForm->addRow(tr("远端 AET:"), remoteAetEdit_);
    serverForm->addRow(tr("本地 AET:"), localAetEdit_);
    layout->addWidget(serverGroup);

    // 查询条件
    auto* condGroup = new QGroupBox(tr("查询条件（留空表示不限）"), this);
    auto* condForm = new QFormLayout(condGroup);
    patientIdEdit_ = new QLineEdit(condGroup);
    patientNameEdit_ = new QLineEdit(condGroup);
    studyDateEdit_ = new QLineEdit(condGroup);
    studyDateEdit_->setPlaceholderText(tr("YYYYMMDD-YYYYMMDD"));
    modalityEdit_ = new QLineEdit(condGroup);
    modalityEdit_->setPlaceholderText(tr("如 DSA / CT / US"));
    condForm->addRow(tr("患者 ID:"), patientIdEdit_);
    condForm->addRow(tr("患者姓名:"), patientNameEdit_);
    condForm->addRow(tr("检查日期:"), studyDateEdit_);
    condForm->addRow(tr("模态:"), modalityEdit_);
    layout->addWidget(condGroup);

    // 结果表
    resultTable_ = new QTableWidget(0, 6, this);
    resultTable_->setHorizontalHeaderLabels({tr("患者 ID"), tr("姓名"), tr("检查日期"),
                                             tr("描述"), tr("模态"), tr("序列数")});
    resultTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    resultTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    resultTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resultTable_->horizontalHeader()->setStretchLastSection(true);
    resultTable_->verticalHeader()->setVisible(false);
    layout->addWidget(resultTable_, 1);

    // 按钮
    auto* btnRow = new QHBoxLayout();
    queryButton_ = new QPushButton(tr("查询"), this);
    queryButton_->setDefault(true);
    auto* bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    btnRow->addWidget(queryButton_, 1);
    btnRow->addWidget(bbox);
    layout->addLayout(btnRow);

    connect(queryButton_, &QPushButton::clicked, this, &PacsQueryDialog::runQuery);
    connect(bbox, &QDialogButtonBox::accepted, this, [this] {
        const int row = resultTable_->currentRow();
        if (row >= 0)
            selectedStudyUid_ = resultTable_->item(row, 0)->data(Qt::UserRole).toString();
        accept();
    });
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(resultTable_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        selectedStudyUid_ = resultTable_->item(row, 0)->data(Qt::UserRole).toString();
        accept();
    });
}

void PacsQueryDialog::runQuery()
{
    PacsQuery q;
    q.host = hostEdit_->text().trimmed();
    q.port = portSpin_->value();
    q.remoteAet = remoteAetEdit_->text().trimmed();
    q.localAet = localAetEdit_->text().trimmed();
    q.patientId = patientIdEdit_->text().trimmed();
    q.patientName = patientNameEdit_->text().trimmed();
    q.studyDate = studyDateEdit_->text().trimmed();
    q.modality = modalityEdit_->text().trimmed();

    auto* progress = new QProgressDialog(tr("正在查询 PACS…"), tr("取消"), 0, 0, this);
    progress->setWindowModality(Qt::WindowModal);
    progress->show();

    auto* thread = new QThread(this);
    auto* worker = new FindWorker(q);
    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &FindWorker::run);
    connect(worker, &FindWorker::done, this,
            [this, progress, thread](const PacsResultList& results, const QString& error) {
                progress->close();
                progress->deleteLater();
                thread->quit();
                if (!error.isEmpty()) {
                    QMessageBox::warning(this, tr("查询失败"), error);
                } else {
                    fillResults(results);
                    if (results.empty())
                        QMessageBox::information(this, tr("查询完成"), tr("未找到匹配结果"));
                }
            });
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    connect(progress, &QProgressDialog::canceled, this, [thread] { thread->quit(); });

    thread->start();
}

void PacsQueryDialog::fillResults(const PacsResultList& results)
{
    resultTable_->setRowCount(static_cast<int>(results.size()));
    int row = 0;
    for (const auto& r : results) {
        auto mkItem = [this](const QString& text) {
            auto* it = new QTableWidgetItem(text);
            it->setFlags(it->flags() & ~Qt::ItemIsEditable);
            return it;
        };
        auto* idItem = mkItem(r.patientId);
        idItem->setData(Qt::UserRole, r.studyUid);
        resultTable_->setItem(row, 0, idItem);
        resultTable_->setItem(row, 1, mkItem(r.patientName));
        resultTable_->setItem(row, 2, mkItem(r.studyDate));
        resultTable_->setItem(row, 3, mkItem(r.studyDescription));
        resultTable_->setItem(row, 4, mkItem(r.modality));
        resultTable_->setItem(row, 5, mkItem(QString::number(r.seriesCount)));
        ++row;
    }
    if (resultTable_->rowCount() > 0)
        resultTable_->selectRow(0);
}

#include "PacsQueryDialog.moc"
