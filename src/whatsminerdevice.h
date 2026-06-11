#pragma once

#include <QJsonObject>
#include <QByteArray>
#include <QString>
#include <QMutex>
#include <QFuture>

struct WhatsminerData {
    QString Address;
    QString DeviceType;
    QString ChipType;
    QString WorkerName;
    QString FirmwareVersion;
    QString SerialNumber;

    bool    Online = false;
    bool    PoolOnline = false;

    double PSUVoltage = 0.;
    double PSUPower = 0.;
    double PSUTemperature = 0.0;

    bool    HasErrors = false;
    QStringList ErrorCodes;

    double  Power = 0.0;
    double  PowerLimit = 0.0;
    QString PowerMode;

    double  EnvironmentTemp = 0.0;
    double  LiquidTemp = 0.0;
    QString BoardTemps;
    double  ChipTempMin = 0.0;
    double  ChipTempMax = 0.0;
    double  ChipTempAvg = 0.0;

    double  THSAverage = 0.0;
    double  THS1m = 0.0;
    double  THS15m = 0.0;

    QString PoolUrl;
    double  PoolRejectRate = 0;
    QString PoolStatus;
};

class WhatsminerDevice
{
public:
    enum MinerConnectionStatus {
        NotReacheable,
        Reacheable,
        Error
    };

    WhatsminerDevice() = default;
    WhatsminerDevice(const QString& id);
    virtual ~WhatsminerDevice();

    void SetAddress(const QString &ip, int port = 4028);
    void SetPassword(const QString &password);
    bool CheckReachability();
    QString GetStringValue(const QString& type) const;
    std::unordered_set<QString> GetVariableNames() const;
    virtual bool ReadData() = 0;
    virtual void AsyncReadData() = 0;
    virtual QJsonObject SaveToJson() = 0;
    std::tuple<QString, quint16> GetConnectionData() const {return {ip_address_, port_};}
    MinerConnectionStatus ConnectionStatus() const { return connection_status_; }
    WhatsminerData Data();
    virtual QString Reboot() {return QString("Команда не поддерживается.");}
    virtual QString SetPowerLimit(int watts) {return QString("Команда не поддерживается.");}

protected:
    bool is_valid_json_(const QByteArray& raw) const;
    const int CONNECTION_TIMEOUT = 3000;
    QString id_;
    QString ip_address_;
    quint16 port_ = 4028;
    QString admin_password_  = QString("admin");
    MinerConnectionStatus connection_status_ = MinerConnectionStatus::NotReacheable;
    WhatsminerData data_;
    QMutex mutex_;
    QFuture<bool> future_read_;
    int device_offline_counter_ = 0;
};

class WhatsminerDeviceV2: public WhatsminerDevice
{
public:
    WhatsminerDeviceV2(): WhatsminerDevice(){}
    WhatsminerDeviceV2(const QString& id): WhatsminerDevice(id){}
    bool ReadData() override;
    void AsyncReadData() override;
    QJsonObject SaveToJson() override;

private:
    QJsonObject send_read_command_(const QString &cmd) const;
    QByteArray tcp_request_(const QByteArray &data) const;
    bool parse_summary_(const QJsonObject &json);
    bool parse_pools_(const QJsonObject &json);
    bool parse_error_code_(const QJsonObject &json);
    bool parse_miner_info_(const QJsonObject &json);
    bool parse_version_(const QJsonObject &json);
    bool parse_psu_info_(const QJsonObject &json);
    bool parse_edevs_info(const QJsonObject& json);
    bool full_update_();
};

class WhatsminerDeviceV3: public WhatsminerDevice
{
public:
    WhatsminerDeviceV3(): WhatsminerDevice() {port_ = 4433;}
    WhatsminerDeviceV3(const QString& id): WhatsminerDevice(id) {port_ = 4433;}

    bool ReadData() override;
    void AsyncReadData() override;
    QJsonObject SaveToJson() override;
    QString Reboot() override;
    QString SetPowerLimit(int watts) override;

private:
    QJsonObject send_read_command_(const QString &cmd) const;
    QByteArray  tcp_request_(const QByteArray &data) const;

    QString compute_token_(const QString &cmd, qint64 ts, const QString& salt) const;
    QJsonObject send_write_command_(const QJsonObject &cmd);

    bool parse_device_info_(const QJsonObject &json);
    bool parse_summary_(const QJsonObject &json);
    bool parse_pools_(const QJsonObject &json);
    bool full_update_();
};

namespace Whatsminer {
std::shared_ptr<WhatsminerDevice> MakeWhatsminerDevice(const QJsonObject& obj);
} // namespace