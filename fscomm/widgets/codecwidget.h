#ifndef CODECWIDGET_H
#define CODECWIDGET_H

#include <QtGui>

namespace Ui {
    class CodecWidget;
}

class CodecWidget : public QWidget {
    Q_OBJECT
public:
    CodecWidget(QWidget *parent = 0);
    ~CodecWidget();
    QString getCodecString();
    void setCodecString(QString);

protected:
    void changeEvent(QEvent *e);

private slots:
    void enableCodecs();
    void disableCodecs();
    void moveUp();
    void moveDown();

private:
    void readCodecs(void);
    Ui::CodecWidget *ui;
    QHash<QString, QList<QHash<QString, QString> > > _listCodecs;
};

#endif // CODECWIDGET_H
