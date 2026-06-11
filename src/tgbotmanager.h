#ifndef TGBOTMANAGER_H
#define TGBOTMANAGER_H

#include <QDateTime>
#include <QObject>
#include <QMessageBox>

#include "tgbot/types/User.h"
#include "tgbot/types/Message.h"
#include "tgbot/types/CallbackQuery.h"
#include "tgbot/types/InlineKeyboardMarkup.h"
#include "tgbot/types/InlineKeyboardButton.h"
#include "tgbot/net/CurlHttpClient.h"
#include "tgbot/types/BotCommand.h"

#include "tgbot/Bot.h"
#include "devicemanager.h"

class QTimer;
class TGScheduledEvent;
class TGButtonWCallback;
class QMessageBox;

enum class USER_TYPE: uint8_t {
    UNDEFINED       = 0,
    NEW_USER        = 1,
    UNREGISTERED    = 2,
    REGISTRD        = 3,
    CHANNEL         = 4,
    ADMIN           = 5,
    BANNED          = 6
};

QString tg_user_type_to_qstring(USER_TYPE type);
USER_TYPE tg_user_type_from_qstring(QString type);

class InterruptibleHttpClient: public TgBot::CurlHttpClient
{
public:
    void RequestInterrupt() {interrupted_.store(true);}
    void ResetInterrupt() {interrupted_.store(false);}

    std::string makeRequest(const TgBot::Url& url, const std::vector<TgBot::HttpReqArg>& args) const override
    {
        if(interrupted_.load()) {
            throw std::runtime_error("Http client: bot request interrupted.");
        }
        return TgBot::CurlHttpClient::makeRequest(url, args);
    }

private:
    std::atomic<bool> interrupted_{false};
};

class TGParent {
public:
    explicit TGParent();
    TgBot::Bot* Bot();
    void InterruptBotHttpClient();
    void AddOrUpdateChatID(int64_t user, USER_TYPE type);
    void DeleteChatID(int64_t user);
    void ClearChatIdData();
    void InitializeBot(const std::string& token);
    std::vector<int64_t> GetChatIDs(USER_TYPE min_auth_level = USER_TYPE::UNDEFINED) const;
    bool CheckUserChatId(int64_t chat_id, USER_TYPE min_auth_level) const;
    TgBot::Message::Ptr BotSendMessage(int64_t chat_id, const std::string& text, TgBot::InlineKeyboardMarkup::Ptr inline_buttons = nullptr);
    void BotDeleteMessage(TgBot::Message::Ptr mes);
    const std::unordered_set<int64_t>& GetInactiveUsers() const;
    void SetBotNameForChannel(QString name);

private:
    std::unique_ptr<TgBot::Bot> bot_ptr_;
    std::unique_ptr<InterruptibleHttpClient> http_client_;
    std::unordered_map<USER_TYPE, std::unordered_set<int64_t>> user_permission_to_chat_id_;
    std::unordered_set<int64_t> inactive_users_;
    std::optional<QString> bot_name_for_channel_;
};

class TgBotUser: public TgBot::User {
public:
    TgBotUser() = default;
    TgBotUser(TgBot::User& other);
    TgBotUser(TgBot::Message::Ptr message);
    int64_t chatId = 0;
    QDateTime lastMessageDT = QDateTime();
    USER_TYPE type = USER_TYPE::NEW_USER;
    bool userActive = true;
    void UpdateData(TgBot::User& other);
    void UpdateData(TgBot::Message::Ptr messag);

private:
    void copy_fields_(TgBot::User other);
    void copy_fields_(TgBot::Chat chat_ptr);
};

class TgBotManager: public QObject
{
    Q_OBJECT
public:
    explicit TgBotManager(const std::string& bot_token, DeviceManager& device_manager);
    virtual ~TgBotManager();

    bool BotIsWorking() const;
    void StartBot();
    void StopBot();
    bool IsAutoRestart() const;
    void SetAutoRestartBot(bool b);
    const std::string& GetToken() const;

    void AddOrUpdateUser(TgBot::Message::Ptr message);
    bool RegisterUser(int64_t user_id);
    bool UnRegisterUser(int64_t user_id);
    bool SetAdmin(int64_t user_id);
    bool ResetAdmin(int64_t user_id);
    bool BanUser(int64_t user_id);
    USER_TYPE CheckUser(int64_t user_id) const;
    std::vector<TgBotUser const *> GetUsers(USER_TYPE type = USER_TYPE::UNDEFINED);
    void SendMessageToUsers(const std::vector<size_t>& id, QString message) const;
    void SaveUserData();
    void SetBotNameForChannels(QString name);
    QString GetBotNameForChannels() const;

    TGParent *GetTGParent() const;
signals:
    void sg_send_message_to_console(QString mes);
    void sg_bot_thread_state_changed();
    void sg_stop_bot_thread();

private slots:
    void sl_bot_finished();
    void sl_bot_started();
    void sl_bot_throw_exception(QString what);
    void sl_check_events_timer_out();
    void sl_check_restart_timer_out();
    void sl_try_to_connect_api_successful();

private:
    std::string bot_token_;

    bool auto_restart_bot_ = false;
    bool bot_started_ = false;
    bool bot_trying_connect_api_ = false;

    DeviceManager* device_manager_;
    std::unique_ptr<TGParent> tg_parent_ = nullptr;

    QTimer* check_events_timer_ = nullptr;
    QTimer* check_restart_timer_ = nullptr;
    std::unique_ptr<QMessageBox> error_message_ = nullptr;

    std::optional<QString> bot_name_for_channels_;

    int restart_counter_ = 0;

    const int CHECK_EVENTS_PERIOD_ = 20000;
    const int RESTART_BOT_PERIOD_ = 10000;
    const int TIME_TO_RESTART_APP_ON_COMM_FAIL_ = 150;
    const std::string screened_symbols_= "_[]()~#+-=|{}.!`";

    std::unordered_map<int64_t, TgBotUser> users_;

    std::unordered_map<std::string, std::function<void(const TgBot::CallbackQuery::Ptr)>> id_to_callback_;
    std::unordered_map<std::string, std::function<void(const TgBot::CallbackQuery::Ptr)>> admin_tools_to_callback_;
    std::unordered_map<std::string, std::function<bool(QString addr)>> triggers_for_check_;
    std::unordered_map<std::string, std::unordered_map<QString,bool>> triggers_previous_check_;
    std::unordered_map<std::string, std::function<void(QString addr)>> triggers_actions_;
    std::unordered_map<std::int64_t, TgBot::Message::Ptr> chat_to_messages_to_delete_;
    std::unordered_map<std::int64_t, std::pair<QString,TgBot::Message::Ptr>> chat_to_messages_wait_limit_;

    bool opc_comm_status_previous_scan_ = false;
    int opc_comm_failed_time_ = 0;

    void process_callbacks_(const TgBot::CallbackQuery::Ptr cb_query);
    void make_admin_tools_();
    void make_users_processing_();
    void make_commands_processing_();
    void make_callback_data_();
    void make_triggers_data_();
    void make_new_bot_();
    void initialize_bot_();
    bool save_user_data_to_file_(const QString& filename) const;
    bool save_tg_data_to_file_(const QString& filename) const;
    bool restore_user_data_from_file_();
    bool restore_tg_data_from_file_();
    void screen_symbols_(std::string& text, const std::string& symbols) const;
};

#endif // TGBOTMANAGER_H
