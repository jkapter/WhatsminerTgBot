#ifndef PLAINTEXTCONSOLE_H
#define PLAINTEXTCONSOLE_H

#include <QPlainTextEdit>
#include <QDateTime>
#include <QScrollBar>

class QDateTime;

enum class ConsoleMessageType {
    MNORMAL,
    MWARNING,
    MERROR
};

class PlainTextConsole : public QPlainTextEdit
{
    Q_OBJECT
public:
    PlainTextConsole(QWidget* parentWidget);

    template<typename... TArgs>
    void ConsoleOutput(TArgs... args) {
        QString str;
        QTextStream ostream(&str);

        switch (mes_type_) {
        case ConsoleMessageType::MNORMAL:
            ostream << "<font color=\"Black\">";
            break;

        case ConsoleMessageType::MWARNING:
            ostream << "<font color=\"Yellow\">";
            break;

        case ConsoleMessageType::MERROR:
            ostream << "<font color=\"Red\">";
            break;
        }

        if(add_dt_to_mes_) {
            ostream << QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss") << " - ";
        }

        (ostream << ... << args);
        ostream << "</font>";
        this->appendHtml(*ostream.string());
        this->verticalScrollBar()->setValue(this->verticalScrollBar()->maximum());
    }

    template<typename Text>
    PlainTextConsole& operator<<(Text text) {
        ConsoleOutput(text);
        return *this;
    }

    PlainTextConsole& operator<<(ConsoleMessageType message_type);
    void AddDTToMessage(bool b);

public slots:
    void sl_add_text_to_console(QString text);

private:
    ConsoleMessageType mes_type_ = ConsoleMessageType::MNORMAL;
    bool add_dt_to_mes_ = false;
};

#endif // PLAINTEXTCONSOLE_H
