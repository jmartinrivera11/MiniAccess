#include "reportquickdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QMessageBox>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QPrinter>
#include <QPageSize>
#include <QPageLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFontMetricsF>
#include <algorithm>
#include <variant>
#include <string>
#include <cstdint>
#include "../core/table.h"
#include <QTextCursor>
#include <QTextCharFormat>
#include <QTextBlockFormat>
#include <QTextOption>

using namespace ma;

ReportQuickDialog::ReportQuickDialog(const QString& projectDir, QWidget* parent)
    : QDialog(parent), projectDir_(QDir(projectDir).absolutePath())
{
    setWindowTitle(tr("Simple Report (PDF)"));
    resize(840, 640);
    buildUi();
    loadTables();
    refreshRelationshipsCheckbox();
    applyCheckboxStyle();
}

ReportQuickDialog::~ReportQuickDialog() = default;

void ReportQuickDialog::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(10,10,10,10);
    root->setSpacing(8);

    auto* row1 = new QHBoxLayout();
    row1->setSpacing(10);
    row1->addWidget(new QLabel(tr("Title:"), this));
    edtTitle_ = new QLineEdit(this);
    edtTitle_->setPlaceholderText(tr("Project Report"));
    row1->addWidget(edtTitle_, 2);
    root->addLayout(row1);

    auto* lbl = new QLabel(tr("Select tables to include:"), this);
    { QFont lf = lbl->font(); lf.setPointSizeF(lf.pointSizeF() + 1.2); lbl->setFont(lf); }
    root->addWidget(lbl);

    scroll_ = new QScrollArea(this);
    scroll_->setWidgetResizable(true);
    panel_ = new QWidget(scroll_);
    panelLayout_ = new QVBoxLayout(panel_);
    panelLayout_->setContentsMargins(8,8,8,8);
    panelLayout_->setSpacing(6);
    panel_->setLayout(panelLayout_);
    scroll_->setWidget(panel_);
    root->addWidget(scroll_, 1);

    chkRelationships_ = new QCheckBox(tr("Include relationships"), this);
    { QFont f = chkRelationships_->font(); f.setPointSizeF(f.pointSizeF() + 1.0); chkRelationships_->setFont(f); }
    chkRelationships_->setChecked(true);
    root->addWidget(chkRelationships_);

    lblHint_ = new QLabel(tr("Tip: If none is checked, all tables will be exported."), this);
    lblHint_->setStyleSheet("color:#555;");
    root->addWidget(lblHint_);

    auto* btns = new QDialogButtonBox(this);
    btnExport_ = btns->addButton(tr("Export PDF…"), QDialogButtonBox::AcceptRole);
    btnClose_  = btns->addButton(tr("Close"),      QDialogButtonBox::RejectRole);
    root->addWidget(btns);

    connect(btnExport_, &QPushButton::clicked, this, &ReportQuickDialog::onExportPdf);
    connect(btnClose_,  &QPushButton::clicked, this, &QDialog::reject);
}

void ReportQuickDialog::applyCheckboxStyle() {
    const QString qss =
        "QCheckBox { color:#222; }"
        "QCheckBox::indicator { width:18px; height:18px; border-radius:3px; "
        "border:2px solid #444; background:#ffffff; margin-right:6px; }"
        "QCheckBox::indicator:hover { border-color:#1e88e5; }"
        "QCheckBox::indicator:checked { background:#1e88e5; border-color:#1e88e5; }";
    this->setStyleSheet(qss);
}

void ReportQuickDialog::loadTables() {
    qDeleteAll(panel_->findChildren<QCheckBox*>());
    tableChecks_.clear();

    if (projectDir_.isEmpty()) return;

    QDir d(projectDir_);
    const QStringList metas = d.entryList(QStringList() << "*.meta", QDir::Files, QDir::Name);
    for (const QString& m : metas) {
        const QString name = QFileInfo(m).completeBaseName();
        auto* chk = new QCheckBox(name, panel_);
        chk->setChecked(true);
        QFont f = chk->font(); f.setPointSizeF(f.pointSizeF() + 1.0); chk->setFont(f);
        panelLayout_->addWidget(chk);
        tableChecks_.insert(name, chk);
    }
    panelLayout_->addStretch(1);
}

void ReportQuickDialog::refreshRelationshipsCheckbox() {
    QVector<RelRow> all; QString why;
    readRelationsForTables(QSet<QString>{}, all, why);
    const int n = all.size();
    chkRelationships_->setText(tr("Include relationships (%1 found)").arg(n));
    chkRelationships_->setEnabled(n > 0);
    chkRelationships_->setChecked(n > 0);
}

QString ReportQuickDialog::baseForTable(const QString& projectDir, const QString& tableName) {
    return QDir(projectDir).filePath(tableName);
}

bool ReportQuickDialog::readSchema(const QString& tableName, QVector<ColInfo>& cols) {
    cols.clear();
    try {
        Table t; t.open(baseForTable(projectDir_, tableName).toStdString());
        const Schema s = t.getSchema();
        int idx = 0;
        for (const auto& f : s.fields) {
            ColInfo ci;
            ci.name  = QString::fromStdString(f.name);
            ci.index = idx++;
            cols.push_back(ci);
        }
        return true;
    } catch (...) {
        return false;
    }
}

static QString valueToQString(const ma::Value& v) {
    return std::visit([](auto&& arg)->QString {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>) {
            return arg ? QStringLiteral("true") : QStringLiteral("false");
        } else if constexpr (std::is_same_v<T, std::string>) {
            return QString::fromStdString(arg);
        } else if constexpr (std::is_same_v<T, int32_t>) {
            return QString::number(static_cast<qint64>(arg));
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return QString::number(static_cast<qlonglong>(arg));
        } else if constexpr (std::is_same_v<T, double>) {
            return QString::number(arg, 'g', 15);
        } else {
            return QString();
        }
    }, v);
}

bool ReportQuickDialog::readAllRows(const QString& tableName,
                                    const QVector<ColInfo>& cols,
                                    QVector<QStringList>& outRows,
                                    qsizetype& totalRows,
                                    QString& whyNot)
{
    outRows.clear();
    totalRows = 0;
    try {
        Table t; t.open(baseForTable(projectDir_, tableName).toStdString());
        const auto rids = t.scanAll();
        totalRows = static_cast<qsizetype>(rids.size());
        outRows.reserve(static_cast<int>(rids.size()));

        for (const auto& rid : rids) {
            auto recOpt = t.read(rid);
            if (!recOpt) { outRows.push_back(QStringList(cols.size(), QString())); continue; }

            const Record& rec = *recOpt;
            QStringList row; row.reserve(cols.size());
            for (int i = 0; i < cols.size(); ++i) {
                int cidx = cols[i].index;
                QString s;
                if (cidx >= 0 && cidx < static_cast<int>(rec.values.size())) {
                    const auto& cell = rec.values[cidx];
                    if (cell.has_value()) s = valueToQString(*cell);
                    else                  s = QString();
                }
                row << s;
            }
            outRows.push_back(row);
        }
        return true;
    } catch (const std::exception& ex) {
        whyNot = QString::fromUtf8(ex.what());
        return false;
    } catch (...) {
        whyNot = tr("Unknown error reading table");
        return false;
    }
}

bool ReportQuickDialog::readRelationsForTables(const QSet<QString>& include,
                                               QVector<RelRow>& out,
                                               QString& whyNot) const
{
    out.clear();
    const QString pathV2 = QDir(projectDir_).filePath("relationships_v2.json");
    const QString pathV1 = QDir(projectDir_).filePath("relations.json");
    QString path;
    if (QFileInfo::exists(pathV2)) path = pathV2;
    else if (QFileInfo::exists(pathV1)) path = pathV1;
    else return true;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        whyNot = tr("Cannot open relations file: %1").arg(path);
        return false;
    }
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return true;

    const auto root = doc.object();
    const QJsonArray rels = root.value("relations").toArray();
    for (const auto& v : rels) {
        const auto o = v.toObject();
        RelRow r;
        r.leftTable  = o.value("leftTable").toString();
        r.leftField  = o.value("leftField").toString();
        r.rightTable = o.value("rightTable").toString();
        r.rightField = o.value("rightField").toString();
        r.relType    = o.value("relType").toString("1:N");
        r.enforceRI  = o.value("enforceRI").toBool(false);
        r.cascadeUpdate = o.value("cascadeUpdate").toBool(false);
        r.cascadeDelete = o.value("cascadeDelete").toBool(false);

        if (!include.isEmpty()) {
            if (!include.contains(r.leftTable) || !include.contains(r.rightTable)) continue;
        }
        out.push_back(r);
    }
    return true;
}

QString ReportQuickDialog::escapeHtml(const QString& s) {
    QString out = s;
    out.replace('&', "&amp;");
    out.replace('<', "&lt;");
    out.replace('>', "&gt;");
    out.replace('"', "&quot;");
    out.replace('\'', "&#39;");
    return out;
}

static QString kReportCss() {
    return QStringLiteral(
        "body{font-family:Arial,Helvetica,sans-serif;font-size:10pt;color:#111;line-height:1.25;}"
        "h1{font-size:20pt;margin:0 0 12pt 0;}"
        "h2{font-size:14pt;margin:18pt 0 10pt 0;}"
        ".meta{color:#555;font-size:18pt;margin-bottom:18pt;}"
        "table{border-collapse:collapse;width:100%;table-layout:fixed;font-size:10pt;}"
        "thead,tbody,tfoot,tr,th,td{font-size:10pt;}"
        "th,td{border:1.2pt solid #333;padding:12pt 14pt;vertical-align:top;"
        "white-space:normal;word-wrap:break-word;overflow-wrap:break-word;}"
        "th{background:#efefef;text-align:left;}"
        "tfoot td{border:none;padding-top:16pt;color:#333;text-align:right;}"
        ".pagebreak{page-break-before:always;}"
        ".pill{display:inline-block;padding:4pt 8pt;border-radius:10pt;background:#eef;border:1pt solid #99f;font-size:16pt;}"
        );
}

QString ReportQuickDialog::buildHtmlMulti(const QString& title,
                                          const QStringList& tablesInOrder,
                                          const QMap<QString, QVector<ColInfo>>& colsByTable,
                                          const QMap<QString, QVector<QStringList>>& rowsByTable,
                                          const QMap<QString, qsizetype>& totalRowsByTable,
                                          const QVector<RelRow>& rels) const
{
    const QString when = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm");
    QString html;
    html.reserve(8192);

    html += "<html><head><meta charset='utf-8'><style>";
    html += kReportCss();
    html += "</style></head><body>";

    const QString t = title.trimmed().isEmpty() ? tr("Project Report") : title.trimmed();
    html += "<h1>" + escapeHtml(t) + "</h1>";
    html += "<div class='meta'>" + tr("Generated on") + " " + escapeHtml(when) + "</div>";

    bool first = true;
    for (const auto& table : tablesInOrder) {
        const auto cols = colsByTable.value(table);
        const auto rows = rowsByTable.value(table);
        const auto total = totalRowsByTable.value(table, 0);

        if (!first) html += "<div class='pagebreak'></div>";
        first = false;

        html += "<h2>" + escapeHtml(table) + "</h2>";
        html += "<table><thead><tr>";
        for (const auto& c : cols) html += "<th>" + escapeHtml(c.name) + "</th>";
        html += "</tr></thead><tbody>";

        for (const auto& r : rows) {
            html += "<tr>";
            for (const QString& cell : r) html += "<td>" + escapeHtml(cell) + "</td>";
            html += "</tr>";
        }
        html += "</tbody></table>";
        html += "<table><tfoot><tr><td>"
                + tr("Rows: ") + QString::number(total)
                + "</td></tr></tfoot></table>";
    }

    if (!rels.isEmpty()) {
        html += "<div class='pagebreak'></div>";
        html += "<h2>" + tr("Relationships") + "</h2>";
        html += "<table><thead><tr>";
        html += "<th>" + tr("Left") + "</th>";
        html += "<th>" + tr("Type") + "</th>";
        html += "<th>" + tr("Right") + "</th>";
        html += "<th>" + tr("Rules") + "</th>";
        html += "</tr></thead><tbody>";

        for (const auto& r : rels) {
            const QString left  = escapeHtml(r.leftTable + "." + r.leftField);
            const QString right = escapeHtml(r.rightTable + "." + r.rightField);
            QString rules;
            if (r.enforceRI)     rules += "<span class='pill'>RI</span> ";
            if (r.cascadeUpdate) rules += "<span class='pill'>Cascade Update</span> ";
            if (r.cascadeDelete) rules += "<span class='pill'>Cascade Delete</span> ";

            html += "<tr>";
            html += "<td>" + left + "</td>";
            html += "<td>" + escapeHtml(r.relType) + "</td>";
            html += "<td>" + right + "</td>";
            html += "<td>" + rules + "</td>";
            html += "</tr>";
        }
        html += "</tbody></table>";
    }

    html += "</body></html>";
    return html;
}

bool ReportQuickDialog::exportHtmlPdf(const QString& outFile,
                                      const QString& title,
                                      const QStringList& tablesInOrder,
                                      const QMap<QString, QVector<ColInfo>>& colsByTable,
                                      const QMap<QString, QVector<QStringList>>& rowsByTable,
                                      const QMap<QString, qsizetype>& totalRowsByTable,
                                      const QVector<RelRow>& rels,
                                      QString& whyNot) const
{
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(outFile);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageOrientation(QPageLayout::Landscape);
    printer.setPageMargins(QMarginsF(12, 12, 18, 12), QPageLayout::Millimeter);
    if (!printer.isValid()) { whyNot = tr("PDF initialization failed."); return false; }

    QTextDocument doc;
    doc.setDefaultFont(QFont("Arial", 20));
    QTextOption opt = doc.defaultTextOption();
    opt.setWrapMode(QTextOption::WordWrap);
    doc.setDefaultTextOption(opt);

    const QString html = buildHtmlMulti(title, tablesInOrder, colsByTable, rowsByTable, totalRowsByTable, rels);
    doc.setHtml(html);

    doc.print(&printer);
    return true;
}

void ReportQuickDialog::onExportPdf() {
    QStringList tables;
    for (auto it = tableChecks_.cbegin(); it != tableChecks_.cend(); ++it) {
        if (it.value() && it.value()->isChecked())
            tables << it.key();
    }
    if (tables.isEmpty()) tables = tableChecks_.keys();
    if (tables.isEmpty()) { QMessageBox::information(this, tr("Report"), tr("No tables found.")); return; }

    QMap<QString, QVector<ColInfo>> colsByTable;
    QMap<QString, QVector<QStringList>> rowsByTable;
    QMap<QString, qsizetype> totalsByTable;
    for (const auto& table : tables) {
        QVector<ColInfo> cols; if (!readSchema(table, cols)) {
            QMessageBox::warning(this, tr("Report"),
                                 tr("Could not read schema for table '%1'.").arg(table));
            return;
        }
        QVector<QStringList> rows; qsizetype total=0; QString why;
        if (!readAllRows(table, cols, rows, total, why)) {
            QMessageBox::warning(this, tr("Report"),
                                 tr("Could not read data for '%1'.\n%2").arg(table, why));
            return;
        }
        colsByTable.insert(table, cols);
        rowsByTable.insert(table, rows);
        totalsByTable.insert(table, total);
    }

    QSet<QString> includeSet = QSet<QString>(tables.cbegin(), tables.cend());
    QVector<RelRow> rels; QString whyRel;
    if (chkRelationships_->isChecked()) {
        if (!readRelationsForTables(includeSet, rels, whyRel)) {
            QMessageBox::warning(this, tr("Report"),
                                 tr("Could not read relationships.\n%1").arg(whyRel));
            return;
        }
    }

    const QString t = edtTitle_->text().trimmed();
    const QString suggested = (t.isEmpty()? QStringLiteral("Project Report") : t) + ".pdf";
    const QString outFile = QFileDialog::getSaveFileName(
        this, tr("Export PDF"), QDir(projectDir_).filePath(suggested),
        tr("PDF files (*.pdf)")
        );
    if (outFile.isEmpty()) return;

    QString whyPdf;
    if (!exportHtmlPdf(outFile, t, tables, colsByTable, rowsByTable, totalsByTable, rels, whyPdf)) {
        QMessageBox::critical(this, tr("Report"), tr("Export failed:\n%1").arg(whyPdf));
        return;
    }
    QMessageBox::information(this, tr("Report"),
                             tr("PDF exported:\n%1").arg(outFile));
}
