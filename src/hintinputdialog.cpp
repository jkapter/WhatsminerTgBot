#include "hintinputdialog.h"

#include <QBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>

HintInputDialog::HintInputDialog(QString help_text, QWidget* parent): QDialog(parent)
{

    QBoxLayout* layout = new QVBoxLayout;
    pte_ = new QPlainTextEdit(this);
    layout->addWidget(pte_);

    if(help_text.length() > 0) {
        QPlainTextEdit* help_text_edit = new QPlainTextEdit(this);
        help_text_edit->setMaximumHeight(200);
        help_text_edit->setMinimumHeight(150);
        help_text_edit->setReadOnly(true);
        help_text_edit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        help_text_edit->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        help_text_edit->document()->setPlainText(help_text);
        layout->addWidget(help_text_edit);
    }

    QPushButton* okBtn = new QPushButton("OK");
    connect(okBtn, SIGNAL(clicked()),SLOT(sl_accepted()));
    layout->addWidget(okBtn);

    QPushButton* cancelBtn = new QPushButton("Cancel");
    connect(cancelBtn, SIGNAL(clicked()), SLOT(sl_canceled()));
    layout->addWidget(cancelBtn);

    setLayout(layout);
}

HintInputDialog::~HintInputDialog() {
    delete pte_;
}

void HintInputDialog::sl_accepted() {
    emit sg_applied(pte_->toPlainText());
    this->close();
}

void HintInputDialog::sl_canceled() {
    emit sg_canceling();
    this->close();
}
