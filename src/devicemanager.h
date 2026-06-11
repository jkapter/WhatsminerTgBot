#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include <QDir>

#include "WhatsMinerDevice.h"

class QTimer;

class DeviceManager : public QObject
{
    Q_OBJECT
public:
    explicit DeviceManager(QObject *parent = nullptr);
    bool AddDevice(const QString& ip_address, int port = 4028, QString version = QString("V3"));
    bool DeleteDevice(const QString& ip_address);
    bool SetPasswordDevice(const QString& ip_address, const QString& pass);
    void StartPeriodReading(int period = 10000);
    void StopPeriodReading();
    std::vector<QString> GetDeviceAddresses() const;
    std::vector<QString> GetVariableNames(const QString& addr) const;
    QString RebootDevice(const QString& addr) const;
    QString SetPowerLimit(const QString& addr, int limit) const;
    WhatsminerData GetDeviceData(const QString& ip_address) const;
    QJsonArray SaveToJson() const;
    void RestoreDevicesFromJson(const QJsonArray& devices);
    QString GetStringValue(const QString& ip, const QString& type) const;
signals:
    void sg_device_data_updated();

private slots:
    void sl_periodic_timer_expired();

private:
    std::unordered_map<QString, std::shared_ptr<WhatsminerDevice>> devices_;
    int period_ = 10000;

    QTimer* period_timer_;
    bool is_reading_ = false;

    void restore_devices_from_json_();
};


#endif // DEVICEMANAGER_H
