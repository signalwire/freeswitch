#ifndef PREFDIALOG_H
#define PREFDIALOG_H

#include <QDialog>
#include <QDomDocument>
#include <fshost.h>

namespace Ui {
    class PrefDialog;
}

class PrefDialog : public QDialog {
    Q_OBJECT
public:
    PrefDialog(QWidget *parent = 0);
    ~PrefDialog();

protected:
    void changeEvent(QEvent *e);

private:
    void getPaDevlist(void);
    Ui::PrefDialog *ui;
    QDomDocument _xmlPaDevList;
};

#endif // PREFDIALOG_H
