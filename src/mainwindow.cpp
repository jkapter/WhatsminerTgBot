#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "plaintextconsole.h"
#include "treetablemodels.h"
#include "tgbotmanager.h"
#include "devicemanager.h"
#include "hintinputdialog.h"

#include <QDir>
#include <QJsonObject>
#include <QJsonArray>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>

using namespace Qt::StringLiterals;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , miner_manager_(new DeviceManager())
{
    ui->setupUi(this);

    QFile style_file(":/styles/styles.qss");
    if(style_file.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream style_stream(&style_file);
        qApp->setStyleSheet(style_stream.readAll());
    } else {
        qWarning() << QString("Не удается открыть файл стилей.");
    }

    console_ = new PlainTextConsole(this);
    console_->setMaximumBlockCount(100);
    console_->AddDTToMessage(true);
    ui->frConsoleTG->setLayout(new QVBoxLayout());
    ui->frConsoleTG->layout()->addWidget(console_);

    QObject::connect(ui->tbRefreshUsers, &QToolButton::clicked, this, &MainWindow::sl_tb_refreshusers_clicked);
    QObject::connect(ui->tbVerifyUser, &QToolButton::clicked, this, &MainWindow::sl_tb_verifyuser_clicked);
    QObject::connect(ui->tbSetAdmin, &QToolButton::clicked, this, &MainWindow::sl_tb_setadmin_clicked);
    QObject::connect(ui->tbResetAdmin, &QToolButton::clicked, this, &MainWindow::sl_tb_resetadmin_clicked);
    QObject::connect(ui->tbSendMessage, &QToolButton::clicked, this, &MainWindow::sl_tb_sendmessage_clicked);
    QObject::connect(ui->tbBanUser, &QToolButton::clicked, this, &MainWindow::sl_tb_banuser_clicked);

    restore_settings_from_json_();

    tgbot_manager_.reset(new TgBotManager(bot_token_.toStdString(), *miner_manager_.get()));
    ui->leBotToken->setText(bot_token_);
    tgbot_manager_->SetAutoRestartBot(true);
    QObject::connect(tgbot_manager_.get(), &TgBotManager::sg_send_message_to_console, console_, &PlainTextConsole::sl_add_text_to_console);
    QObject::connect(tgbot_manager_.get(), &TgBotManager::sg_bot_thread_state_changed, this, &MainWindow::sl_bot_status_changed);
    QObject::connect(ui->pbRestartBot, &QPushButton::clicked, this, [this](){tgbot_manager_->StopBot();});

    QObject::connect(miner_manager_.get(), &DeviceManager::sg_device_data_updated, this, &MainWindow::sl_update_device_data);

    device_tree_model_ = new DeviceTreeViewModel(*miner_manager_.get(), this);
    ui->trvDevices->setModel(device_tree_model_);

    QItemSelectionModel *selection = ui->trvDevices->selectionModel();
    QObject::connect(selection, &QItemSelectionModel::currentChanged, this, &MainWindow::sl_select_device_in_tree);

    ui->trvDevices->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->trvDevices->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->trvDevices->setHeaderHidden(true);
    QObject::connect(ui->trvDevices, &QWidget::customContextMenuRequested, this, &MainWindow::sl_add_new_device_context_menu);
    ui->trvDevices->setSortingEnabled(false);

    ui->tbvDeviceData->setSelectionMode(QAbstractItemView::NoSelection);

    if(!miner_manager_->GetDeviceAddresses().empty()) {
        miner_manager_->StartPeriodReading();
    }

    tgbot_manager_->StartBot();
}

MainWindow::~MainWindow()
{
    save_settings_to_json_();
    delete ui;
    ui = nullptr;
}

void MainWindow::sl_add_new_device_context_menu(const QPoint &pos)
{
    selected_item_ = ui->trvDevices->indexAt(pos);
    QAction *add_device = new QAction(u"Добавить прибор"_s, ui->trvDevices);
    QAction *delete_device = new QAction(u"Удалить"_s, ui->trvDevices);
    QAction *set_password = new QAction(u"Задать пароль"_s, ui->trvDevices);
    delete_device->setEnabled(selected_item_.isValid());
    set_password->setEnabled(selected_item_.isValid());

    QObject::connect(add_device, &QAction::triggered, this, &MainWindow::sl_add_device_context_menu_action);
    QObject::connect(delete_device, &QAction::triggered, this, &MainWindow::sl_delete_device_context_menu_action);
    QObject::connect(set_password, &QAction::triggered, this, &MainWindow::sl_set_password_context_menu_action);

    QMenu context_menu(ui->trvDevices);
    context_menu.addAction(add_device);
    context_menu.addSeparator();
    context_menu.addAction(delete_device);
    context_menu.addSeparator();
    context_menu.addAction(set_password);
    context_menu.exec(ui->trvDevices->mapToGlobal(pos));
}

void MainWindow::sl_add_device_context_menu_action()
{
    AddDeviceDialog* new_host_dialog = new AddDeviceDialog(this);
    QObject::connect(new_host_dialog, &AddDeviceDialog::sg_add_new_device_V2, this, &MainWindow::sl_add_device_api2);
    QObject::connect(new_host_dialog, &AddDeviceDialog::sg_add_new_device_V3, this, &MainWindow::sl_add_device_api3);
    new_host_dialog->exec();
    new_host_dialog->deleteLater();
}

void MainWindow::sl_delete_device_context_menu_action()
{
    if(selected_item_.isValid()) {
        miner_manager_->StopPeriodReading();
        QString dev_addr = static_cast<const DeviceTreeItem*>(selected_item_.internalPointer())->GetId();
        miner_manager_->DeleteDevice(dev_addr);
        device_tree_model_->update();
    }
    if(!miner_manager_->GetDeviceAddresses().empty()) {
        miner_manager_->StartPeriodReading();
    }
}

void MainWindow::sl_set_password_to_device(QString pswd)
{
    if(selected_item_.isValid()) {
        QString dev_addr = static_cast<const DeviceTreeItem*>(selected_item_.internalPointer())->GetId();
        miner_manager_->SetPasswordDevice(dev_addr, pswd);
    }
}

void MainWindow::sl_set_password_context_menu_action()
{
    SetPasswordDialog* set_pswd_dialog = new SetPasswordDialog(this);
    QObject::connect(set_pswd_dialog, &SetPasswordDialog::sg_set_password, this, &MainWindow::sl_set_password_to_device);
    set_pswd_dialog->exec();
    set_pswd_dialog->deleteLater();
}

void MainWindow::sl_add_device_api2(const QString &addr)
{
    tgbot_manager_->SetAutoRestartBot(false);
    tgbot_manager_->StopBot();

    miner_manager_->AddDevice(addr, 4028, u"V2"_s);
    device_tree_model_->update();

    if(!miner_manager_->GetDeviceAddresses().empty()) {
        miner_manager_->StartPeriodReading();
    }

    ui->trvDevices->resizeColumnToContents(0);

    tgbot_manager_->SetAutoRestartBot(true);
    tgbot_manager_->StartBot();
}

void MainWindow::sl_add_device_api3(const QString &addr)
{
    tgbot_manager_->SetAutoRestartBot(false);
    tgbot_manager_->StopBot();

    miner_manager_->AddDevice(addr, 4433, u"V3"_s);
    device_tree_model_->update();
    if(!miner_manager_->GetDeviceAddresses().empty()) {
        miner_manager_->StartPeriodReading();
    }
    ui->trvDevices->resizeColumnToContents(0);

    tgbot_manager_->SetAutoRestartBot(true);
    tgbot_manager_->StartBot();
}

void MainWindow::sl_select_device_in_tree(const QModelIndex &current, const QModelIndex &previous)
{
    if(!current.isValid()) return;
    QString dev_addr = static_cast<const DeviceTreeItem*>(current.internalPointer())->GetId();
    ui->tbvDeviceData->setModel(new DeviceDataViewerModel(dev_addr, *miner_manager_.get(), ui->tbvDeviceData));
    ui->tbvDeviceData->setWordWrap(true);
    ui->tbvDeviceData->setTextElideMode(Qt::ElideNone);
    ui->tbvDeviceData->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    datatable_set_column_widths_();
}

void MainWindow::save_settings_to_json_() const
{
    QString file_name = QString("%1/settings.json").arg(QDir().path());
    QFile file(file_name);
    if(file.open(QIODevice::WriteOnly)) {
        QJsonObject main_obj;

        main_obj.insert("devices", miner_manager_->SaveToJson());
        main_obj.insert("token", ui->leBotToken->text());

        QJsonDocument json_doc(main_obj);
        if(file.write(json_doc.toJson()) == -1) {
            qWarning() << QString("MainWindow: не удалось сохраненить данные в файл %1.").arg(file_name);
        }

        file.close();
        qInfo() << QString("MainWindow: данные сохранены в файл %1.").arg(file_name);

    }   else {
        qWarning() << QString("MainWindow: не удалось открыть файл %1 для записи.").arg(file_name);
    }
}

void MainWindow::restore_settings_from_json_()
{
    QString file_name = QString("%1/settings.json").arg(QDir().path());
    QFile file(file_name);

    if(file.open(QIODeviceBase::ReadOnly)) {
        QJsonParseError json_error;
        QJsonDocument input_doc = QJsonDocument::fromJson(file.readAll(), &json_error);

        if(json_error.error != QJsonParseError::NoError || !input_doc.isObject()) {
            QString log_message = QString("MainWindow: файл %1 поврежден.").arg(file_name);
            qWarning() << log_message;
            return;
        }

        QJsonObject main_obj = input_doc.object();

        if(main_obj.contains("devices") && main_obj.value("devices").isArray()) {
            miner_manager_->RestoreDevicesFromJson(main_obj.value("devices").toArray());
        } else {
            qWarning() << QString("MainWindow: не удалось восстановить устройства из файла настроек.");
        }

        if(main_obj.contains("token") && main_obj.value("token").isString()) {
            bot_token_ = main_obj.value("token").toString();
        } else {
            qWarning() << QString("MainWindow: не найден токен телеграм бота в файле настроек.");
        }
    } else {
        qWarning() << u"DeviceManager: невозможно открыть файл %1 для чтения"_s.arg(file_name);
    }
}

void MainWindow::datatable_set_column_widths_()
{
    QTableView* data_table = ui->tbvDeviceData;

    int w_header = data_table->horizontalHeader()->geometry().width() - 20;
    if(w_header <= 0) return;

    int w_header_col =w_header / 3;
    data_table->setColumnWidth(0, w_header_col);
    data_table->setColumnWidth(1, w_header - w_header_col);

    data_table->repaint();
}

void MainWindow::users_table_update_()
{
    QTableWidget* tbl = ui->tblUsers;
    size_t w = tbl->width() - 25;
    for(int i = 0; i < 6; ++i) {
        tbl->setColumnWidth(i, 100);
    }
    tbl->setColumnWidth(1, 80);
    if(w < 700) {
        tbl->setColumnWidth(6, 100);
    } else {
        tbl->setColumnWidth(6, w - 580);
    }
    auto users_data = tgbot_manager_->GetUsers();
    tbl->clearContents();
    tbl->setRowCount(users_data.size());
    size_t r = 0;
    for(const auto& user: users_data) {
        QString user_type;
        switch(user->type) {
        case USER_TYPE::ADMIN       : user_type = QString("Администратор"); break;
        case USER_TYPE::REGISTRD    : user_type = QString("Зарегистрированный"); break;
        case USER_TYPE::BANNED      : user_type = QString("Забанен"); break;
        case USER_TYPE::CHANNEL     : user_type = QString("Канал"); break;
        default                     : user_type = QString("");
        }
        tbl->setItem(r, 0, new QTableWidgetItem(QString::number(user->id)));
        QWidget* chb_item = new QWidget(tbl);
        QHBoxLayout* hbox_la = new QHBoxLayout(chb_item);
        QCheckBox* cb_active = new QCheckBox(chb_item);
        hbox_la->addWidget(cb_active);
        hbox_la->setAlignment(Qt::AlignCenter);
        hbox_la->setContentsMargins(0, 0, 0, 0);
        chb_item->setLayout(hbox_la);
        cb_active->setChecked(user->userActive);
        cb_active->setEnabled(false);
        tbl->setCellWidget(r, 1, chb_item);
        tbl->setItem(r, 2, new QTableWidgetItem(QString::fromStdString(user->username)));
        tbl->setItem(r, 3, new QTableWidgetItem(QString::fromStdString(user->firstName)));
        tbl->setItem(r, 4, new QTableWidgetItem(QString::fromStdString(user->lastName)));
        tbl->setItem(r, 5, new QTableWidgetItem(user_type));
        tbl->setItem(r, 6, new QTableWidgetItem(user->lastMessageDT.toString("dd.MM.yyyy hh:mm:ss")));
        ++r;
    }
}

void MainWindow::sl_bot_status_changed() {
    if(!ui) return;
    if(tgbot_manager_->BotIsWorking()) {
        ui->lbBotStatus->setText("Бот запущен");
        ui->lbBotStatus->setStyleSheet(QString("color: white; background-color: darkgreen; border: 2px solid #519999; border-radius: 6px; padding: 5px;"));
    } else {
        ui->lbBotStatus->setText("Бот остановлен");
        ui->lbBotStatus->setStyleSheet(QString("color: white; background-color: darkred; border: 2px solid #519999; border-radius: 6px; padding: 5px;"));
    }
}

void MainWindow::sl_update_device_data()
{
    if(!ui) return;
    auto trv_model = ui->trvDevices->model();
    QModelIndex topleft = trv_model->index(0, 0);
    QModelIndex btmright = trv_model->index(trv_model->rowCount() - 1, trv_model->columnCount() - 1);
    ui->trvDevices->dataChanged(topleft, btmright);
}

void MainWindow::sl_tb_refreshusers_clicked()
{
    users_table_update_();
}

void MainWindow::sl_tb_verifyuser_clicked()
{
    auto sel_ranges = ui->tblUsers->selectedRanges();
    if(sel_ranges.isEmpty()) {
        QMessageBox::information(this, "Информация", "Пользователь не выбран");
        return;
    }

    size_t id;
    bool b;
    id = ui->tblUsers->item(sel_ranges.at(0).topRow(), 0)->text().toLongLong(&b);

    if(!b) {
        QMessageBox::information(this, "Информация", "ID пользователя неверный");
        return;
    }

    if(tgbot_manager_->RegisterUser(id)) {
        users_table_update_();
        tgbot_manager_->SaveUserData();
    } else {
        QMessageBox::information(this, "Информация", "Пользователь не найден");
    }
}

void MainWindow::sl_tb_setadmin_clicked()
{
    auto sel_ranges = ui->tblUsers->selectedRanges();
    if(sel_ranges.isEmpty()) {
        QMessageBox::information(this, "Информация", "Пользователь не выбран");
        return;
    }

    size_t id;
    bool b;
    id = ui->tblUsers->item(sel_ranges.at(0).topRow(), 0)->text().toLongLong(&b);

    if(!b) {
        QMessageBox::information(this, "Информация", "ID пользователя неверный");
        return;
    }

    if(tgbot_manager_->SetAdmin(id)) {
        users_table_update_();
        tgbot_manager_->SaveUserData();
    } else {
        QMessageBox::information(this, "Информация", "Пользователь не найден");
    }
}

void MainWindow::sl_tb_resetadmin_clicked()
{
    auto sel_ranges = ui->tblUsers->selectedRanges();
    if(sel_ranges.isEmpty()) {
        QMessageBox::information(this, "Информация", "Пользователь не выбран");
        return;
    }

    size_t id;
    bool b;
    id = ui->tblUsers->item(sel_ranges.at(0).topRow(), 0)->text().toLongLong(&b);

    if(!b) {
        QMessageBox::information(this, "Информация", "ID пользователя неверный");
        return;
    }

    if(tgbot_manager_->ResetAdmin(id)) {
        users_table_update_();
        tgbot_manager_->SaveUserData();
    } else {
        QMessageBox::information(this, "Информация", "Пользователь не найден");
    }
}

void MainWindow::sl_tb_sendmessage_clicked()
{
    auto sel_ranges = ui->tblUsers->selectedRanges();
    if(sel_ranges.isEmpty()) {
        QMessageBox::information(this, "Информация", "Пользователь не выбран");
        return;
    }

    size_t id;
    bool b;
    id = ui->tblUsers->item(sel_ranges.at(0).topRow(), 0)->text().toLongLong(&b);

    if(!b) {
        QMessageBox::information(this, "Информация", "ID пользователя неверный");
        return;
    }

    temp_clients_.clear();
    temp_clients_.push_back(id);

    show_message_input_dialog_();
}

void MainWindow::sl_tb_banuser_clicked()
{
    auto sel_ranges = ui->tblUsers->selectedRanges();
    if(sel_ranges.isEmpty()) {
        QMessageBox::information(this, "Информация", "Пользователь не выбран");
        return;
    }

    size_t id;
    bool b;
    id = ui->tblUsers->item(sel_ranges.at(0).topRow(), 0)->text().toLongLong(&b);

    if(!b) {
        QMessageBox::information(this, "Информация", "ID пользователя неверный");
        return;
    }

    if(tgbot_manager_->BanUser(id)) {
        users_table_update_();
        tgbot_manager_->SaveUserData();
    } else {
        QMessageBox::information(this, "Информация", "Пользователь не найден");
    }
}

void MainWindow::sl_tb_sendtoall_clicked()
{
    temp_clients_.clear();
    for(const auto& it: tgbot_manager_->GetUsers()) {
        if(it->type != USER_TYPE::BANNED) {
            temp_clients_.push_back(it->id);
        }
    }
    show_message_input_dialog_();
}

void MainWindow::sl_get_text_message_to_clients(QString mes)
{
    tgbot_manager_->SendMessageToUsers(temp_clients_, mes);
}

void MainWindow::show_message_input_dialog_() {
    QString help_text = "*bold text*\n_italic text_\n__underline__\n~strikethrough~\n||spoiler||\n[inline URL](http://www.example.com/)\n[inline mention of a user](tg://user?id=123456789)\n![👍](tg://emoji?id=5368324170671202286)\n";
    HintInputDialog* input_dialog = new HintInputDialog(help_text, this);
    input_dialog->setWindowFlag(Qt::MSWindowsFixedSizeDialogHint);
    input_dialog->setWindowModality(Qt::WindowModality::WindowModal);
    input_dialog->setWindowTitle(tr("Отправить сообщение пользователю"));
    input_dialog->setGeometry(this->pos().rx() + 200, this->pos().ry() + 100, 400, 500);
    QObject::connect(input_dialog, &HintInputDialog::sg_applied, this, &MainWindow::sl_get_text_message_to_clients);
    input_dialog->show();
}

void MainWindow::showEvent(QShowEvent *event)
{
    datatable_set_column_widths_();
    users_table_update_();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    datatable_set_column_widths_();
    users_table_update_();
}

//===============================================================
//================ AddDeviceDialog ==============================
//===============================================================

AddDeviceDialog::AddDeviceDialog(QWidget *parent)
    : QDialog(parent, Qt::Dialog)
{
    setWindowTitle("Добавить новое устройство");

    QVBoxLayout* vbla = new QVBoxLayout();
    le_value_ = new QLineEdit();
    le_value_->setAlignment(Qt::AlignCenter);

    QPushButton* add2_btn = new QPushButton("Добавить API V2");
    QObject::connect(add2_btn, &QAbstractButton::pressed, this, &AddDeviceDialog::sl_add2_pressed);

    QPushButton* add3_btn = new QPushButton("Добавить API V3");
    QObject::connect(add3_btn, &QAbstractButton::pressed, this, &AddDeviceDialog::sl_add3_pressed);

    QPushButton* cancel_btn = new QPushButton("Отмена");
    QObject::connect(cancel_btn, &QAbstractButton::pressed, this, &QWidget::close);

    QHBoxLayout* hbla = new QHBoxLayout();
    hbla->addWidget(add3_btn);
    hbla->addWidget(add2_btn);
    hbla->addWidget(cancel_btn);
    vbla->addWidget(le_value_);
    vbla->addItem(hbla);

    setLayout(vbla);
}

void AddDeviceDialog::sl_add2_pressed()
{
    if(le_value_->text().length() > 0) {
        emit sg_add_new_device_V2(le_value_->text());
    }
    close();
}

void AddDeviceDialog::sl_add3_pressed()
{
    if(le_value_->text().length() > 0) {
        emit sg_add_new_device_V3(le_value_->text());
    }
    close();
}

//===============================================================
//================ AddDeviceDialog ==============================
//===============================================================

SetPasswordDialog::SetPasswordDialog(QWidget *parent)
    : QDialog(parent, Qt::Dialog)
{
    setWindowTitle("Задать пароль для устройства");

    QVBoxLayout* vbla = new QVBoxLayout();
    le_value_ = new QLineEdit();
    le_value_->setAlignment(Qt::AlignCenter);

    QPushButton* set_pwd = new QPushButton("Установить пароль");
    QObject::connect(set_pwd, &QAbstractButton::pressed, this, &SetPasswordDialog::sl_set_pass_pressed);

    QPushButton* cancel_btn = new QPushButton("Отмена");
    QObject::connect(cancel_btn, &QAbstractButton::pressed, this, &QWidget::close);

    QHBoxLayout* hbla = new QHBoxLayout();
    hbla->addWidget(set_pwd);
    hbla->addWidget(cancel_btn);
    vbla->addWidget(le_value_);
    vbla->addItem(hbla);

    setLayout(vbla);
}

void SetPasswordDialog::sl_set_pass_pressed()
{
    if(le_value_->text().length() > 0) {
        emit sg_set_password(le_value_->text());
    }
    close();
}