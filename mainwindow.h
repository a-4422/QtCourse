#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QSqlDatabase>
#include <QStandardItemModel>
#include <QNetworkReply>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QNetworkAccessManager *networkManager;
    QSqlDatabase db;
    QStandardItemModel *historyModel;
    QStandardItemModel *commonModel;
    QStandardItemModel *examplesModel;
    QStringList examplesPendingList;

    void initDatabase();
    void loadHistory();
    void saveHistory(const QString &query, const QString &result);
    void deleteHistoryById(qint64 id);
    void fetchExamples(const QString &word);
    void loadCommon();
    void addCommon(const QString &word);
    void removeCommonById(qint64 id);
    void removeCommonByWord(const QString &word);

private slots:
    void on_translateButton_clicked();
    void onNetworkFinished(QNetworkReply *reply);
    void onHistoryClicked(const QModelIndex &index);
    void on_deleteHistoryButton_clicked();
    void on_clearHistoryButton_clicked();
    void on_addCommonButton_clicked();
    void on_removeCommonButton_clicked();
    void on_commonClicked(const QModelIndex &index);
};
#endif // MAINWINDOW_H
