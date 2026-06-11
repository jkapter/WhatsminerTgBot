#ifndef TGBOTTHREADWRAPPER_H
#define TGBOTTHREADWRAPPER_H

#include <QObject>

namespace TgBot { class Bot;}

class TgBotWorker: public QObject
{
    Q_OBJECT

public:
    explicit TgBotWorker(TgBot::Bot* bot, QObject* parent = 0);
    virtual ~TgBotWorker();

signals:
    void sg_emit_text_message(QString mes);
    void sg_emit_exception(QString what);
    void sg_finished();
    void sg_try_connection_ok();
    void sg_try_connection_fail();

public slots:
    void sl_try_to_connect_api();
    void sl_process();
    void sl_stop_bot();

private:
    TgBot::Bot* bot_;
    bool request_stop_bot_ = false;
};

#endif // TGBOTTHREADWRAPPER_H
