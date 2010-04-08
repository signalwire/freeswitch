#ifndef STATEDEBUGDIALOG_H
#define STATEDEBUGDIALOG_H

#include <QtGui>
#include "fshost.h"

namespace Ui {
    class StateDebugDialog;
}

class StateDebugDialog : public QDialog {
    Q_OBJECT
public:
    StateDebugDialog(QWidget *parent = 0);
    ~StateDebugDialog();

private slots:
    void newEvent(QSharedPointer<switch_event_t> event);
    void currentUuidChanged();
    void currentEventsChanged();

protected:
    void changeEvent(QEvent *e);

private:
    Ui::StateDebugDialog *ui;
    QHash<QString, QList<QSharedPointer<switch_event_t> > > _events;
};

#endif // STATEDEBUGDIALOG_H
