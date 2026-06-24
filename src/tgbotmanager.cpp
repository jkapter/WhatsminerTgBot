#include "tgbotmanager.h"

#include <QDir>
#include <QFile>
#include <QTimer>
#include <QDateTime>
#include <QDebug>
#include <QThread>
#include <QApplication>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QMessageBox>

#include "tgbot/tools/StringTools.h"
#include "tgbot/Bot.h"

#include "tgbotthreadwrapper.h"

using namespace Qt::StringLiterals;

QString tg_user_type_to_qstring(USER_TYPE type)
{
    switch(type) {
    case USER_TYPE::ADMIN: return "ADMIN";
    case USER_TYPE::BANNED: return "BANNED";
    case USER_TYPE::NEW_USER: return "NEW_USER";
    case USER_TYPE::UNDEFINED: return "UNDEFINED";
    case USER_TYPE::UNREGISTERED: return "UNREGISTERED";
    case USER_TYPE::REGISTRD: return "REGISTRD";
    case USER_TYPE::CHANNEL: return "CHANNEL";
    }
    return "";
}

USER_TYPE tg_user_type_from_qstring(QString type)
{
    if(type == "ADMIN") return USER_TYPE::ADMIN;
    if(type == "BANNED") return USER_TYPE::BANNED;
    if(type == "NEW_USER") return USER_TYPE::NEW_USER;
    if(type == "UNDEFINED") return USER_TYPE::UNDEFINED;
    if(type == "REGISTRD") return USER_TYPE::REGISTRD;
    if(type == "CHANNEL") return USER_TYPE::CHANNEL;
    if(type == "UNREGISTERED") return USER_TYPE::UNREGISTERED;
    return USER_TYPE::UNDEFINED;
}

//===============================================================
//============ T G B O T P A R E N T ============================
//===============================================================

TGParent::TGParent()
    : bot_ptr_(nullptr)
    , http_client_(nullptr)
{}

TgBot::Bot* TGParent::Bot() {
    return bot_ptr_.get();
}

void TGParent::InterruptBotHttpClient()
{
    http_client_->RequestInterrupt();
}

void TGParent::AddOrUpdateChatID(int64_t user, USER_TYPE type) {
    for(auto& [t, id_set]: user_permission_to_chat_id_) {
        if(id_set.count(user) > 0 && t != type) {
            id_set.erase(user);
        }
    }
    user_permission_to_chat_id_[type].insert(user);
}

void TGParent::DeleteChatID(int64_t user) {
    for(auto& [type, us]: user_permission_to_chat_id_) {
        us.erase(user);
    }
}

void TGParent::ClearChatIdData()
{
    user_permission_to_chat_id_.clear();
    inactive_users_.clear();
}

void TGParent::InitializeBot(const std::string& token)
{
    http_client_ = std::make_unique<InterruptibleHttpClient>();
    http_client_->ResetInterrupt();
    bot_ptr_ = std::make_unique<TgBot::Bot>(token, *http_client_);
    ClearChatIdData();
}

std::vector<int64_t> TGParent::GetChatIDs(USER_TYPE min_auth_level) const {
    std::vector<int64_t> ret_vec;

    for(const auto& [type, id_set]: user_permission_to_chat_id_) {
        if(type >= min_auth_level && type != USER_TYPE::BANNED) {
            ret_vec.insert(ret_vec.end(), id_set.begin(), id_set.end());
        }
    }

    return ret_vec;
}

bool TGParent::CheckUserChatId(int64_t chat_id, USER_TYPE min_auth_level) const
{
    for(const auto& [type, id_set]: user_permission_to_chat_id_) {
        if(type >= min_auth_level && type != USER_TYPE::BANNED && id_set.count(chat_id) > 0) {
            return true;
        }
    }
    return false;
}

TgBot::Message::Ptr TGParent::BotSendMessage(int64_t chat_id, const std::string& text, TgBot::InlineKeyboardMarkup::Ptr inline_buttons)
{
    TgBot::InlineKeyboardMarkup::Ptr buttons = inline_buttons;
    if(bot_ptr_) {
        std::string text_to_send(text);
        if(!CheckUserChatId(chat_id, USER_TYPE::REGISTRD)) {
                text_to_send = "Недоступно\\. Обратитесь к администратору\\.";
                buttons = nullptr;
            }

        if(user_permission_to_chat_id_.count(USER_TYPE::CHANNEL) > 0
            && user_permission_to_chat_id_.at(USER_TYPE::CHANNEL).count(chat_id) > 0
            && bot_name_for_channel_.has_value()) {

            text_to_send.insert(0, QString("%1\r\n").arg(bot_name_for_channel_.value()).toStdString());
        }
        try {
            auto ret_ptr = bot_ptr_->getApi().sendMessage(chat_id, text_to_send, nullptr, nullptr, buttons, "MarkdownV2");
            inactive_users_.erase(chat_id);
            return ret_ptr;
        } catch (std::exception& e) {
            qCritical() << QString("Получено исключение при отправке сообщения пользователю id = [%1]: [%2]")
                               .arg(chat_id)
                               .arg(e.what());
            inactive_users_.insert(chat_id);
            return nullptr;
        }
    } else {
        qCritical() << "Указатель на TgBot не инициализирован!";
    }
    return nullptr;
}

void TGParent::BotDeleteMessage(TgBot::Message::Ptr mes)
{
    if(bot_ptr_ && mes) {
        try {
            bot_ptr_->getApi().deleteMessage(mes->chat->id, mes->messageId);
        } catch (std::exception& e) {
            qCritical() << QString("Получено исключение при удалении сообщения id = [%1]: [%2]")
                               .arg(mes->messageId)
                               .arg(e.what());
        }
    } else {
        qCritical() << "Указатель на TgBot не инициализирован!";
    }
}

const std::unordered_set<int64_t>& TGParent::GetInactiveUsers() const
{
    return inactive_users_;
}

void TGParent::SetBotNameForChannel(QString name)
{
    if(name.length() > 0) {
        bot_name_for_channel_ = name;
    } else {
        bot_name_for_channel_.reset();
    }
}

//===============================================================
//============ T G B O T U S E R ================================
//===============================================================
TgBotUser::TgBotUser(TgBot::User& other) {
    copy_fields_(other);
}
TgBotUser::TgBotUser(TgBot::Message::Ptr message) {
    if(message && message->from) {
        copy_fields_(*message->from.get());
        return;
    }
    if(message && message->chat) {
        copy_fields_(*message->chat.get());
        return;
    }

    id = 0;
}
void TgBotUser::copy_fields_(TgBot::User other) {
    addedToAttachmentMenu = other.addedToAttachmentMenu;
    canConnectToBusiness = other.canConnectToBusiness;
    canJoinGroups = other.canJoinGroups;
    canReadAllGroupMessages = other.canReadAllGroupMessages;
    firstName = other.firstName;
    id = other.id;
    isBot = other.isBot;
    isPremium = other.isPremium;
    languageCode = other.languageCode;
    lastName = other.lastName;
    supportsInlineQueries = other.supportsInlineQueries;
    username = other.username;
}
void TgBotUser::copy_fields_(TgBot::Chat chat)
{
    id = chat.id;
    if(chat.type == TgBot::Chat::Type::Channel) {
        type = USER_TYPE::CHANNEL;
    }
    username = chat.username;
    firstName = chat.title;

    addedToAttachmentMenu = false;
    canConnectToBusiness = false;
    canReadAllGroupMessages = false;
    isBot = false;
    isPremium = false;
    languageCode = "";
    lastName = chat.lastName;
}
void TgBotUser::UpdateData(TgBot::User& other) {
    copy_fields_(other);
}
void TgBotUser::UpdateData(TgBot::Message::Ptr message) {
    if(message && message->from) {
        copy_fields_(*message->from.get());
        return;
    }
    if(message && message->chat) {
        copy_fields_(*message->chat.get());
    }
}
//===============================================================
//============ T G B O T M A N A G E R ==========================
//===============================================================

TgBotManager::TgBotManager(const std::string& bot_token, DeviceManager& driver_manager)
    : QObject()
    , bot_token_(bot_token)
    , device_manager_(&driver_manager)
    , check_events_timer_(new QTimer(this))
    , check_restart_timer_(new QTimer(this))
{
    qInfo() << QString("TgBotManager конструктор.");

    QObject::connect(check_events_timer_, SIGNAL(timeout()), this, SLOT(sl_check_events_timer_out()));
    QObject::connect(check_restart_timer_, SIGNAL(timeout()), this, SLOT(sl_check_restart_timer_out()));

    tg_parent_ = std::make_unique<TGParent>();

    check_restart_timer_->setInterval(RESTART_BOT_PERIOD_);
    check_restart_timer_->start();

    restore_user_data_from_file_();

    if(auto_restart_bot_) {
        QTimer::singleShot(RESTART_BOT_PERIOD_, this, &TgBotManager::StartBot);
    }
}

TgBotManager::~TgBotManager() {
    emit sg_stop_bot_thread();
    while(bot_started_ || bot_trying_connect_api_) {
        QThread::currentThread()->eventDispatcher()->processEvents(QEventLoop::AllEvents);
    };

    QDir app_dir(qApp->applicationDirPath());
    bool autosave_flag = app_dir.exists("autosave");
    if(!autosave_flag) {
        autosave_flag = app_dir.mkdir("autosave");
        if(autosave_flag) {
            qInfo() << QString("Папка автосохранения конфигурационных файлов успешно создана.");
        } else {
            qWarning() << QString("Не удалось создать папку для автосохранения конфигурационных файлов.");
        }
    }

    if(autosave_flag) {
        QString users_file = QString("%1%2").arg(app_dir.absolutePath(), "/users.json");
        save_user_data_to_file_(users_file);
    } else {
        qWarning() << "Автосохранение кофигурационных файлов TgBotManager не удалось.";
    }

    qInfo() << QString("TgBotManager деструктор завершен");
}

void TgBotManager::screen_symbols_(std::string& text, const std::string& symbols) const
{
    for(auto it = text.begin(); it < text.end(); ++it) {
        if(symbols.find(*it) != symbols.npos) {
            if(it == text.begin() || *(it-1) != '\\') {
                it = text.insert(it, '\\');
                ++it;
            }
        }
    }
}

void TgBotManager::AddOrUpdateUser(TgBot::Message::Ptr message) {
    if(!message || !message->chat) {
        qCritical() << "Получено пустое сообщение (NULL) или отсутствует id чата";
        return;
    }

    if(CheckUser(message->chat->id) == USER_TYPE::NEW_USER) {
        std::string mes = "Подключен новый пользователь: ";
        mes += message->chat->username;
        mes += " ";
        mes += message->chat->firstName;
        mes += " ";
        mes += message->chat->lastName;
        mes += ", тип = ";
        switch(message->chat->type) {
            case TgBot::Chat::Type::Channel: mes += "Channel"; break;
            case TgBot::Chat::Type::Group: mes += "Group"; break;
            case TgBot::Chat::Type::Private: mes += "Private"; break;
            case TgBot::Chat::Type::Supergroup: mes += "Supergroup";
        }

        emit sg_send_message_to_console(QString::fromStdString(mes));
        qInfo() << QString::fromStdString(mes);

        screen_symbols_(mes, screened_symbols_);
        users_.emplace(message->chat->id, TgBotUser(message));
        users_.at(message->chat->id).type = USER_TYPE::UNREGISTERED;
        tg_parent_->AddOrUpdateChatID(message->chat->id, USER_TYPE::UNREGISTERED);

        TgBot::InlineKeyboardMarkup::Ptr inline_buttons(new TgBot::InlineKeyboardMarkup);
        std::vector<TgBot::InlineKeyboardButton::Ptr> buttons_vec;

        TgBot::InlineKeyboardButton::Ptr reg_btn(new TgBot::InlineKeyboardButton);
        QString text = QString("Зарегистрировать");
        std::string std_text = std::move(text.toStdString());
        screen_symbols_(std_text, screened_symbols_);
        reg_btn->text = std_text;
        reg_btn->callbackData = QString("%1_register_user").arg(message->chat->id).toStdString();
        size_t id = message->chat->id;
        id_to_callback_[reg_btn->callbackData] = [this, id](const TgBot::CallbackQuery::Ptr ptr){RegisterUser(id);};
        buttons_vec.push_back(reg_btn);

        TgBot::InlineKeyboardButton::Ptr ban_btn(new TgBot::InlineKeyboardButton);
        text = QString("Забанить");
        std_text = std::move(text.toStdString());
        screen_symbols_(std_text, screened_symbols_);
        ban_btn->text = std_text;
        ban_btn->callbackData = QString("%1_ban_user").arg(id).toStdString();
        id_to_callback_[ban_btn->callbackData] = [this, id](const TgBot::CallbackQuery::Ptr ptr){BanUser(id);};
        buttons_vec.push_back(ban_btn);
        inline_buttons->inlineKeyboard.push_back(buttons_vec);

        for(const auto& it: GetUsers(USER_TYPE::ADMIN)) {
            tg_parent_->BotSendMessage(it->chatId, mes, inline_buttons);
        }
    } else {

        users_.at(message->chat->id).UpdateData(message);
        qInfo() << QString("Получено сообщение или команда от пользователя ID[%1] Username[%2] [%3] [%4]")
                       .arg(static_cast<quint64>(message->chat->id))
                       .arg(QString::fromStdString(users_.at(message->chat->id).username)
                            , QString::fromStdString(users_.at(message->chat->id).firstName)
                            , QString::fromStdString(users_.at(message->chat->id).lastName));
    }
    users_.at(message->chat->id).chatId = message->chat->id;
    users_.at(message->chat->id).lastMessageDT = QDateTime::currentDateTime();
}

bool TgBotManager::RegisterUser(int64_t user_id) {
    if(CheckUser(user_id) == USER_TYPE::UNREGISTERED) {
        users_.at(user_id).type = USER_TYPE::REGISTRD;
        tg_parent_->AddOrUpdateChatID(user_id, USER_TYPE::REGISTRD);
        QString log_string = QString("Зарегистрирован пользователь: %1").arg(QString::fromStdString(users_.at(user_id).username));
        emit sg_send_message_to_console(log_string);
        qInfo() << log_string;
        return true;
    }
    if(CheckUser(user_id) == USER_TYPE::BANNED) {
        users_.at(user_id).type = USER_TYPE::UNREGISTERED;
        tg_parent_->AddOrUpdateChatID(user_id, USER_TYPE::UNREGISTERED);
        QString log_string = QString("Отменена блокировка пользователя: %1").arg(QString::fromStdString(users_.at(user_id).username));
        emit sg_send_message_to_console(log_string);
        qInfo() << log_string;
        return true;
    }
    return false;
}

USER_TYPE TgBotManager::CheckUser(int64_t user_id) const {
    if(users_.count(user_id) > 0) {
        return users_.at(user_id).type;
    }
    return USER_TYPE::NEW_USER;
}

bool TgBotManager::SetAdmin(int64_t user_id) {
    if(CheckUser(user_id) != USER_TYPE::NEW_USER) {
        users_.at(user_id).type = USER_TYPE::ADMIN;
        QString log_string = QString("Назначен администратор: %1").arg(QString::fromStdString(users_.at(user_id).username));
        emit sg_send_message_to_console(log_string);
        qInfo() << log_string;
        return true;
    }
    return false;
}

bool TgBotManager::ResetAdmin(int64_t user_id) {
    if(CheckUser(user_id) == USER_TYPE::ADMIN) {
        users_.at(user_id).type = USER_TYPE::UNREGISTERED;
        QString log_string = QString("Отменена регистрация администратора: %1").arg(QString::fromStdString(users_.at(user_id).username));
        emit sg_send_message_to_console(log_string);
        qInfo() << log_string;
        return true;
    }
    return false;
}

bool TgBotManager::BanUser(int64_t user_id) {
    if(CheckUser(user_id) != USER_TYPE::NEW_USER
        && CheckUser(user_id) != USER_TYPE::BANNED
        && CheckUser(user_id) != USER_TYPE::ADMIN) {
        users_.at(user_id).type = USER_TYPE::BANNED;
        tg_parent_->AddOrUpdateChatID(user_id, USER_TYPE::BANNED);
        QString log_string = QString("Забанен пользователь: %1").arg(QString::fromStdString(users_.at(user_id).username));
        emit sg_send_message_to_console(log_string);
        qInfo() << log_string;
        return true;
    }
    return false;
}

void TgBotManager::make_admin_tools_() {
    admin_tools_to_callback_.clear();

    TgBot::InlineKeyboardMarkup::Ptr main_admin_kb(new TgBot::InlineKeyboardMarkup);
    TgBot::InlineKeyboardButton::Ptr btn(new TgBot::InlineKeyboardButton);
    btn->text = "Все пользователи";
    btn->callbackData = "admin_tools_get_all_users";
    main_admin_kb->inlineKeyboard.push_back({btn});
    admin_tools_to_callback_["admin_tools_get_all_users"] = [this](const TgBot::CallbackQuery::Ptr query) {
        std::vector<TgBotUser const *> users = GetUsers();
        QString mes = "Все пользователи:\n";
        for(const auto& it: users) {
            mes.append(QString("*%1* %2 %3 %4: %5 \n").arg(it->id).arg(QString::fromStdString(it->username)
                                                                       ,QString::fromStdString(it->firstName)
                                                                       , QString::fromStdString(it->lastName)
                                                                       ,it->lastMessageDT.toString("dd.MM.yyyy hh:mm:ss")));
        }
        std::string std_mes = mes.toStdString();
        screen_symbols_(std_mes, screened_symbols_);
        tg_parent_->BotSendMessage(query->message->chat->id, std_mes);
    };

    tg_parent_->Bot()->getEvents().onCommand("admin", [this, main_admin_kb](TgBot::Message::Ptr message) {
        QString log_string{"Получена команда администрирования."};
        emit sg_send_message_to_console(log_string);
        qInfo() << log_string;

        if(!message || !message->chat) {
            qCritical() << "Пустой указатель на сообщение в команде администрирования";
            return;
        }

        if(CheckUser(message->chat->id) == USER_TYPE::ADMIN) {
            tg_parent_->BotSendMessage(message->chat->id, "Доступные команды:", main_admin_kb);
        } else {
            tg_parent_->BotSendMessage(message->chat->id, "Вы не администратор");
        }
    });
}

void TgBotManager::process_callbacks_(const TgBot::CallbackQuery::Ptr cb_query)
{
    if(id_to_callback_.contains(cb_query->data)) {
        auto cb_func = id_to_callback_.at(cb_query->data);
        cb_func(cb_query);
    } else if(admin_tools_to_callback_.contains(cb_query->data)) {
        auto cb_func = admin_tools_to_callback_.at(cb_query->data);
        cb_func(cb_query);
    }
    else {
        tg_parent_->BotSendMessage(cb_query->message->chat->id, "Неизвестная команда");
    }
}

std::vector<TgBotUser const *> TgBotManager::GetUsers(USER_TYPE type) {
    std::vector<TgBotUser const *> ret_vec;

    for(const auto& it: tg_parent_->GetInactiveUsers()) {
        if(users_.count(it) > 0) {
            users_.at(it).userActive = false;
        }
    }

    for(const auto& [id, it]: users_) {
        if(it.type == type || type == USER_TYPE::UNDEFINED) {
            ret_vec.push_back(&it);
        }
    }
    return ret_vec;
}

TGParent *TgBotManager::GetTGParent() const
{
    return tg_parent_.get();
}

void TgBotManager::sl_set_settings_values(QString type, bool active, int value, int hysteresis)
{
    if(type == "liquid_temp_notification") {
        liquid_notifications_.active = active;
        liquid_notifications_.value = value;
        liquid_notifications_.hysteresis = hysteresis;
    }

    if(type == "psu_temp_notification") {
        psu_notifications_.active = active;
        psu_notifications_.value = value;
        psu_notifications_.hysteresis = hysteresis;
    }

    if(type == "chip_temp_notification") {
        chip_notifications_.active = active;
        chip_notifications_.value = value;
        chip_notifications_.hysteresis = hysteresis;
    }
}


void TgBotManager::make_users_processing_() {
    tg_parent_->Bot()->getEvents().onAnyMessage([this](TgBot::Message::Ptr message) {
        AddOrUpdateUser(message);

        if(message->replyToMessage && chat_to_messages_wait_limit_.contains(message->chat->id)) {

            if(message->replyToMessage->messageId == chat_to_messages_wait_limit_.at(message->chat->id).second->messageId) {
                bool b;
                QString addr = chat_to_messages_wait_limit_.at(message->chat->id).first;
                int limit = QString::fromStdString(message->text).toInt(&b);
                if(b && limit > 0 && limit < 20000) {
                    QString mes = u"Команда выполнена: "_s;
                    mes.append(device_manager_->SetPowerLimit(addr, limit));
                    std::string std_mes = mes.toStdString();
                    screen_symbols_(std_mes, screened_symbols_);
                    tg_parent_->BotSendMessage(message->chat->id, std_mes);
                } else {
                    tg_parent_->BotSendMessage(message->chat->id, "Неверное числовое значение");
                }
                chat_to_messages_wait_limit_.erase(message->chat->id);
            }
        }

        if(chat_to_messages_to_delete_.contains(message->chat->id)) {
            tg_parent_->BotDeleteMessage(chat_to_messages_to_delete_.at(message->chat->id));
            chat_to_messages_to_delete_.erase(message->chat->id);
        }
    });
}

void TgBotManager::make_commands_processing_()
{
    std::vector<TgBot::BotCommand::Ptr> main_menu;


    TgBot::BotCommand::Ptr cmd_start(new TgBot::BotCommand);
    cmd_start->command = "start";
    cmd_start->description = "Начало";

    main_menu.push_back(cmd_start);

    auto CommandExec = [this](const TgBot::Message::Ptr mes){
        TgBot::InlineKeyboardMarkup::Ptr inline_buttons(new TgBot::InlineKeyboardMarkup);

        for(const auto& it: device_manager_->GetDeviceAddresses()) {
            std::vector<TgBot::InlineKeyboardButton::Ptr> buttons_vec;
            TgBot::InlineKeyboardButton::Ptr dev_btn(new TgBot::InlineKeyboardButton);
            QString text = QString("[%1]: %2").arg(it, device_manager_->GetStringValue(it, u"WorkerName"_s));
            std::string std_text = std::move(text.toStdString());
            screen_symbols_(std_text, screened_symbols_);
            dev_btn->text = std_text;

            std::string callback_id = std::move(QString("%1_main_inlbtn").arg(it).toStdString());

            dev_btn->callbackData = callback_id;
            buttons_vec.push_back(dev_btn);
            inline_buttons->inlineKeyboard.push_back(buttons_vec);
        }

        tg_parent_->BotSendMessage(mes->chat->id, "Доступные приборы:", inline_buttons);
    };

    GetTGParent()->Bot()->getEvents().onCommand(cmd_start->command, CommandExec);

    if(tg_parent_->Bot() && !main_menu.empty()) {
        tg_parent_->Bot()->getApi().setMyCommands(main_menu);
    }
}

void TgBotManager::make_callback_data_()
{
    id_to_callback_.clear();

    for(const auto& it: device_manager_->GetDeviceAddresses()) {
        std::string callback_id = std::move(QString("%1_main_inlbtn").arg(it).toStdString());

        auto callback_fnc = [this, it](const TgBot::CallbackQuery::Ptr query) {
            QString mes = QString("*Прибор %1: %2*\n").arg(it, device_manager_->GetStringValue(it, u"WorkerName"_s));
            mes.append(QString("Тип: %1\n").arg(device_manager_->GetStringValue(it, u"DeviceType"_s)));
            mes.append(QString("Прибор Онлайн: %1\n").arg(device_manager_->GetStringValue(it, u"Online"_s)));
            mes.append(QString("Пул Онлайн: %1\n").arg(device_manager_->GetStringValue(it, u"PoolOnline"_s)));
            mes.append(QString("Есть ошибки: %1\n").arg(device_manager_->GetStringValue(it, u"HasErrors"_s)));
            mes.append(QString("Температура теплоносителя на входе: %1\n").arg(device_manager_->GetStringValue(it, u"LiquidTemp"_s)));
            mes.append(QString("Температура блока питания: %1\n").arg(device_manager_->GetStringValue(it, u"PSUTemperature"_s)));
            mes.append(QString("Температура плат: %1\n").arg(device_manager_->GetStringValue(it, u"BoardTemps"_s)));
            mes.append(QString("Хэшрейт за 15мин: %1\n").arg(device_manager_->GetStringValue(it, u"THS15m"_s)));
            mes.append(QString("Мощность мгновенная: %1\n").arg(device_manager_->GetStringValue(it, u"Power"_s)));
            mes.append(QString("Ограничение мощности: %1\n").arg(device_manager_->GetStringValue(it, u"PowerLimit"_s)));

            std::string std_mes = std::move(mes.toUtf8().toStdString());
            screen_symbols_(std_mes, screened_symbols_);

            TgBot::InlineKeyboardMarkup::Ptr inline_buttons(new TgBot::InlineKeyboardMarkup);
            std::vector<TgBot::InlineKeyboardButton::Ptr> buttons_vec;

            TgBot::InlineKeyboardButton::Ptr err_btn(new TgBot::InlineKeyboardButton);
            QString text = QString("Ошибки");
            std::string std_text = std::move(text.toStdString());
            screen_symbols_(std_text, screened_symbols_);
            err_btn->text = std_text;
            err_btn->callbackData = QString("%1_1_errors").arg(it).toStdString();
            buttons_vec.push_back(err_btn);

            TgBot::InlineKeyboardButton::Ptr res_btn(new TgBot::InlineKeyboardButton);
            text = QString("Перезагрузить прибор");
            std_text = std::move(text.toStdString());
            screen_symbols_(std_text, screened_symbols_);
            res_btn->text = std_text;
            res_btn->callbackData = QString("%1_1_restart").arg(it).toStdString();
            buttons_vec.push_back(res_btn);

            std::vector<TgBot::InlineKeyboardButton::Ptr> buttons2_vec;
            TgBot::InlineKeyboardButton::Ptr limit_btn(new TgBot::InlineKeyboardButton);
            text = QString("Установить лимит мощности");
            std_text = std::move(text.toStdString());
            screen_symbols_(std_text, screened_symbols_);
            limit_btn->text = std_text;
            limit_btn->callbackData = QString("%1_1_limit_power").arg(it).toStdString();
            buttons2_vec.push_back(limit_btn);

            inline_buttons->inlineKeyboard.push_back(buttons_vec);
            inline_buttons->inlineKeyboard.push_back(buttons2_vec);

            tg_parent_->BotSendMessage(query->message->chat->id, std_mes, inline_buttons);
        };
        id_to_callback_[callback_id] = callback_fnc;

        auto callback_error_fnc = [this, it](const TgBot::CallbackQuery::Ptr query) {
            QString mes = QString("*Прибор %1: %2*\n").arg(it, device_manager_->GetStringValue(it, u"WorkerName"_s));
            mes.append(QString("Ошибки: %1\n %2\n").arg(device_manager_->GetStringValue(it, u"HasErrors"_s), device_manager_->GetStringValue(it, u"ErrorCodes"_s)));

            std::string std_mes = std::move(mes.toUtf8().toStdString());
            screen_symbols_(std_mes, screened_symbols_);

            tg_parent_->BotSendMessage(query->message->chat->id, std_mes);
        };
        id_to_callback_[QString("%1_1_errors").arg(it).toStdString()] = callback_error_fnc;

        auto callback_reset_fnc = [this, it](const TgBot::CallbackQuery::Ptr query) {
            QString mes = QString("Перезагрузить прибор?");
            std::string std_mes = std::move(mes.toUtf8().toStdString());
            screen_symbols_(std_mes, screened_symbols_);

            TgBot::InlineKeyboardMarkup::Ptr inline_buttons(new TgBot::InlineKeyboardMarkup);
            std::vector<TgBot::InlineKeyboardButton::Ptr> buttons_vec;

            TgBot::InlineKeyboardButton::Ptr ok_btn(new TgBot::InlineKeyboardButton);
            QString text = QString("ДА");
            std::string std_text = std::move(text.toStdString());
            screen_symbols_(std_text, screened_symbols_);
            ok_btn->text = std_text;
            ok_btn->callbackData = QString("%1_2_ok_to_reboot").arg(it).toStdString();
            buttons_vec.push_back(ok_btn);

            inline_buttons->inlineKeyboard.push_back(buttons_vec);

            auto mes_ptr = tg_parent_->BotSendMessage(query->message->chat->id, std_mes, inline_buttons);
            if(mes_ptr) {
                chat_to_messages_to_delete_[mes_ptr->chat->id] = mes_ptr;
            }
        };
        id_to_callback_[QString("%1_1_restart").arg(it).toStdString()] = callback_reset_fnc;

        auto callback_reset_accept_fnc = [this, it](const TgBot::CallbackQuery::Ptr query) {
            QString mes = QString("Команда выполнена: ");
            mes.append(device_manager_->RebootDevice(it));
            std::string std_mes = mes.toStdString();
            screen_symbols_(std_mes, screened_symbols_);
            tg_parent_->BotSendMessage(query->message->chat->id, std_mes);
            if(chat_to_messages_to_delete_.contains(query->message->chat->id)) {
                tg_parent_->BotDeleteMessage(chat_to_messages_to_delete_.at(query->message->chat->id));
                chat_to_messages_to_delete_.erase(query->message->chat->id);
            }
        };
        id_to_callback_[QString("%1_2_ok_to_reboot").arg(it).toStdString()] = callback_reset_accept_fnc;

        auto callback_set_limit_fnc = [this, it](const TgBot::CallbackQuery::Ptr query) {
            QString mes = QString("Текущий лимит мощности: %1 Вт\n").arg(device_manager_->GetStringValue(it, u"PowerLimit"_s));
            mes.append("В ответ на это сообщение напишите новый лимит");
            std::string std_mes = mes.toStdString();
            screen_symbols_(std_mes, screened_symbols_);
            auto mes_ptr = tg_parent_->BotSendMessage(query->message->chat->id, std_mes);
            if(mes_ptr) {
                chat_to_messages_wait_limit_[mes_ptr->chat->id] = {it, mes_ptr};
                chat_to_messages_to_delete_[mes_ptr->chat->id] = mes_ptr;
            }
        };
        id_to_callback_[QString("%1_1_limit_power").arg(it).toStdString()] = callback_set_limit_fnc;
    }

    tg_parent_->Bot()->getEvents().onCallbackQuery([this](const TgBot::CallbackQuery::Ptr cb_query){
        process_callbacks_(cb_query);
    });
}

void TgBotManager::make_triggers_data_()
{

    triggers_for_check_.clear();
    triggers_previous_check_.clear();
    triggers_actions_.clear();
    address_to_trigger_states_.clear();

    for(const auto& it: device_manager_->GetDeviceAddresses()) {
        address_to_trigger_states_[it];
    }

    std::string trigger_name;
    std::function<bool(QString)> trigger_func;
    std::function<void(QString)> trigger_action;

    trigger_name = "get_offline";
    trigger_func = [this](QString addr){
        WhatsminerData miner_data = device_manager_->GetDeviceData(addr);
        return !miner_data.Online;
    };
    trigger_action = [this](QString addr){
        QString mes = QString("Прибор [%1][%2] потеря связи").arg(addr, device_manager_->GetStringValue(addr, u"WorkerName"_s));
        std::string std_mes = mes.toStdString();
        screen_symbols_(std_mes, screened_symbols_);
        auto ids_vec = tg_parent_->GetChatIDs(USER_TYPE::REGISTRD);
        for(const auto& it: ids_vec) {
            tg_parent_->BotSendMessage(it, std_mes);
        }
    };
    triggers_for_check_[trigger_name] = trigger_func;
    for(const auto& it: device_manager_->GetDeviceAddresses()) {
        triggers_previous_check_[trigger_name][it] = false;
    }
    triggers_actions_[trigger_name] = trigger_action;

    trigger_name = "get_online";
    trigger_func = [this](QString addr){
        WhatsminerData miner_data = device_manager_->GetDeviceData(addr);
        return miner_data.Online;
    };
    trigger_action = [this](QString addr){
        QString mes = QString("Прибор [%1][%2] связь восстановлена").arg(addr, device_manager_->GetStringValue(addr, u"WorkerName"_s));
        std::string std_mes = mes.toStdString();
        screen_symbols_(std_mes, screened_symbols_);
        auto ids_vec = tg_parent_->GetChatIDs(USER_TYPE::REGISTRD);
        for(const auto& it: ids_vec) {
            tg_parent_->BotSendMessage(it, std_mes);
        }
    };
    triggers_for_check_[trigger_name] = trigger_func;
    for(const auto& it: device_manager_->GetDeviceAddresses()) {
        triggers_previous_check_[trigger_name][it] = false;
    }
    triggers_actions_[trigger_name] = trigger_action;

    trigger_name = "get_pool_offline";
    trigger_func = [this](QString addr){
        WhatsminerData miner_data = device_manager_->GetDeviceData(addr);
        return !miner_data.PoolOnline;
    };
    trigger_action = [this](QString addr){
        QString mes = QString("Прибор [%1][%2] потеря связи с пулом").arg(addr, device_manager_->GetStringValue(addr, u"WorkerName"_s));
        std::string std_mes = mes.toStdString();
        screen_symbols_(std_mes, screened_symbols_);
        auto ids_vec = tg_parent_->GetChatIDs(USER_TYPE::REGISTRD);
        for(const auto& it: ids_vec) {
            tg_parent_->BotSendMessage(it, std_mes);
        }
    };
    triggers_for_check_[trigger_name] = trigger_func;
    for(const auto& it: device_manager_->GetDeviceAddresses()) {
        triggers_previous_check_[trigger_name][it] = false;
    }
    triggers_actions_[trigger_name] = trigger_action;

    trigger_name = "get_pool_online";
    trigger_func = [this](QString addr){
        WhatsminerData miner_data = device_manager_->GetDeviceData(addr);
        return miner_data.PoolOnline;
    };
    trigger_action = [this](QString addr){
        QString mes = QString("Прибор [%1][%2] связь с пулом восстановлена").arg(addr, device_manager_->GetStringValue(addr, u"WorkerName"_s));
        std::string std_mes = mes.toStdString();
        screen_symbols_(std_mes, screened_symbols_);
        auto ids_vec = tg_parent_->GetChatIDs(USER_TYPE::REGISTRD);
        for(const auto& it: ids_vec) {
            tg_parent_->BotSendMessage(it, std_mes);
        }
    };
    triggers_for_check_[trigger_name] = trigger_func;
    for(const auto& it: device_manager_->GetDeviceAddresses()) {
        triggers_previous_check_[trigger_name][it] = false;
    }
    triggers_actions_[trigger_name] = trigger_action;

    trigger_name = "liquid_hot";
    trigger_func = [this](QString addr){
        if(!liquid_notifications_.active) return false;
        WhatsminerData miner_data = device_manager_->GetDeviceData(addr);
        if(!address_to_trigger_states_.at(addr).liquid && miner_data.LiquidTemp >= liquid_notifications_.value) {
            address_to_trigger_states_.at(addr).liquid = true;
            return true;
        }
        if(address_to_trigger_states_.at(addr).liquid && (miner_data.LiquidTemp <= (liquid_notifications_.value - liquid_notifications_.hysteresis))) {
            address_to_trigger_states_.at(addr).liquid = false;
        }
        return false;
    };
    trigger_action = [this](QString addr){
        QString mes = QString("Прибор [%1][%2] температрура теплоносителя более %3 градусов")
                          .arg(addr, device_manager_->GetStringValue(addr, u"WorkerName"_s))
                          .arg(liquid_notifications_.value);
        std::string std_mes = mes.toStdString();
        screen_symbols_(std_mes, screened_symbols_);
        auto ids_vec = tg_parent_->GetChatIDs(USER_TYPE::REGISTRD);
        for(const auto& it: ids_vec) {
            tg_parent_->BotSendMessage(it, std_mes);
        }
    };
    triggers_for_check_[trigger_name] = trigger_func;
    for(const auto& it: device_manager_->GetDeviceAddresses()) {
        triggers_previous_check_[trigger_name][it] = false;
    }
    triggers_actions_[trigger_name] = trigger_action;

    trigger_name = "psu_hot";
    trigger_func = [this](QString addr){
        if(!psu_notifications_.active) return false;
        WhatsminerData miner_data = device_manager_->GetDeviceData(addr);
        if(!address_to_trigger_states_.at(addr).psu && miner_data.PSUTemperature >= psu_notifications_.value) {
            address_to_trigger_states_.at(addr).psu = true;
            return true;
        }
        if(address_to_trigger_states_.at(addr).psu && (miner_data.PSUTemperature <= (psu_notifications_.value - psu_notifications_.hysteresis))) {
            address_to_trigger_states_.at(addr).psu = false;
        }
        return false;
    };
    trigger_action = [this](QString addr){
        QString mes = QString("Прибор [%1][%2] температрура блока питания более %3 градусов")
                          .arg(addr, device_manager_->GetStringValue(addr, u"WorkerName"_s))
                          .arg(psu_notifications_.value);
        std::string std_mes = mes.toStdString();
        screen_symbols_(std_mes, screened_symbols_);
        auto ids_vec = tg_parent_->GetChatIDs(USER_TYPE::REGISTRD);
        for(const auto& it: ids_vec) {
            tg_parent_->BotSendMessage(it, std_mes);
        }
    };
    triggers_for_check_[trigger_name] = trigger_func;
    for(const auto& it: device_manager_->GetDeviceAddresses()) {
        triggers_previous_check_[trigger_name][it] = false;
    }
    triggers_actions_[trigger_name] = trigger_action;

    trigger_name = "chip_hot";
    trigger_func = [this](QString addr){
        if(!chip_notifications_.active) return false;
        WhatsminerData miner_data = device_manager_->GetDeviceData(addr);
        if(!address_to_trigger_states_.at(addr).chip && miner_data.ChipTempMax >= chip_notifications_.value) {
            address_to_trigger_states_.at(addr).chip = true;
            return true;
        }
        if(address_to_trigger_states_.at(addr).chip && (miner_data.ChipTempMax <= (chip_notifications_.value - chip_notifications_.hysteresis))) {
            address_to_trigger_states_.at(addr).chip = false;
        }
        return false;
    };
    trigger_action = [this](QString addr){
        QString mes = QString("Прибор [%1][%2] макс. температура чипа более %3 градусов")
                          .arg(addr, device_manager_->GetStringValue(addr, u"WorkerName"_s))
                          .arg(chip_notifications_.value);
        std::string std_mes = mes.toStdString();
        screen_symbols_(std_mes, screened_symbols_);
        auto ids_vec = tg_parent_->GetChatIDs(USER_TYPE::REGISTRD);
        for(const auto& it: ids_vec) {
            tg_parent_->BotSendMessage(it, std_mes);
        }
    };
    triggers_for_check_[trigger_name] = trigger_func;
    for(const auto& it: device_manager_->GetDeviceAddresses()) {
        triggers_previous_check_[trigger_name][it] = false;
    }
    triggers_actions_[trigger_name] = trigger_action;

    trigger_name = "has_errors";
    trigger_func = [this](QString addr){
        WhatsminerData miner_data = device_manager_->GetDeviceData(addr);
        return miner_data.HasErrors;
    };
    trigger_action = [this](QString addr){
        QString mes = QString("Прибор [%1][%2] есть ошибки").arg(addr, device_manager_->GetStringValue(addr, u"WorkerName"_s));
        std::string std_mes = mes.toStdString();
        screen_symbols_(std_mes, screened_symbols_);
        auto ids_vec = tg_parent_->GetChatIDs(USER_TYPE::REGISTRD);
        for(const auto& it: ids_vec) {
            tg_parent_->BotSendMessage(it, std_mes);
        }
    };
    triggers_for_check_[trigger_name] = trigger_func;
    for(const auto& it: device_manager_->GetDeviceAddresses()) {
        triggers_previous_check_[trigger_name][it] = false;
    }
    triggers_actions_[trigger_name] = trigger_action;
}

void TgBotManager::make_new_bot_()
{
    if(bot_started_) {
        emit sg_stop_bot_thread();
        while(bot_started_) {
            QThread::currentThread()->eventDispatcher()->processEvents(QEventLoop::AllEvents);
        }
    }
    tg_parent_->InitializeBot(bot_token_);
}

bool TgBotManager::BotIsWorking() const {
    return bot_started_;
}

void TgBotManager::initialize_bot_() {

    if(!tg_parent_->Bot()) return;

    for(const auto& [id, user]: users_) {
        tg_parent_->AddOrUpdateChatID(user.chatId, user.type);
    }

    if(bot_name_for_channels_.has_value()) {
        tg_parent_->SetBotNameForChannel(bot_name_for_channels_.value());
    }

    try {
        tg_parent_->Bot()->getApi();
        make_admin_tools_();
        make_users_processing_();
        make_commands_processing_();
        make_callback_data_();
        make_triggers_data_();

    } catch (std::exception &e) {
        sl_bot_throw_exception(e.what());
        return;
    }

    emit sg_send_message_to_console(QString("Бот инициализирован"));
    qInfo() << QString("Бот инициализирован");

}

void TgBotManager::StartBot() {

    make_new_bot_();

    if(tg_parent_->Bot()) {
        QThread* bot_thread = new QThread(this);
        TgBotWorker* bot_worker = new TgBotWorker(tg_parent_->Bot());
        QObject::connect(bot_thread, &QThread::started, bot_worker, &TgBotWorker::sl_try_to_connect_api);
        QObject::connect(bot_worker, &TgBotWorker::sg_emit_text_message, this, &TgBotManager::sg_send_message_to_console);
        QObject::connect(bot_worker, &TgBotWorker::sg_emit_exception, this, &TgBotManager::sl_bot_throw_exception);
        QObject::connect(bot_worker, &TgBotWorker::sg_try_connection_ok, bot_thread, &QThread::quit);
        QObject::connect(bot_worker, &TgBotWorker::sg_try_connection_fail, bot_thread, &QThread::quit);
        QObject::connect(bot_worker, &TgBotWorker::sg_try_connection_ok, this, &TgBotManager::sl_try_to_connect_api_successful);
        QObject::connect(bot_thread, &QThread::finished, this, [this](){bot_trying_connect_api_ = false;});
        QObject::connect(bot_thread, &QThread::finished, bot_thread, &QObject::deleteLater, Qt::QueuedConnection);
        QObject::connect(this, &TgBotManager::sg_stop_bot_thread, bot_worker, &TgBotWorker::sl_stop_bot);
        QObject::connect(this, &TgBotManager::sg_stop_bot_thread, this, [this]{tg_parent_->InterruptBotHttpClient();});
        QObject::connect(bot_thread, &QThread::finished, [bot_worker] {delete bot_worker;});

        bot_trying_connect_api_ = true;

        bot_worker->moveToThread(bot_thread);
        bot_thread->start();
    }
}

void TgBotManager::sl_try_to_connect_api_successful()
{
    bot_trying_connect_api_ = false;
    initialize_bot_();

    if(tg_parent_->Bot()) {
        QThread* bot_thread = new QThread(this);
        TgBotWorker* bot_worker = new TgBotWorker(tg_parent_->Bot());
        QObject::connect(bot_thread, SIGNAL(started()), this, SIGNAL(sg_bot_thread_state_changed()));
        QObject::connect(bot_thread, SIGNAL(started()), this, SLOT(sl_bot_started()));
        QObject::connect(bot_thread, SIGNAL(started()), bot_worker, SLOT(sl_process()));
        QObject::connect(bot_worker, SIGNAL(sg_emit_text_message(QString)), this, SIGNAL(sg_send_message_to_console(QString)));
        QObject::connect(bot_worker, SIGNAL(sg_emit_exception(QString)), this, SLOT(sl_bot_throw_exception(QString)));
        QObject::connect(bot_worker, SIGNAL(sg_finished()), bot_thread, SLOT(quit()));
        QObject::connect(bot_thread, SIGNAL(finished()), bot_thread, SLOT(deleteLater()), Qt::QueuedConnection);
        QObject::connect(bot_thread, SIGNAL(finished()), this, SLOT(sl_bot_finished()), Qt::QueuedConnection);
        QObject::connect(bot_thread, SIGNAL(finished()), this, SIGNAL(sg_bot_thread_state_changed()), Qt::QueuedConnection);
        QObject::connect(this, SIGNAL(sg_stop_bot_thread()), bot_worker, SLOT(sl_stop_bot()));
        QObject::connect(bot_thread, &QThread::finished, [bot_worker] {delete bot_worker;});

        bot_started_ = true;

        bot_worker->moveToThread(bot_thread);
        bot_thread->start();
    }
}


void TgBotManager::StopBot() {
    emit sg_stop_bot_thread();
    for(const auto& it: tg_parent_->GetInactiveUsers()) {
        if(users_.count(it) > 0) {
            users_.at(it).userActive = false;
        }
    }
}

bool TgBotManager::IsAutoRestart() const {
    return auto_restart_bot_;
}

void TgBotManager::SetAutoRestartBot(bool b) {
    auto_restart_bot_ = b;
    emit sg_send_message_to_console(QString("Изменен автозапуск бота: %1").arg(b));
    qInfo() << QString("Изменен автозапуск бота: %1").arg(b ? "АВТО" : "РУЧН");
}

void TgBotManager::sl_bot_finished() {
    bot_started_ = false;
    emit sg_send_message_to_console(QString("Бот остановлен."));
    qInfo() << QString("Бот остановлен.");
    check_events_timer_->stop();
}

void TgBotManager::sl_bot_started()
{
    emit sg_send_message_to_console(QString("Бот запущен."));
    check_events_timer_->setInterval(CHECK_EVENTS_PERIOD_);
    check_events_timer_->start();
    error_message_.reset(nullptr);
}


void TgBotManager::sl_bot_throw_exception(QString what)
{
    error_message_.reset(new QMessageBox(QMessageBox::Critical, "Ошибка!"
                                         , QString("Ошибка инициализации Бота, проверьте токен!\nПерезапустите приложение.\n %1").arg(what)
                                         , QMessageBox::Ok
                                         , nullptr
                                         , Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint | Qt::WindowStaysOnTopHint));
    error_message_->setWindowModality(Qt::NonModal);
    error_message_->show();
    emit sg_stop_bot_thread();
    emit sg_send_message_to_console(QString("Бот: получено исключение - %1").arg(what));
}

void TgBotManager::sl_check_events_timer_out()
{
    for(const auto& addr: device_manager_->GetDeviceAddresses()) {
        for(const auto& [id, trig]: triggers_for_check_) {
            if(trig(addr) && !triggers_previous_check_.at(id).at(addr)) {
                triggers_actions_.at(id)(addr);
            }
            triggers_previous_check_.at(id).at(addr) = trig(addr);
        }
    }
}

void TgBotManager::sl_check_restart_timer_out() {
    if(bot_started_ || bot_trying_connect_api_) {
        return;
    }

    if(auto_restart_bot_) {
        ++restart_counter_;
        emit sg_send_message_to_console(QString("Бот: автоматический рестарт."));
        qWarning() << QString("Бот: автоматический рестарт. Попытка %1").arg(restart_counter_ - 1);
        StartBot();
    }
}

const std::string& TgBotManager::GetToken() const {
    return bot_token_;
}

void TgBotManager::SendMessageToUsers(const std::vector<size_t>& id, QString message) const {
    if(!bot_started_) return;
    for(const auto& it: id) {
        if(users_.count(it) == 0) continue;
        tg_parent_->BotSendMessage(users_.at(it).chatId, message.toStdString());
    }
}

void TgBotManager::SaveUserData()
{
    QDir app_dir(qApp->applicationDirPath());
    QString users_file = QString("%1%2").arg(app_dir.absolutePath(), "/users.json");
    save_user_data_to_file_(users_file);
}

void TgBotManager::SetBotNameForChannels(QString name)
{
    if(name.length() > 0) {
        bot_name_for_channels_ = name;
    } else {
        bot_name_for_channels_.reset();
    }
}

QString TgBotManager::GetBotNameForChannels() const
{
    return bot_name_for_channels_.has_value() ? bot_name_for_channels_.value() : QString();
}

bool TgBotManager::save_user_data_to_file_(const QString& filename) const {
    QFile file(filename);
    if(file.open(QIODeviceBase::WriteOnly)) {
        QJsonArray users_array;
        for(const auto& [id, user]: users_) {
            QJsonObject obj;
            obj.insert("first_name", QString::fromStdString(user.firstName));
            obj.insert("last_name", QString::fromStdString(user.lastName));
            obj.insert("username", QString::fromStdString(user.username));
            obj.insert("chat_id", user.chatId);
            obj.insert("type", tg_user_type_to_qstring(user.type));
            obj.insert("id", id);
            obj.insert("user_active", user.userActive);
            users_array.append(obj);
        }
        QJsonDocument doc(users_array);
        bool res = file.write(doc.toJson()) != (-1);
        qInfo() << QString("Результат записи данных пользователей в файл %1: %2").arg(filename, res ? "OK" : "ERROR");
        return res;
    }
    qWarning() << QString("Ошибка открытия файла %1 для записи").arg(filename);
    return false;
}

bool TgBotManager::restore_user_data_from_file_() {
    QFile file("users.json");
    if(file.open(QIODeviceBase::ReadOnly)) {
        QJsonParseError json_error;
        QJsonDocument input_doc = QJsonDocument::fromJson(file.readAll(), &json_error);
        std::unordered_map<int64_t, TgBotUser> users_temp;

        if(json_error.error != QJsonParseError::NoError) {
            qWarning() << QString("Ошибка парсинга файла users.json: %1").arg(json_error.errorString());
            return false;
        }

        if(input_doc.isArray()) {
            for(int i = 0; i < input_doc.array().size(); ++i) {
                QJsonObject item_obj = input_doc.array().at(i).toObject();
                if((item_obj.contains("first_name") && item_obj.value("first_name").isString())
                    && (item_obj.contains("last_name") && item_obj.value("last_name").isString())
                    && (item_obj.contains("username") && item_obj.value("username").isString())
                    && (item_obj.contains("chat_id") && item_obj.value("chat_id").isDouble())
                    && (item_obj.contains("type") && item_obj.value("type").isString())
                    && (item_obj.contains("id") && item_obj.value("id").isDouble())) {

                    TgBotUser user_tmp;
                    user_tmp.chatId = static_cast<int64_t>(item_obj.value("chat_id").toInteger());
                    user_tmp.firstName = item_obj.value("first_name").toString().toStdString();
                    user_tmp.lastName = item_obj.value("last_name").toString().toStdString();
                    user_tmp.username = item_obj.value("username").toString().toStdString();
                    user_tmp.type = tg_user_type_from_qstring(item_obj.value("type").toString());
                    user_tmp.id = static_cast<int64_t>(item_obj.value("id").toInteger());

                    users_temp.emplace(static_cast<int64_t>(item_obj.value("id").toInteger()), user_tmp);
                } else {
                    qWarning() << "Файл настроек users.json поврежден!";
                    return false;
                }
            }
            users_.swap(users_temp);
            emit sg_send_message_to_console("Данные пользователей прочитаны.");
            qInfo() << "Данные пользователей прочитаны из users.json";
            return true;
        }
    }

    return false;
}