#ifndef CONSOLEWINDOW_H
#define CONSOLEWINDOW_H

#include <QtGui>
#include "fshost.h"
#include "sortfilterproxymodel.h"

namespace Ui {
    class ConsoleWindow;
}

class ConsoleWindow : public QMainWindow {
    Q_OBJECT
public:
    ConsoleWindow(QWidget *parent = 0);
    ~ConsoleWindow();

protected:
    void changeEvent(QEvent *e);

/*public slots:
    void clearConsoleContents();
    void saveLogToFile();
    void pastebinLog();
    void filterLogUUID(QString);
    void findText();*/

private slots:
    void setConditionalScroll();
    void conditionalScroll();
    /*void filterClear();
    void filterStringChanged();*/
    void loggerHandler(QSharedPointer<switch_log_node_t> node, switch_log_level_t level);
    void addNewConsoleItem(QStandardItem *item);
    void cmdSendClicked();
    void lineCmdChanged(QString);
    /*void reverseFilterChecked();*/
    void filterModelLogLevel(int);


private:
    Ui::ConsoleWindow *ui;
    ConsoleModel *sourceModel;
    QModelIndexList foundItems;
    SortFilterProxyModel *model;
    /*pastebinDialog *_pastebinDlg;
    FindDialog *_findDialog;*/
    bool findNext;
    bool autoScroll;
    QSignalMapper *_levelFilter;

    /*void readSettings();
    void writeSettings();*/
};

#endif // CONSOLEWINDOW_H
