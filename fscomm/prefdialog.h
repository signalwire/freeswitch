#ifndef PREFDIALOG_H
#define PREFDIALOG_H

#include <QDialog>
#include <QDomDocument>
#include <QSettings>
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

private slots:
    void configAccepted();

private:
    void getPaDevlist(void);
    Ui::PrefDialog *ui;
    QDomDocument _xmlPaDevList;
    QSettings *_settings;
};

#endif // PREFDIALOG_H
