#ifndef HINTINPUTDIALOG_H
#define HINTINPUTDIALOG_H

#include <QDialog>

class QPlainTextEdit;
class QString;

class HintInputDialog: public QDialog
{
    Q_OBJECT

public:
    HintInputDialog(QString help_text = "", QWidget* parent = 0);
    ~HintInputDialog();

signals:
    void sg_applied(QString text);
    void sg_canceling();

private slots:
    void sl_accepted();
    void sl_canceled();

private:
    QPlainTextEdit* pte_;

};

#endif // HINTINPUTDIALOG_H
