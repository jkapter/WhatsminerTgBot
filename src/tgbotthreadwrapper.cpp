#include "tgbotthreadwrapper.h"
#include "tgbot/tgbot.h"

#include <QDebug>
#include <QThread>
#include <QAbstractEventDispatcher>
#include <QEventLoop>

using namespace Qt::StringLiterals;

TgBotWorker::TgBotWorker(TgBot::Bot *bot, QObject *parent)
    : QObject(parent)
    , bot_(bot)
{
    qInfo() << QString("TgBotWorker конструктор.");
}

TgBotWorker::~TgBotWorker()
{
    qInfo() << QString("TgBotWorker деструктор.");
}

void TgBotWorker::sl_try_to_connect_api()
{
    if(!bot_) {
        emit sg_try_connection_fail();
        QString mes = u"Телеграм бот: указатель не инициализирован."_s;
        emit sg_emit_text_message(mes);
        qCritical() << mes;
        return;
    }

    try {
        auto me = bot_->getApi().getMe();
        if(me) {
            emit sg_try_connection_ok();
            QString mes = u"Телеграм бот: API телеграм доступно."_s;
            emit sg_emit_text_message(mes);
            qInfo() << mes;
            return;
        }
    } catch (std::exception& e) {
        emit sg_try_connection_fail();
        QString mes = QString("Получено исключение процесса бота: %1").arg(QString::fromStdString(e.what()));
        emit sg_emit_exception(mes);
        qCritical() << mes;
    }

    emit sg_try_connection_fail();
    QString mes = u"Телеграм бот: API телеграм недоступно."_s;
    emit sg_emit_text_message(mes);
    qWarning() << mes;
}

void TgBotWorker::sl_process()
{
    if(!bot_) {
        emit sg_finished();
        return;
    }

    emit sg_emit_text_message("Старт потока Бота.");
    qInfo() << QString("Старт потока бота.");

    try{
        bot_->getApi().getMyCommands();
    } catch    (std::exception &e) {
        emit sg_emit_exception(QString::fromStdString(e.what()));
        qCritical() << QString("Получено исключение процесса бота: %1").arg(QString::fromStdString(e.what()));
        emit sg_finished();
    }


    try {
        bot_->getApi().deleteWebhook();
        TgBot::TgLongPoll longPoll(*bot_, 1, 2, nullptr);
        emit sg_emit_text_message(QString("Бот: старт процесса"));
        qInfo() << QString("Телеграм бот: старт работы бота.");
        while (bot_ && !request_stop_bot_) {
            longPoll.start();
            QThread::currentThread()->eventDispatcher()->processEvents(QEventLoop::AllEvents);
        }
    } catch (std::exception& e) {
        emit sg_emit_text_message(QString("Получено исключение : %1").arg(e.what()));
        qCritical() << QString("Получено исключение процесса бота: %1").arg(QString::fromStdString(e.what()));
        emit sg_finished();
    }
    emit sg_finished();
}

void TgBotWorker::sl_stop_bot()
{
    request_stop_bot_ = true;
    qInfo() << QString("Запрос на остановку бота.");
}
