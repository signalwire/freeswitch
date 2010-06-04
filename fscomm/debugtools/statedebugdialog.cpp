#include "statedebugdialog.h"
#include "ui_statedebugdialog.h"

StateDebugDialog::StateDebugDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::StateDebugDialog)
{
    ui->setupUi(this);
    connect(g_FSHost, SIGNAL(newEvent(QSharedPointer<switch_event_t>)), this, SLOT(newEvent(QSharedPointer<switch_event_t>)));
    connect(ui->listUUID, SIGNAL(itemSelectionChanged()), this, SLOT(currentUuidChanged()));
    connect(ui->listEvents, SIGNAL(itemSelectionChanged()), this, SLOT(currentEventsChanged()));
}

StateDebugDialog::~StateDebugDialog()
{
    delete ui;
}

void StateDebugDialog::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void StateDebugDialog::newEvent(QSharedPointer<switch_event_t>event)
{
    /* We don't want to keep track of events that are not calls at this moment */
    if (QString(switch_event_get_header_nil(event.data(), "Unique-ID")).isEmpty())
        return;

    QString uuid(switch_event_get_header_nil(event.data(), "Unique-ID"));

    if (!_events.contains(uuid))
    {
        QList<QSharedPointer<switch_event_t> > tmpListEvents;
        tmpListEvents.append(event);
        _events.insert(uuid, tmpListEvents);
        ui->listUUID->addItem(new QListWidgetItem(uuid));
    }
    else
    {
        QList<QSharedPointer<switch_event_t> > tmpListEvents = _events.value(uuid);
        tmpListEvents.append(event);
        _events.insert(uuid, tmpListEvents);
    }
}

void StateDebugDialog::currentUuidChanged()
{;
    ui->listEvents->clear();
    ui->listDetails->clear();
    QString uuid = ui->listUUID->currentItem()->text();
    foreach(QSharedPointer<switch_event_t> e, _events.value(uuid))
    {
        ui->listEvents->addItem(new QListWidgetItem(switch_event_name(e.data()->event_id)));
    }
}

void StateDebugDialog::currentEventsChanged()
{
    ui->listDetails->clear();
    int r = ui->listEvents->currentRow();
    if (r == -1) return;
    QString uuid = ui->listUUID->currentItem()->text();
    QList<QSharedPointer<switch_event_t> > tmpListEvents = _events.value(uuid);
    QSharedPointer<switch_event_t> e = tmpListEvents.at(r);
    for(switch_event_header_t* h = e.data()->headers; h != e.data()->last_header; h = h->next)
    {
        ui->listDetails->addItem(new QListWidgetItem(QString("%1 = %2").arg(h->name, h->value)));
    }
}
