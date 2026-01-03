#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>
#include <QCoreApplication>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardItem>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    networkManager = new QNetworkAccessManager(this);
    // Ensure the credit label appears at the right side of the status bar
    if (ui->creditLabel && ui->statusbar) {
        ui->statusbar->addPermanentWidget(ui->creditLabel);
        ui->creditLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }

    historyModel = new QStandardItemModel(this);
    commonModel = new QStandardItemModel(this);
    examplesModel = new QStandardItemModel(this);

    ui->historyListView->setModel(historyModel);
    ui->commonListView->setModel(commonModel);
    ui->examplesListView->setModel(examplesModel);

    connect(ui->translateButton, &QPushButton::clicked, this, &MainWindow::on_translateButton_clicked);
    connect(networkManager, &QNetworkAccessManager::finished, this, &MainWindow::onNetworkFinished);
    connect(ui->historyListView, &QListView::clicked, this, &MainWindow::onHistoryClicked);
    if (ui->deleteHistoryButton) connect(ui->deleteHistoryButton, &QPushButton::clicked, this, &MainWindow::on_deleteHistoryButton_clicked);
    if (ui->clearHistoryButton) connect(ui->clearHistoryButton, &QPushButton::clicked, this, &MainWindow::on_clearHistoryButton_clicked);
    if (ui->addCommonButton) connect(ui->addCommonButton, &QPushButton::clicked, this, &MainWindow::on_addCommonButton_clicked);
    if (ui->removeCommonButton) connect(ui->removeCommonButton, &QPushButton::clicked, this, &MainWindow::on_removeCommonButton_clicked);
    if (ui->commonListView) connect(ui->commonListView, &QListView::clicked, this, &MainWindow::on_commonClicked);

    initDatabase();
    loadHistory();
    loadCommon();
}

MainWindow::~MainWindow()
{
    delete ui;
    if (db.isOpen()) db.close();
}

void MainWindow::initDatabase()
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDir.isEmpty()) {
        dataDir = QCoreApplication::applicationDirPath();
    }
    QDir dir;
    dir.mkpath(dataDir);
    QString dbPath = dataDir + QDir::separator() + "translations.db";

    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        qWarning() << "Failed to open database:" << db.lastError().text();
        return;
    }

    QSqlQuery q;
    q.exec("CREATE TABLE IF NOT EXISTS history (id INTEGER PRIMARY KEY AUTOINCREMENT, query TEXT, result TEXT, timestamp INTEGER)");
    q.exec("CREATE TABLE IF NOT EXISTS common (id INTEGER PRIMARY KEY AUTOINCREMENT, word TEXT UNIQUE, freq INTEGER DEFAULT 1)");
}

void MainWindow::loadHistory()
{
    historyModel->clear();
    QSqlQuery q;
    if (!q.exec("SELECT id, query, result, timestamp FROM history ORDER BY timestamp DESC LIMIT 200")) {
        qWarning() << "Failed to load history:" << q.lastError().text();
        return;
    }
    while (q.next()) {
        QString queryText = q.value("query").toString();
        QString resultText = q.value("result").toString();
        qint64 ts = q.value("timestamp").toLongLong();
        qint64 id = q.value("id").toLongLong();
        QString display = QString("%1 — %2").arg(queryText, resultText);
        QStandardItem *item = new QStandardItem(display);
        item->setData(queryText, Qt::UserRole + 1);
        item->setData(resultText, Qt::UserRole + 2);
        item->setData(ts, Qt::UserRole + 3);
        item->setData(id, Qt::UserRole + 4);
        historyModel->appendRow(item);
    }
}

void MainWindow::loadCommon()
{
    commonModel->clear();
    QSqlQuery q;
    if (!q.exec("SELECT id, word, freq FROM common ORDER BY freq DESC, word ASC")) {
        qWarning() << "Failed to load common words:" << q.lastError().text();
        return;
    }
    while (q.next()) {
        qint64 id = q.value("id").toLongLong();
        QString word = q.value("word").toString();
        int freq = q.value("freq").toInt();
        QString display = QString("%1 (%2)").arg(word).arg(freq);
        QStandardItem *item = new QStandardItem(display);
        item->setData(word, Qt::UserRole + 1);
        item->setData(freq, Qt::UserRole + 2);
        item->setData(id, Qt::UserRole + 4);
        commonModel->appendRow(item);
    }
}

void MainWindow::addCommon(const QString &word)
{
    if (!db.isOpen() || word.trimmed().isEmpty()) return;
    QSqlQuery q;
    // try update freq
    q.prepare("UPDATE common SET freq = freq + 1 WHERE word = :w");
    q.bindValue(":w", word);
    if (!q.exec()) {
        qWarning() << "Failed to update common freq:" << q.lastError().text();
    }
    if (q.numRowsAffected() == 0) {
        QSqlQuery ins;
        ins.prepare("INSERT OR IGNORE INTO common (word, freq) VALUES (:w, 1)");
        ins.bindValue(":w", word);
        if (!ins.exec()) {
            qWarning() << "Failed to insert common word:" << ins.lastError().text();
            return;
        }
    }
    loadCommon();
}

void MainWindow::removeCommonById(qint64 id)
{
    if (!db.isOpen() || id == 0) return;
    QSqlQuery q;
    q.prepare("DELETE FROM common WHERE id = :id");
    q.bindValue(":id", id);
    if (!q.exec()) {
        qWarning() << "Failed to remove common id" << id << ":" << q.lastError().text();
    }
}

void MainWindow::removeCommonByWord(const QString &word)
{
    if (!db.isOpen() || word.trimmed().isEmpty()) return;
    QSqlQuery q;
    q.prepare("DELETE FROM common WHERE word = :w");
    q.bindValue(":w", word);
    if (!q.exec()) {
        qWarning() << "Failed to remove common word" << word << ":" << q.lastError().text();
    }
}

void MainWindow::on_addCommonButton_clicked()
{
    QString text = ui->inputLineEdit->text().trimmed();
    if (text.isEmpty()) return;
    addCommon(text);
    statusBar()->showMessage("已添加到常用", 2000);
}

void MainWindow::on_removeCommonButton_clicked()
{
    // Use selectedRows to avoid duplicate deletions
    QModelIndexList rowsList = ui->commonListView->selectionModel()->selectedRows();
    if (rowsList.isEmpty()) return;
    // collect unique rows and remove by word to be more robust
    QSet<int> rowsSet;
    QStringList wordsToRemove;
    for (const QModelIndex &idx : rowsList) {
        int row = idx.row();
        if (!rowsSet.contains(row)) {
            rowsSet.insert(row);
            QStandardItem *item = commonModel->item(row);
            if (item) {
                QString word = item->data(Qt::UserRole + 1).toString();
                if (!word.isEmpty()) wordsToRemove.append(word);
            }
        }
    }
    // remove from DB by word, then reload model
    for (const QString &w : wordsToRemove) {
        removeCommonByWord(w);
    }
    loadCommon();
    ui->commonListView->selectionModel()->clearSelection();
}

void MainWindow::on_commonClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;
    QString word = index.data(Qt::UserRole + 1).toString();
    ui->inputLineEdit->setText(word);
    // increment freq and translate
    addCommon(word);
    on_translateButton_clicked();
}

void MainWindow::saveHistory(const QString &query, const QString &result)
{
    if (!db.isOpen()) return;
    QSqlQuery q;
    q.prepare("INSERT INTO history (query, result, timestamp) VALUES (:q, :r, :t)");
    q.bindValue(":q", query);
    q.bindValue(":r", result);
    q.bindValue(":t", QDateTime::currentSecsSinceEpoch());
    if (!q.exec()) {
        qWarning() << "Failed to save history:" << q.lastError().text();
        return;
    }
    QVariant lastId = q.lastInsertId();
    qint64 id = lastId.isValid() ? lastId.toLongLong() : 0;
    // prepend to model
    QString display = QString("%1 — %2").arg(query, result);
    QStandardItem *item = new QStandardItem(display);
    item->setData(query, Qt::UserRole + 1);
    item->setData(result, Qt::UserRole + 2);
    item->setData(QDateTime::currentSecsSinceEpoch(), Qt::UserRole + 3);
    item->setData(id, Qt::UserRole + 4);
    historyModel->insertRow(0, item);
}

void MainWindow::deleteHistoryById(qint64 id)
{
    if (!db.isOpen() || id == 0) return;
    QSqlQuery q;
    q.prepare("DELETE FROM history WHERE id = :id");
    q.bindValue(":id", id);
    if (!q.exec()) {
        qWarning() << "Failed to delete history id" << id << ":" << q.lastError().text();
    }
}

void MainWindow::on_deleteHistoryButton_clicked()
{
    // Use selectedRows to avoid duplicate indexes and only delete one row per selection
    QModelIndexList rowsList = ui->historyListView->selectionModel()->selectedRows();
    if (rowsList.isEmpty()) return;
    // collect unique row numbers using a set to be safe
    QSet<int> rowsSet;
    for (const QModelIndex &idx : rowsList) {
        rowsSet.insert(idx.row());
    }
    QVector<int> rows = QVector<int>::fromList(rowsSet.values());
    // sort rows descending to remove safely
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    // debug output to help trace duplicate deletions
    qDebug() << "Deleting history rows:" << rows;
    for (int row : rows) {
        QStandardItem *item = historyModel->item(row);
        qint64 id = item ? item->data(Qt::UserRole + 4).toLongLong() : 0;
        qDebug() << " -> row" << row << "id" << id;
        if (id != 0) deleteHistoryById(id);
        historyModel->removeRow(row);
    }
    // clear selection to avoid stale indexes
    ui->historyListView->selectionModel()->clearSelection();
}

void MainWindow::on_clearHistoryButton_clicked()
{
    if (!db.isOpen()) return;
    QSqlQuery q;
    if (!q.exec("DELETE FROM history")) {
        qWarning() << "Failed to clear history:" << q.lastError().text();
        return;
    }
    historyModel->clear();
}

void MainWindow::on_translateButton_clicked()
{
    QString text = ui->inputLineEdit->text().trimmed();
    if (text.isEmpty()) return;

    // check cache first
    if (db.isOpen()) {
        QSqlQuery q;
        q.prepare("SELECT result FROM history WHERE query = :q ORDER BY timestamp DESC LIMIT 1");
        q.bindValue(":q", text);
        if (q.exec() && q.next()) {
            QString cached = q.value(0).toString();
            ui->resultTextEdit->setPlainText(cached);
            statusBar()->showMessage("Loaded from cache");
            return;
        }
    }

    // detect language: if contains CJK characters treat as zh-CN -> en, else en -> zh-CN
    bool hasCJK = false;
    for (QChar ch : text) {
        ushort u = ch.unicode();
        if ((u >= 0x4E00 && u <= 0x9FFF) || (u >= 0x3400 && u <= 0x4DBF)) {
            hasCJK = true;
            break;
        }
    }
    QString targetLang = hasCJK ? "en" : "zh-CN";

    // Use unofficial Google Translate endpoint for better quality
    QUrl url("https://translate.googleapis.com/translate_a/single");
    QUrlQuery query;
    query.addQueryItem("client", "gtx");
    query.addQueryItem("sl", "auto");
    query.addQueryItem("tl", targetLang);
    query.addQueryItem("dt", "t");
    query.addQueryItem("q", text);
    url.setQuery(query);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "wtq-translator/1.0");
    networkManager->get(req);
    statusBar()->showMessage("翻译中...");
    // If input looks like a single English word, also fetch example sentences
    QRegularExpression wordRe("^[A-Za-z'-]+$");
    if (!hasCJK && wordRe.match(text).hasMatch()) {
        fetchExamples(text);
    } else {
        examplesModel->clear();
    }
}

void MainWindow::onNetworkFinished(QNetworkReply *reply)
{
    if (!reply) return;
    QByteArray body = reply->readAll();
    reply->deleteLater();
    QUrl replyUrl = reply->url();
    QString host = replyUrl.host();
    // If this reply is from dictionaryapi.dev, parse examples and populate examplesModel
    if (host.contains("dictionaryapi.dev")) {
        QJsonParseError parseErr;
        QJsonDocument exDoc = QJsonDocument::fromJson(body, &parseErr);
        examplesModel->clear();
        if (parseErr.error != QJsonParseError::NoError) {
            statusBar()->clearMessage();
            return;
        }
        if (exDoc.isArray()) {
            QStringList found;
            QJsonArray arr = exDoc.array();
            for (const QJsonValue &entryVal : arr) {
                if (!entryVal.isObject()) continue;
                QJsonObject entryObj = entryVal.toObject();
                if (!entryObj.contains("meanings")) continue;
                QJsonArray meanings = entryObj.value("meanings").toArray();
                for (const QJsonValue &mVal : meanings) {
                    QJsonObject meaning = mVal.toObject();
                    if (!meaning.contains("definitions")) continue;
                    QJsonArray defs = meaning.value("definitions").toArray();
                    for (const QJsonValue &dVal : defs) {
                        QJsonObject def = dVal.toObject();
                        if (def.contains("example")) {
                            QString ex = def.value("example").toString().trimmed();
                            if (!ex.isEmpty() && !found.contains(ex)) found.append(ex);
                        }
                        if (def.contains("examples") && def.value("examples").isArray()) {
                            QJsonArray exs = def.value("examples").toArray();
                            for (const QJsonValue &ev : exs) {
                                QString ex = ev.toString().trimmed();
                                if (!ex.isEmpty() && !found.contains(ex)) found.append(ex);
                            }
                        }
                        if (found.size() >= 5) break;
                    }
                    if (found.size() >= 5) break;
                }
                if (found.size() >= 5) break;
            }
            // limit to 5 and prepare placeholders, then request translations for each example
            examplesPendingList = found.mid(0, 5);
            examplesModel->clear();
            for (int i = 0; i < examplesPendingList.size(); ++i) {
                QString original = examplesPendingList.at(i);
                QString placeholder = QString("%1. %2\n翻译: 正在翻译...").arg(i+1).arg(original);
                QStandardItem *it = new QStandardItem(placeholder);
                examplesModel->appendRow(it);
                // send translate request for the example
                QUrl turl("https://translate.googleapis.com/translate_a/single");
                QUrlQuery tq;
                tq.addQueryItem("client", "gtx");
                tq.addQueryItem("sl", "auto");
                tq.addQueryItem("tl", "zh-CN");
                tq.addQueryItem("dt", "t");
                tq.addQueryItem("q", original);
                turl.setQuery(tq);
                QNetworkRequest treq(turl);
                QVariantMap attr;
                attr.insert("type", "examples_translate");
                attr.insert("idx", i);
                treq.setAttribute(QNetworkRequest::User, attr);
                treq.setHeader(QNetworkRequest::UserAgentHeader, "wtq-translator/1.0");
                networkManager->get(treq);
            }
        }
        statusBar()->clearMessage();
        return;
    }
    // Check if this reply has a user attribute indicating it's an examples translation
    QVariant userAttr = reply->request().attribute(QNetworkRequest::User);
    if (userAttr.isValid() && userAttr.type() == QVariant::Map) {
        QVariantMap m = userAttr.toMap();
        if (m.value("type").toString() == "examples_translate") {
            int idx = m.value("idx").toInt();
            // parse google translate array response
            QJsonParseError tErr;
            QJsonDocument tDoc = QJsonDocument::fromJson(body, &tErr);
            QString translatedEx;
            if (tErr.error == QJsonParseError::NoError && tDoc.isArray()) {
                QJsonArray rootArray = tDoc.array();
                if (!rootArray.isEmpty() && rootArray[0].isArray()) {
                    QJsonArray segments = rootArray[0].toArray();
                    QStringList parts;
                    for (const QJsonValue &segVal : segments) {
                        if (segVal.isArray()) {
                            QJsonArray segArr = segVal.toArray();
                            if (!segArr.isEmpty()) parts << segArr[0].toString();
                        }
                    }
                    translatedEx = parts.join("");
                }
            }
            QString original = examplesPendingList.value(idx);
            QString display = QString("%1. %2\n翻译: %3").arg(idx+1).arg(original).arg(translatedEx.isEmpty() ? QStringLiteral("未找到翻译") : translatedEx);
            QStandardItem *it = examplesModel->item(idx);
            if (it) it->setText(display);
            statusBar()->clearMessage();
            return;
        }
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error != QJsonParseError::NoError) {
        ui->resultTextEdit->setPlainText(QString("解析响应失败：%1").arg(err.errorString()));
        statusBar()->clearMessage();
        return;
    }

    // Response is an array; first element contains translation segments
    QString translated;
    QJsonArray rootArray = doc.array();
    if (!rootArray.isEmpty() && rootArray[0].isArray()) {
        QJsonArray segments = rootArray[0].toArray();
        QStringList parts;
        for (const QJsonValue &segVal : segments) {
            if (segVal.isArray()) {
                QJsonArray segArr = segVal.toArray();
                if (!segArr.isEmpty()) {
                    parts << segArr[0].toString();
                }
            }
        }
        translated = parts.join("");
    }

    if (translated.isEmpty()) {
        ui->resultTextEdit->setPlainText("未找到翻译结果。");
    } else {
        ui->resultTextEdit->setPlainText(translated);
        // save to history (avoid duplicate identical recent entry)
        QString queryText = ui->inputLineEdit->text().trimmed();
        // check last entry
        bool shouldSave = true;
        if (historyModel->rowCount() > 0) {
            QStandardItem *first = historyModel->item(0);
            if (first) {
                QString lastQuery = first->data(Qt::UserRole + 1).toString();
                if (lastQuery == queryText) shouldSave = false;
            }
        }
        if (shouldSave) saveHistory(queryText, translated);
    }
    statusBar()->clearMessage();
}

void MainWindow::onHistoryClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;
    QString queryText = index.data(Qt::UserRole + 1).toString();
    QString resultText = index.data(Qt::UserRole + 2).toString();
    ui->inputLineEdit->setText(queryText);
    ui->resultTextEdit->setPlainText(resultText);
}

void MainWindow::fetchExamples(const QString &word)
{
    if (word.trimmed().isEmpty()) {
        examplesModel->clear();
        return;
    }
    // Use dictionaryapi.dev for English example sentences
    QString encoded = QString::fromUtf8(QUrl::toPercentEncoding(word));
    QUrl url(QString("https://api.dictionaryapi.dev/api/v2/entries/en/%1").arg(encoded));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, "wtq-translator/1.0");
    networkManager->get(req);
    statusBar()->showMessage("加载例句...");
}
