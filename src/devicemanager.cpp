#include "devicemanager.h"

#include <QTimer>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonDocument>

using namespace Qt::StringLiterals;

DeviceManager::DeviceManager(QObject *parent)
    : QObject(parent)
    , period_timer_(new QTimer(this))
{
    QObject::connect(period_timer_, &QTimer::timeout, this, &DeviceManager::sl_periodic_timer_expired);
}

bool DeviceManager::AddDevice(const QString &ip_address, int port, QString version)
{
    if(devices_.contains(ip_address)) return false;
    bool timer_status = period_timer_->isActive();

    if(timer_status) {
        period_timer_->stop();
    }

    std::shared_ptr<WhatsminerDevice> p_dev;
    if(version == u"V2"_s) {
        p_dev.reset(new WhatsminerDeviceV2());
        p_dev->SetAddress(ip_address, port);
    }
    if(version == u"V3"_s) {
        p_dev.reset(new WhatsminerDeviceV3());
        p_dev->SetAddress(ip_address, port);
    }

    if(p_dev) {
        devices_[ip_address] = std::move(p_dev);
        devices_.at(ip_address)->ReadData();
    }

    if(timer_status) {
        period_timer_->start();
    }

    return devices_.contains(ip_address);
}

bool DeviceManager::DeleteDevice(const QString &ip_address)
{
    bool timer_status = period_timer_->isActive();
    bool res = false;

    if(timer_status) {
        period_timer_->stop();
    }

    if(devices_.contains(ip_address)) {
        devices_.erase(ip_address);
        res = true;
    }

    if(timer_status) {
        period_timer_->start();
    }

    return res;
}

bool DeviceManager::SetPasswordDevice(const QString &ip_address, const QString &pass)
{
    if(devices_.contains(ip_address)) {
        devices_.at(ip_address)->SetPassword(pass);
        return true;
    }
    return false;
}

void DeviceManager::StartPeriodReading(int period)
{
    period_ = period;
    period_timer_->start(period_);
}

void DeviceManager::StopPeriodReading()
{
    period_timer_->stop();
}

std::vector<QString> DeviceManager::GetDeviceAddresses() const
{
    auto keys_view = devices_ | std::views::keys;
    return {keys_view.begin(), keys_view.end()};
}

std::vector<QString> DeviceManager::GetVariableNames(const QString &addr) const
{
    if(!devices_.contains(addr)) return {};
    auto vars = devices_.at(addr)->GetVariableNames();
    return {vars.begin(), vars.end()};
}

QString DeviceManager::RebootDevice(const QString &addr) const
{
    if(!devices_.contains(addr)) return u"Устройтсво не найдено"_s;
    return devices_.at(addr)->Reboot();
}

QString DeviceManager::SetPowerLimit(const QString &addr, int limit) const
{
    if(!devices_.contains(addr)) return u"Устройтсво не найдено"_s;
    return devices_.at(addr)->SetPowerLimit(limit);
}

WhatsminerData DeviceManager::GetDeviceData(const QString &ip_address) const
{
    if(!devices_.contains(ip_address)) return {};
    return devices_.at(ip_address)->Data();
}

QJsonArray DeviceManager::SaveToJson() const
{
    QJsonArray ret_ar;
    for(const auto& [addr, dev_ptr]: devices_) {
        ret_ar.append(dev_ptr->SaveToJson());
    }
    return ret_ar;
}

void DeviceManager::RestoreDevicesFromJson(const QJsonArray &devices)
{
    for(int i = 0; i < devices.size(); ++i) {
        QJsonObject obj = devices.at(i).toObject();
        if(!obj.contains("address")
            || !obj.value("address").isString()
            || !obj.contains("version")
            || !obj.value("version").isString()) {
            continue;
        }

        if(devices_.contains(obj.value("address").toString())) continue;

        auto p_dev = Whatsminer::MakeWhatsminerDevice(obj);
        if(p_dev) {
            devices_[obj.value("address").toString()] = p_dev;
        }
    }
}

QString DeviceManager::GetStringValue(const QString &ip, const QString &type) const
{
    if(devices_.contains(ip)) {
        return devices_.at(ip)->GetStringValue(type);
    }
    return u"UNKNOWN"_s;
}

void DeviceManager::sl_periodic_timer_expired()
{
    if(is_reading_) return;
    is_reading_ = true;
    for(auto& [ip, dev_ptr]: devices_) {
        dev_ptr->AsyncReadData();
    }
    is_reading_ = false;
    emit sg_device_data_updated();
}