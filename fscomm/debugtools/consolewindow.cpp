#include "consolewindow.h"
#include "ui_consolewindow.h"

ConsoleWindow::ConsoleWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ConsoleWindow),
    findNext(false)
{
    ui->setupUi(this);
    sourceModel = new ConsoleModel(this);
    model = new SortFilterProxyModel(this);
    model->setSourceModel(sourceModel);
    model->setFilterKeyColumn(0);
    ui->consoleListView->setModel(model);
    ui->consoleListView->setColumnWidth(0, 2000);

    connect(sourceModel, SIGNAL(beforeInserting()),
                     this, SLOT(setConditionalScroll()));
    connect(sourceModel, SIGNAL(afterInserting()),
                     this, SLOT(conditionalScroll()));

    connect(ui->btnSend, SIGNAL(clicked()),
                     this, SLOT(cmdSendClicked()));
    connect(ui->lineCmd, SIGNAL(textChanged(QString)),
                     this, SLOT(lineCmdChanged(QString)));

    _levelFilter = new QSignalMapper(this);
    connect(ui->checkEmerg, SIGNAL(clicked()), _levelFilter, SLOT(map()));
    connect(ui->checkAlert, SIGNAL(clicked()), _levelFilter, SLOT(map()));
    connect(ui->checkCrit, SIGNAL(clicked()), _levelFilter, SLOT(map()));
    connect(ui->checkDebug, SIGNAL(clicked()), _levelFilter, SLOT(map()));
    connect(ui->checkError, SIGNAL(clicked()), _levelFilter, SLOT(map()));
    connect(ui->checkInfo, SIGNAL(clicked()), _levelFilter, SLOT(map()));
    connect(ui->checkNotice, SIGNAL(clicked()), _levelFilter, SLOT(map()));
    connect(ui->checkWarn, SIGNAL(clicked()), _levelFilter, SLOT(map()));
    _levelFilter->setMapping(ui->checkEmerg, SWITCH_LOG_CONSOLE);
    _levelFilter->setMapping(ui->checkAlert, SWITCH_LOG_ALERT);
    _levelFilter->setMapping(ui->checkCrit, SWITCH_LOG_CRIT);
    _levelFilter->setMapping(ui->checkDebug, SWITCH_LOG_DEBUG);
    _levelFilter->setMapping(ui->checkError, SWITCH_LOG_ERROR);
    _levelFilter->setMapping(ui->checkInfo, SWITCH_LOG_INFO);
    _levelFilter->setMapping(ui->checkNotice, SWITCH_LOG_NOTICE);
    _levelFilter->setMapping(ui->checkWarn, SWITCH_LOG_WARNING);
    connect(_levelFilter, SIGNAL(mapped(int)), this, SLOT(filterModelLogLevel(int)));

    connect(ui->btnFilterClear, SIGNAL(clicked()),
                     this, SLOT(filterClear()));
    connect(ui->lineFilter, SIGNAL(textChanged(QString)),
                     this, SLOT(filterStringChanged()));
    connect(ui->filterCaseSensitivityCheckBox, SIGNAL(toggled(bool)),
                     this, SLOT(filterStringChanged()));
    connect(ui->filterSyntaxComboBox, SIGNAL(currentIndexChanged(int)),
                     this, SLOT(filterStringChanged()));
    connect(ui->filterReverseCheckBox, SIGNAL(toggled(bool)),
                     this, SLOT(reverseFilterChecked()));

    connect(g_FSHost, SIGNAL(eventLog(QSharedPointer<switch_log_node_t>,switch_log_level_t)), this, SLOT(loggerHandler(QSharedPointer<switch_log_node_t>,switch_log_level_t)));

}

ConsoleWindow::~ConsoleWindow()
{
    delete ui;
}

void ConsoleWindow::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void ConsoleWindow::setConditionalScroll()
{
    autoScroll = (ui->consoleListView->verticalScrollBar()->maximum() == ui->consoleListView->verticalScrollBar()->value());
}

void ConsoleWindow::conditionalScroll()
{
    if (autoScroll)
        ui->consoleListView->scrollToBottom();
}

void ConsoleWindow::cmdSendClicked()
{
    if (ui->lineCmd->text().isEmpty()) return;

    QString cmd = ui->lineCmd->text().split(" ", QString::SkipEmptyParts)[0];
    if (cmd.isEmpty()) return;

    QStringList split = ui->lineCmd->text().split(" ", QString::SkipEmptyParts);
    if (split.isEmpty()) return;
    QString args;
    for (int i=1; i<split.length(); i++)
    {
        args += split[i];
        if (i!=split.length()-1)
            args += " ";
    }

    QString res;
    g_FSHost->sendCmd(cmd.toAscii().data(), args.toAscii().data(), &res);
    if (!res.isEmpty())
    {
         /* Remove \r\n */
        QStringList textList = res.split(QRegExp("(\r+)"), QString::SkipEmptyParts);
        QString final_str;
        for (int line = 0; line<textList.size(); line++)
        {
            final_str += textList[line];
        }
        QStringList lines = final_str.split(QRegExp("(\n+)"), QString::SkipEmptyParts);
        for (int line = 0; line < lines.size(); ++line)
        {
            QStandardItem *item = new QStandardItem(lines[line]);
            item->setData(SWITCH_LOG_CONSOLE, ConsoleModel::LogLevelRole);
            addNewConsoleItem(item);
        }
    }
    ui->lineCmd->clear();
}

void ConsoleWindow::lineCmdChanged(QString text)
{
    ui->btnSend->setDisabled(text.isEmpty());
}

void ConsoleWindow::filterModelLogLevel(int level)
{
    model->setLogLevelFilter(level);
}

void ConsoleWindow::loggerHandler(QSharedPointer<switch_log_node_t> node, switch_log_level_t level)
{
    if (level > ui->comboLogLevel->currentIndex()) return;
    QString text(node.data()->data);
    if (!text.isEmpty())
    {
         /* Remove \r\n */
        QStringList textList = text.split(QRegExp("(\r+)"), QString::SkipEmptyParts);
        QString final_str;
        for (int line = 0; line<textList.size(); line++)
        {
            final_str += textList[line];
        }
        QStringList lines = final_str.split(QRegExp("(\n+)"), QString::SkipEmptyParts);
        for (int line = 0; line < lines.size(); ++line)
        {
            QStandardItem *item = new QStandardItem(lines[line]);
            item->setData(level, ConsoleModel::LogLevelRole);
            item->setData(node.data()->userdata, ConsoleModel::UUIDRole);
            addNewConsoleItem(item);
        }
    }
}

void ConsoleWindow::addNewConsoleItem(QStandardItem *item)
{
    QSettings settings;
    settings.beginGroup("Console");
    QPalette palette = settings.value(QString("log-level-%1-palette").arg(item->data(Qt::UserRole).toInt())).value<QPalette>();
    QFont font = settings.value(QString("log-level-%1-font").arg(item->data(Qt::UserRole).toInt())).value<QFont>();
    item->setBackground(palette.base());
    item->setForeground(palette.text());
    item->setFont(font);
    sourceModel->appendRow(item);
}
