#include "codecwidget.h"
#include "ui_codecwidget.h"
#include "fshost.h"

CodecWidget::CodecWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CodecWidget)
{
    ui->setupUi(this);
    connect(ui->btnEnable, SIGNAL(clicked()), this, SLOT(enableCodecs()));
    connect(ui->btnDisable, SIGNAL(clicked()), this, SLOT(disableCodecs()));
    connect(ui->btnUp, SIGNAL(clicked()), this, SLOT(moveUp()));
    connect(ui->btnDown, SIGNAL(clicked()), this, SLOT(moveDown()));
    readCodecs();
}

CodecWidget::~CodecWidget()
{
    delete ui;
}

void CodecWidget::moveUp()
{
    QList<QListWidgetItem *>items = ui->listEnabledCodecs->selectedItems();
    foreach(QListWidgetItem *item, items)
    {
        int row = ui->listEnabledCodecs->row(item);
        if (row != 0)
            ui->listEnabledCodecs->insertItem(row-1, ui->listEnabledCodecs->takeItem(row));
    }
}

void CodecWidget::moveDown()
{
    QList<QListWidgetItem *>items = ui->listEnabledCodecs->selectedItems();
    foreach(QListWidgetItem *item, items)
    {
        int row = ui->listEnabledCodecs->row(item);
        if (row != ui->listEnabledCodecs->count())
            ui->listEnabledCodecs->insertItem(row+1, ui->listEnabledCodecs->takeItem(row));
    }
}

void CodecWidget::enableCodecs()
{
    QList<QListWidgetItem *>items = ui->listAvailCodecs->selectedItems();
    foreach(QListWidgetItem *item, items)
    {
        ui->listEnabledCodecs->insertItem(0,item->text());
        delete item;
    }
}

void CodecWidget::disableCodecs()
{
    QList<QListWidgetItem *>items = ui->listEnabledCodecs->selectedItems();
    foreach(QListWidgetItem *item, items)
    {
        ui->listAvailCodecs->insertItem(0,item->text());
        delete item;
    }
}

void CodecWidget::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void CodecWidget::readCodecs(void)
{
    /* This is here to show the proper codec config! */
    const switch_codec_implementation_t *codecs[SWITCH_MAX_CODECS];
    uint32_t num_codecs = switch_loadable_module_get_codecs(codecs, sizeof(codecs) / sizeof(codecs[0]));
    uint32_t x;

    for (x = 0; x < num_codecs; x++) {

        /* Codecs we cannot enable/disable or dont want to */
        if (QString(codecs[x]->iananame) == "PROXY" ||
            QString(codecs[x]->iananame) == "PROXY-VID")
        {
            continue;
        }

        QList<QHash<QString, QString> > implList;
        QHash<QString, QString> implPair;
        implPair.insert(QString::number(codecs[x]->samples_per_second), QString::number(codecs[x]->microseconds_per_packet/1000));
        implList.append(implPair);

        /* Iterate over the other implementations */
        switch_codec_implementation_t *curr = codecs[x]->next;
        while (curr != NULL)
        {
            QHash<QString, QString> implPair;
            implPair.insert(QString::number(curr->samples_per_second), QString::number(curr->microseconds_per_packet/1000));
            implList.append(implPair);
            curr = curr->next;
        }
        _listCodecs.insert(codecs[x]->iananame, implList);
        ui->listAvailCodecs->insertItem(0, codecs[x]->iananame);
    }
    ui->listAvailCodecs->sortItems(Qt::AscendingOrder);
}

QString CodecWidget::getCodecString()
{
    QString codecList;
    for(int i = 0; i<ui->listEnabledCodecs->count(); i++)
    {
        QString codecName = ui->listEnabledCodecs->item(i)->text();
        if (!_listCodecs.contains(codecName))
            QMessageBox::warning(this, tr("Error"), tr("Codec %1 does not exist as loaded codec, therefore will not be used.").arg(codecName), QMessageBox::Ok);
        codecList += codecName;
        if (i!= ui->listEnabledCodecs->count()-1)
            codecList += ",";
    }
    return codecList;
}

void CodecWidget::setCodecString(QString codecList)
{
    QStringList rawEnCodecs;
    QStringList split = codecList.split(",");
    foreach(QString s, split)
    {
        QStringList cs = s.split("@");
        if (!rawEnCodecs.contains(cs[0]))
        {
            ui->listEnabledCodecs->insertItem(ui->listEnabledCodecs->count(), cs[0]);
            rawEnCodecs.append(cs[0]);
        }
    }

    foreach(QString c, rawEnCodecs)
    {
        foreach(QListWidgetItem *i, ui->listAvailCodecs->findItems(c, Qt::MatchExactly))
        {
            delete i;
        }
    }
}
