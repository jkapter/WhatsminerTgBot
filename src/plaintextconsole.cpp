#include "plaintextconsole.h"

PlainTextConsole::PlainTextConsole(QWidget* parentWidget)
    :QPlainTextEdit(parentWidget)
{
    this->setReadOnly(true);
}

PlainTextConsole& PlainTextConsole::operator<<(ConsoleMessageType message_type) {
    mes_type_ = message_type;
    return *this;
}

void PlainTextConsole::AddDTToMessage(bool b) {
    add_dt_to_mes_ = b;
}

void PlainTextConsole::sl_add_text_to_console(QString text)
{
    ConsoleOutput(text);
}

