#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QAbstractTableModel>
#include <QDialog>
#include <QMainWindow>


QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class DeviceTreeViewModel;
class PlainTextConsole;
class QLineEdit;
class TgBotManager;
class DeviceManager;

class WhatsminerDeviceV3;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void sl_add_new_device_context_menu(const QPoint &pos);
    void sl_add_device_context_menu_action();
    void sl_delete_device_context_menu_action();
    void sl_set_password_context_menu_action();
    void sl_set_password_to_device(QString pswd);
    void sl_add_device_api2(const QString& addr);
    void sl_add_device_api3(const QString& addr);
    void sl_select_device_in_tree(const QModelIndex &current, const QModelIndex &previous);
    void sl_bot_status_changed();
    void sl_update_device_data();
    void sl_tb_refreshusers_clicked();
    void sl_tb_verifyuser_clicked();
    void sl_tb_setadmin_clicked();
    void sl_tb_resetadmin_clicked();
    void sl_tb_sendmessage_clicked();
    void sl_tb_banuser_clicked();
    void sl_tb_sendtoall_clicked();
    void sl_get_text_message_to_clients(QString mes);
    void sl_set_save_button_enabled();
    void sl_save_settings_to_bot();

private:
    Ui::MainWindow *ui;
    std::unique_ptr<DeviceManager> miner_manager_;
    std::unique_ptr<TgBotManager> tgbot_manager_;

    std::vector<size_t> temp_clients_;

    PlainTextConsole* console_;
    DeviceTreeViewModel* device_tree_model_;

    QString bot_token_;

    QModelIndex selected_item_;

    void save_settings_to_json_() const;
    void restore_settings_from_json_();
    void datatable_set_column_widths_();
    void users_table_update_();
    void show_message_input_dialog_();
};


class AddDeviceDialog: public QDialog {
    Q_OBJECT
public:
    explicit AddDeviceDialog(QWidget* parent = nullptr);

signals:
    void sg_add_new_device_V2(QString hostname);
    void sg_add_new_device_V3(QString hostname);

private slots:
    void sl_add2_pressed();
    void sl_add3_pressed();

private:
    QLineEdit* le_value_;

};

class SetPasswordDialog: public QDialog {
    Q_OBJECT
public:
    explicit SetPasswordDialog(QWidget* parent = nullptr);

signals:
    void sg_set_password(QString password);

private slots:
    void sl_set_pass_pressed();

private:
    QLineEdit* le_value_;

};
#endif // MAINWINDOW_H
