#include "WhatsminerDevice.h"

#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>
#include <QNetworkProxy>
#include <QJSonObject>
#include <QElapsedTimer>
#include <QMutexLocker>
#include <QCryptographicHash>
#include <QtConcurrent/QtConcurrentRun>

using namespace Qt::StringLiterals;

//==========================================================
//=============== WhatsminerDevice =========================
//==========================================================

WhatsminerDevice::WhatsminerDevice(const QString& id)
    : id_(id)
{}

WhatsminerDevice::~WhatsminerDevice()
{
    if(future_read_.isRunning()) {
        future_read_.waitForFinished();
    }
    if(mutex_.try_lock_for(std::chrono::seconds(1))) {
        mutex_.unlock();
    }
}

void WhatsminerDevice::SetAddress(const QString &ip, int port)
{
    ip_address_ = ip;
    data_.Address = ip;
    port_ = port;
}

void WhatsminerDevice::SetPassword(const QString &password)
{
    admin_password_ = password;
}

bool WhatsminerDevice::CheckReachability()
{
    QTcpSocket sock;
    sock.setProxy(QNetworkProxy::NoProxy);
    sock.connectToHost(ip_address_, port_, QIODeviceBase::ReadWrite, QAbstractSocket::IPv4Protocol);
    bool ok = sock.waitForConnected(CONNECTION_TIMEOUT);
    if(!ok) {
        qWarning() << QString("WhatsminerDevice: [%1] ошибка при попытке подключения = %2").arg(ip_address_, sock.errorString());
    }
    sock.abort();
    connection_status_ = ok ? Reacheable : NotReacheable;
    return ok;
}

QString WhatsminerDevice::GetStringValue(const QString &type) const
{
    if (type == u"Address"_s)            return data_.Address;
    if (type == u"DeviceType"_s)         return data_.DeviceType;
    if (type == u"ChipType"_s)           return data_.ChipType;
    if (type == u"WorkerName"_s)         return data_.WorkerName;
    if (type == u"FirmwareVersion"_s)    return data_.FirmwareVersion;
    if (type == u"SerialNumber"_s)       return data_.SerialNumber;
    if (type == u"Online"_s)             return data_.Online ? u"ДА"_s : u"НЕТ"_s;
    if (type == u"PoolOnline"_s)         return data_.PoolOnline ? u"ДА"_s : u"НЕТ"_s;
    if (type == u"PSUVoltage"_s)         return QString::number(data_.PSUVoltage, 'c', 2);
    if (type == u"PSUPower"_s)           return QString::number(data_.PSUPower, 'c', 2);
    if (type == u"PSUTemperature"_s)     return QString::number(data_.PSUTemperature, 'c', 2);
    if (type == u"HasErrors"_s)          return data_.HasErrors ? u"ДА"_s : u"НЕТ"_s;
    if (type == u"ErrorCodes"_s)         {
        QString ret_str;
        for(const auto& it: data_.ErrorCodes) {
            ret_str.append(it).append(u"\n"_s);
        }
        return ret_str;
    }
    if (type == u"Power"_s)              return QString::number(data_.Power, 'c', 2);
    if (type == u"PowerLimit"_s)         return QString::number(data_.PowerLimit, 'c', 2);
    if (type == u"PowerMode"_s)          return data_.PowerMode;
    if (type == u"EnvironmentTemp"_s)    return QString::number(data_.EnvironmentTemp, 'c', 2);
    if (type == u"LiquidTemp"_s)         return QString::number(data_.LiquidTemp, 'c', 2);
    if (type == u"BoardTemps"_s)         return data_.BoardTemps;
    if (type == u"ChipTempMin"_s)        return QString::number(data_.ChipTempMin, 'c', 2);
    if (type == u"ChipTempMax"_s)        return QString::number(data_.ChipTempMax, 'c', 2);
    if (type == u"ChipTempAvg"_s)        return QString::number(data_.ChipTempAvg, 'c', 2);
    if (type == u"THSAverage"_s)         return QString::number(data_.THSAverage, 'c', 2);
    if (type == u"THS1m"_s)              return QString::number(data_.THS1m, 'c', 2);
    if (type == u"THS15m"_s)             return QString::number(data_.THS15m, 'c', 2);
    if (type == u"PoolUrl"_s)            return data_.PoolUrl;
    if (type == u"PoolRejectRate"_s)     return QString::number(data_.PoolRejectRate, 'c', 3);
    if (type == u"PoolStatus"_s)         return data_.PoolStatus;

    return u"UNKNOWN"_s;
}

std::unordered_set<QString> WhatsminerDevice::GetVariableNames() const
{
    return {
        u"Address"_s,
        u"DeviceType"_s,
        u"ChipType"_s,
        u"WorkerName"_s,
        u"FirmwareVersion"_s,
        u"SerialNumber"_s,
        u"Online"_s,
        u"PoolOnline"_s,
        u"PSUVoltage"_s,
        u"PSUPower"_s,
        u"PSUTemperature"_s,
        u"HasErrors"_s,
        u"ErrorCodes"_s,
        u"Power"_s,
        u"PowerLimit"_s,
        u"PowerMode"_s,
        u"EnvironmentTemp"_s,
        u"LiquidTemp"_s,
        u"BoardTemps"_s,
        u"ChipTempMin"_s,
        u"ChipTempMax"_s,
        u"ChipTempAvg"_s,
        u"THSAverage"_s,
        u"THS1m"_s,
        u"THS15m"_s,
        u"PoolUrl"_s,
        u"PoolRejectRate"_s,
        u"PoolStatus"_s
    };
}

WhatsminerData WhatsminerDevice::Data()
{
    QMutexLocker locker(&mutex_);
    return data_;
}

bool WhatsminerDevice::is_valid_json_(const QByteArray &raw) const
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(raw, &error);

    if (error.error != QJsonParseError::NoError) {
        return false;
    }

    return true;
}

//==========================================================
//=============== WhatsminerDeviceV2========================
//==========================================================

bool WhatsminerDeviceV2::ReadData()
{
    if (ip_address_.isEmpty()) {
        qWarning() << QString("WhatsminerDevice: IP адрес не установлен.");
        return false;
    }

    if(!CheckReachability()) {
        qInfo() << QString("WhatsminerDevice: [%1] устройство недоступно.").arg(ip_address_);
        return false;
    }

    return full_update_();
}

void WhatsminerDeviceV2::AsyncReadData()
{
    if(!future_read_.isRunning()) {
        future_read_ = QtConcurrent::run(&WhatsminerDeviceV2::ReadData, this);
    }
}

QJsonObject WhatsminerDeviceV2::SaveToJson()
{
    QJsonObject obj;
    obj.insert("address", ip_address_);
    obj.insert("port", port_);
    obj.insert("id", id_);
    obj.insert("version", u"V2"_s);
    return obj;
}

QByteArray WhatsminerDeviceV2::tcp_request_(const QByteArray &data) const
{
    QTcpSocket sock;
    sock.setProxy(QNetworkProxy::NoProxy);
    sock.connectToHost(ip_address_, port_);
    if (!sock.waitForConnected(CONNECTION_TIMEOUT)) {
        return {};
    }

    sock.write(data);
    if (!sock.waitForBytesWritten(CONNECTION_TIMEOUT)) {
        return {};
    }

    QByteArray response;
    QElapsedTimer elapsed_timer;
    elapsed_timer.start();

    while (elapsed_timer.elapsed() < CONNECTION_TIMEOUT) {

        int remaining = CONNECTION_TIMEOUT - static_cast<int>(elapsed_timer.elapsed());

        if(sock.waitForReadyRead(std::min(remaining, 500))) {
            response += sock.readAll();
        }

        if (!response.isEmpty() && is_valid_json_(response)) {
            break;
        }

        if (sock.state() == QAbstractSocket::UnconnectedState) {
            break;
        }
    }

    sock.disconnectFromHost();
    return response;
}

QJsonObject WhatsminerDeviceV2::send_read_command_(const QString &cmd) const
{

    QByteArray raw = tcp_request_(cmd.toUtf8());
    if (raw.isEmpty()) return {};

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(raw, &err);

    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << QString("WhatsminerDevice: [%1] ошибка разбора пакета JSON по запросу [%2]: %3").arg(ip_address_, cmd.toUtf8(), err.errorString());
        return {};
    }
    return doc.object();
}

bool WhatsminerDeviceV2::full_update_()
{
    bool all_ok = true;

    {
        QJsonObject resp = send_read_command_(R"({"cmd":"summary"})");
        QMutexLocker locker(&mutex_);
        all_ok = all_ok && parse_summary_(resp);;
    }

    {
        QJsonObject resp = send_read_command_(R"({"cmd":"pools"})");
        QMutexLocker locker(&mutex_);
        all_ok = all_ok && parse_pools_(resp);
    }

    {
        QJsonObject resp = send_read_command_(R"({"cmd":"get_error_code"})");
        QMutexLocker locker(&mutex_);
        all_ok = all_ok && parse_error_code_(resp);
    }
    {
        QJsonObject resp = send_read_command_(R"({"cmd":"get_version"})");
        QMutexLocker locker(&mutex_);
        all_ok = all_ok && parse_version_(resp);
    }
    {
        QJsonObject resp = send_read_command_(R"({"cmd":"get_psu"})");
        QMutexLocker locker(&mutex_);
        all_ok = all_ok && parse_psu_info_(resp);
    }
    {
        QJsonObject resp = send_read_command_(R"({"cmd":"edevs"})");
        QMutexLocker locker(&mutex_);
        all_ok = all_ok && parse_edevs_info(resp);
    }
    {
        QJsonObject resp = send_read_command_(R"({"cmd":"get_miner_info"})");
        QMutexLocker locker(&mutex_);
        all_ok = all_ok && parse_miner_info_(resp);
    }

    device_offline_counter_ = all_ok ? 0 : (device_offline_counter_ + 1);
    data_.Online = device_offline_counter_ < 3;

    if (all_ok) {
        connection_status_ = Reacheable;
    } else {
        connection_status_ = Error;
    }
    return all_ok;
}

bool WhatsminerDeviceV2::parse_summary_(const QJsonObject &json)
{
    if(json.empty() || !json.contains("Msg") || !json.value("Msg").isObject()) {
        qWarning() << QString("WhatsminerDevice: [%1] ошибка разбора пакета JSON по запросу [SUMMARY]").arg(ip_address_);
        return false;
    }

    QJsonObject s = json.value("Msg").toObject();

    data_.THSAverage    = s.value("MHS av").toDouble() / 1e6;
    data_.THS1m         = s.value("MHS 1m").toDouble() / 1e6;
    data_.THS15m        = s.value("MHS 15m").toDouble() / 1e6;
    data_.Power         = s.value("Power").toDouble();
    data_.PowerLimit    = s.value("Power Limit").toDouble();
    data_.PowerMode      = s.value("Power Mode").toString();
    data_.EnvironmentTemp    = s.value("Env Temp").toDouble();

    data_.ChipTempMin  = s.value("Chip Temp Min").toDouble();
    data_.ChipTempMax  = s.value("Chip Temp Max").toDouble();
    data_.ChipTempAvg  = s.value("Chip Temp Avg").toDouble();
    return true;
}

bool WhatsminerDeviceV2::parse_pools_(const QJsonObject &json)
{
    if(json.empty() || !json.contains("POOLS") || !json.value("POOLS").isArray()) {
        qWarning() << QString("WhatsminerDevice: [%1] ошибка разбора пакета JSON по запросу [POOLS]").arg(ip_address_);
        return false;
    }

    QJsonArray arr = json.value("POOLS").toArray();
    if(arr.isEmpty()) return false;

    QJsonObject p = arr.at(0).toObject();
    data_.PoolUrl       = p.value("URL").toString();
    data_.WorkerName    = p.value("User").toString();

    data_.PoolRejectRate    = p.value("Pool Rejected%").toDouble();
    data_.PoolStatus        = p.value("Status").toString();
    data_.PoolOnline        = data_.PoolStatus == QString("Alive");
    return true;
}

bool WhatsminerDeviceV2::parse_miner_info_(const QJsonObject &json)
{
    if(json.empty() || !json.contains("Msg") || !json.value("Msg").isObject()) {
        qWarning() << QString("WhatsminerDevice: [%1] ошибка разбора пакета JSON по запросу [MINER_INFO]").arg(ip_address_);
        return false;
    }

    data_.SerialNumber = json.value("Msg").toObject().value("minersn").toString();
    return true;
}

bool WhatsminerDeviceV2::parse_error_code_(const QJsonObject &json)
{
    // {"STATUS":[...],"Msg":{"error_code":["xxx","yyy"]}}
    if(json.empty() || !json.contains("Msg") || !json.value("Msg").isObject()) {
        qWarning() << QString("WhatsminerDevice: [%1] ошибка разбора пакета JSON по запросу [ERRORS]").arg(ip_address_);
        return false;
    }

    QJsonObject msg = json.value("Msg").toObject();
    if(msg.value("error_code").isArray()) {
        const QJsonArray codes = msg.value("error_code").toArray();

        data_.HasErrors = !codes.isEmpty();
        data_.ErrorCodes.clear();

        for(size_t i = 0; i < codes.size(); ++i) {
            if(!codes.at(i).isObject()) continue;
            QJsonObject er_obj = codes.at(i).toObject();
            for(const auto& key: er_obj.keys()) {
                data_.ErrorCodes << QString("%1: %2").arg(key, er_obj.value(key).toString());
            }
        }
    }
    return true;
}

bool WhatsminerDeviceV2::parse_version_(const QJsonObject &json)
{
    // {"STATUS":[...],"Msg":{"miner_type":"Whatsminer M63S","firmware":...}}
    if(json.empty() || !json.contains("Msg") || !json.value("Msg").isObject()) {
        qWarning() << QString("WhatsminerDevice: [%1] ошибка разбора пакета JSON по запросу [INFO]").arg(ip_address_);
        return false;
    }

    QJsonObject msg = json.value("Msg").toObject();
    data_.DeviceType      = msg.value("miner_type").toString();
    data_.FirmwareVersion = msg.value("fw_ver").toString();
    data_.ChipType        = msg.value("chip").toString();
    return true;
}

bool WhatsminerDeviceV2::parse_psu_info_(const QJsonObject &json)
{
    if(json.empty() || !json.contains("Msg") || !json.value("Msg").isObject()) {
        qWarning() << QString("WhatsminerDevice: [%1] ошибка разбора пакета JSON по запросу [INFO]").arg(ip_address_);
        return false;
    }
    QJsonObject msg = json.value("Msg").toObject();
    QString buffer = msg.value("vin").toString();
    bool b = false;
    double temp_val = buffer.toDouble(&b);
    data_.PSUVoltage = b ? temp_val / 100.0 : 0.0;

    buffer = msg.value("pin").toString();
    temp_val = buffer.toDouble(&b);
    data_.PSUPower = b ? temp_val : 0.0;

    buffer = msg.value("temp0").toString();
    temp_val = buffer.toDouble(&b);
    data_.PSUTemperature = b ? temp_val : 0.0;
    data_.LiquidTemp = data_.PSUTemperature;

    return true;
}

bool WhatsminerDeviceV2::parse_edevs_info(const QJsonObject &json)
{
    if(json.empty() || !json.contains("DEVS") || !json.value("DEVS").isArray()) {
        qWarning() << QString("WhatsminerDevice: [%1] ошибка разбора пакета JSON по запросу [EDEVS]").arg(ip_address_);
        return false;
    }

    const QJsonArray ar = json.value("DEVS").toArray();
    QString temps;
    for(size_t i = 0; i < ar.size(); ++i) {
        double temp = ar.at(i).toObject().value("Temperature").toDouble();
        temps.append(QString("[%1]").arg(temp));
    }

    data_.BoardTemps = temps;
    return true;
}

//==========================================================
//=============== WhatsminerDeviceV3 =======================
//==========================================================

bool WhatsminerDeviceV3::ReadData()
{
    if (ip_address_.isEmpty()) {
        qWarning() << QString("WhatsminerDevice: IP адрес не установлен.");
        return false;
    }

    if(!CheckReachability()) {
        qInfo() << QString("WhatsminerDevice: [%1] устройство недоступно.").arg(ip_address_);
        return false;
    }

    return full_update_();
}

void WhatsminerDeviceV3::AsyncReadData()
{
    if(!future_read_.isRunning()) {
        future_read_ = QtConcurrent::run(&WhatsminerDeviceV3::ReadData, this);
    }
}

QJsonObject WhatsminerDeviceV3::SaveToJson()
{
    QJsonObject obj;
    obj.insert("address", ip_address_);
    obj.insert("port", port_);
    obj.insert("version", u"V3"_s);
    obj.insert("password", admin_password_);
    return obj;
}

QByteArray WhatsminerDeviceV3::tcp_request_(const QByteArray &data) const
{
    QTcpSocket sock;
    sock.setProxy(QNetworkProxy::NoProxy);
    sock.connectToHost(ip_address_, port_);
    if (!sock.waitForConnected(CONNECTION_TIMEOUT)) {
        return {};
    }

    quint32 length = static_cast<quint32>(data.size());
    QByteArray lengthBytes(4, 0);
    lengthBytes[0] = (length >>  0) & 0xFF;
    lengthBytes[1] = (length >>  8) & 0xFF;
    lengthBytes[2] = (length >> 16) & 0xFF;
    lengthBytes[3] = (length >> 24) & 0xFF;

    sock.write(lengthBytes);
    if (!sock.waitForBytesWritten(CONNECTION_TIMEOUT)) {
        return {};
    }

    sock.write(data);
    if (!sock.waitForBytesWritten(CONNECTION_TIMEOUT)) {
        return {};
    }

    QByteArray response;
    QByteArray header;
    QElapsedTimer elapsed_timer;
    elapsed_timer.start();

    while (header.size() < 4 && elapsed_timer.elapsed() < CONNECTION_TIMEOUT) {
        if (sock.waitForReadyRead(200))
            header += sock.readAll();
    }

    if(header.size() < 4) {
        qWarning() << QString("WhatsminerDevice: [%1] не удалось прочитать длину ответа.").arg(ip_address_);
        return {};
    }

    quint32 expected_len = static_cast<quint8>(header[0]) | (static_cast<quint8>(header[1]) << 8)
                          | (static_cast<quint8>(header[2]) << 16) | (static_cast<quint8>(header[3]) << 24);

    response = header.mid(4);
    while (response.size() < expected_len && elapsed_timer.elapsed() < CONNECTION_TIMEOUT) {

        int remaining = CONNECTION_TIMEOUT - static_cast<int>(elapsed_timer.elapsed());

        if(sock.waitForReadyRead(std::min(remaining, 500))) {
            response += sock.readAll();
        }

        if (!response.isEmpty() && is_valid_json_(response)) {
            break;
        }

        if (sock.state() == QAbstractSocket::UnconnectedState) {
            break;
        }
    }

    sock.disconnectFromHost();
    return response.left(expected_len);
}

QJsonObject WhatsminerDeviceV3::send_read_command_(const QString &cmd) const
{

    QByteArray raw = tcp_request_(cmd.toUtf8());
    if (raw.isEmpty()) return {};

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(raw, &err);

    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << QString("WhatsminerDevice: [%1] ошибка разбора пакета JSON по запросу [%2]: %3").arg(ip_address_, cmd.toUtf8(), err.errorString());
        return {};
    }
    return doc.object();
}

bool WhatsminerDeviceV3::full_update_()
{
    bool all_ok = true;
    QMutexLocker locker(&mutex_);
    {
        QJsonObject resp = send_read_command_(R"({"cmd":"get.device.info"})");
        all_ok = all_ok && parse_device_info_(resp);
    }

    {
        QJsonObject resp = send_read_command_(R"({"cmd":"get.miner.status","param": "summary"})");
        all_ok = all_ok && parse_summary_(resp);
    }
    {
        QJsonObject resp = send_read_command_(R"({"cmd":"get.miner.status","param": "pools"})");
        all_ok = all_ok && parse_pools_(resp);
    }

    device_offline_counter_ = all_ok ? 0 : (device_offline_counter_ + 1);
    data_.Online = device_offline_counter_ < 3;

    if (all_ok) {
        connection_status_ = Reacheable;
    } else {
        connection_status_ = Error;
    }
    return all_ok;
}

bool WhatsminerDeviceV3::parse_device_info_(const QJsonObject &json)
{
    if(json.isEmpty() || !json.contains("desc") || json.value("desc").toString() != u"get.device.info"_s
        || !json.contains("msg") || !json.value("msg").isObject()) {

        qWarning() << QString("WhatsminerDevice: [%1] ошибка разбора пакета JSON по запросу [get.device.info]").arg(ip_address_);
        return false;
    }

    QJsonObject msg = json.value("msg").toObject();

    if(!msg.contains("miner") || !msg.value("miner").isObject()
        || !msg.contains("system") || !msg.value("system").isObject()
        || !msg.contains("power") || !msg.value("power").isObject()
        || !msg.contains("error-code") || !msg.value("error-code").isArray()) {

        qWarning() << QString("WhatsminerDevice: [%1] ошибка разбора пакета JSON по запросу [get.device.info]").arg(ip_address_);
        return false;
    }

    data_.PoolOnline    = msg.value("miner").toObject().value("working").toString() == "true";
    data_.DeviceType    = msg.value("miner").toObject().value("type").toString();
    data_.ChipType      = msg.value("miner").toObject().value("chipdata0").toString();
    data_.SerialNumber  = msg.value("miner").toObject().value("miner-sn").toString();

    bool b;
    double pl = msg.value("miner").toObject().value("power-limit-set").toString().toDouble(&b);
    data_.PowerLimit = b ? pl : 0;

    data_.FirmwareVersion   = msg.value("system").toObject().value("fwversion").toString();

    data_.PSUVoltage        = msg.value("power").toObject().value("vin").toDouble();
    data_.PSUPower          = msg.value("power").toObject().value("pin").toDouble();
    data_.PSUTemperature    = msg.value("power").toObject().value("temp0").toDouble();
    data_.LiquidTemp        = msg.value("power").toObject().value("liquid-temperature").toDouble();

    int mode = msg.value("power").toObject().value("mode").toString().toInt(&b);
    if(b) {
        switch(mode) {
        case 0: data_.PowerMode = u"Low"_s; break;
        case 1: data_.PowerMode = u"Normal"_s; break;
        case 2: data_.PowerMode = u"High"_s; break;
        default: data_.PowerMode = u"Unknown"_s; break;
        }
    }

    if(msg.value("error-code").isArray()) {
        const QJsonArray codes = msg.value("error-code").toArray();

        data_.HasErrors = !codes.isEmpty();
        data_.ErrorCodes.clear();


        for(size_t i = 0; i < codes.size(); ++i) {
            if(!codes.at(i).isObject()) continue;
            QJsonObject er_obj = codes.at(i).toObject();
            for(const auto& key: er_obj.keys()) {
                data_.ErrorCodes << QString("%1: %2").arg(key, er_obj.value(key).toString());
            }
        }
    }

    return true;
}

bool WhatsminerDeviceV3::parse_summary_(const QJsonObject &json) {

    if(json.isEmpty() || !json.contains("desc") || json.value("desc").toString() != u"get.miner.status"_s
        || !json.contains("msg") || !json.value("msg").isObject()) {

        qWarning() << QString("WhatsminerDevice: [%1] ошибка разбора пакета JSON по запросу [get.miner.status]").arg(ip_address_);
        return false;
    }

    QJsonObject msg = json.value("msg").toObject();

    if(!msg.contains("summary") || !msg.value("summary").isObject()) {

        qWarning() << QString("WhatsminerDevice: [%1] ошибка разбора пакета JSON по запросу [get.miner.status]").arg(ip_address_);
        return false;
    }

    data_.THSAverage    = msg.value("summary").toObject().value("hash-average").toDouble();
    data_.THS15m        = msg.value("summary").toObject().value("hash-15min").toDouble();
    data_.THS1m         = msg.value("summary").toObject().value("hash-1min").toDouble();
    data_.Power         = msg.value("summary").toObject().value("power-5min").toDouble();
    data_.EnvironmentTemp = msg.value("summary").toObject().value("environment-temperature").toDouble();
    data_.ChipTempMin   = msg.value("summary").toObject().value("chip-temp-min").toDouble();
    data_.ChipTempMax   = msg.value("summary").toObject().value("chip-temp-max").toDouble();
    data_.ChipTempAvg   = msg.value("summary").toObject().value("chip-temp-avg").toDouble();
    //data_.PowerLimit    = msg.value("summary").toObject().value("power-limit").toDouble();

    const QJsonArray ar = msg.value("summary").toObject().value("board-temperature").toArray();
    QString temps;
    for(size_t i = 0; i < ar.size(); ++i) {
        double temp = ar.at(i).toDouble();
        temps.append(QString("[%1]").arg(temp));
    }

    data_.BoardTemps    = std::move(temps);

    return true;
}

bool WhatsminerDeviceV3::parse_pools_(const QJsonObject &json)
{
    if(json.isEmpty() || !json.contains("desc") || json.value("desc").toString() != u"get.miner.status"_s
        || !json.contains("msg") || !json.value("msg").isObject()) {

        qWarning() << QString("WhatsminerDevice: [%1] ошибка разбора пакета JSON по запросу [get.miner.status]").arg(ip_address_);
        return false;
    }

    QJsonObject msg = json.value("msg").toObject();

    if(!msg.contains("pools") || !msg.value("pools").isArray()) {

        qWarning() << QString("WhatsminerDevice: [%1] ошибка разбора пакета JSON по запросу [get.miner.status]").arg(ip_address_);
        return false;
    }

    QJsonArray arr = msg.value("pools").toArray();
    if(arr.isEmpty()) return false;

    QJsonObject p = arr.at(0).toObject();
    data_.PoolUrl       = p.value("url").toString();
    data_.WorkerName    = p.value("account").toString();
    data_.PoolRejectRate  = p.value("reject-rate").toDouble();
    data_.PoolStatus    = p.value("status").toString();

    return true;
}

QString WhatsminerDeviceV3::compute_token_(const QString &cmd, qint64 ts, const QString& salt) const
{
    // Вычислить токен по документации:
    // sha256_data = SHA256( cmd + password + salt + ts )
    // token       = Base64(sha256_data)[0..7]
    // Формируем строку: cmd + password + salt + ts
    QString input = cmd + admin_password_ + salt + QString::number(ts);

    QByteArray sha256 = QCryptographicHash::hash(input.toUtf8(),
                                                 QCryptographicHash::Sha256);

    // Base64 от 32 байт SHA256, берём первые 8 символов
    QString b64 = QString::fromLatin1(sha256.toBase64());
    return b64.left(8);
}

QJsonObject WhatsminerDeviceV3::send_write_command_(const QJsonObject &cmd)
{
    // Формат запроса:
    // {
    //   "cmd":     "set.miner.power_limit",
    //   "ts":      419304089,
    //   "token":   "Y0Ijoro/",
    //   "account": "super",
    //   "param":   3000          // зависит от команды
    // }

    QMutexLocker locker(&mutex_);

    QJsonObject resp = send_read_command_(R"({"cmd":"get.device.info","param":"salt"})");

    if(resp.size() == 0 || !resp.contains("msg")
        || !resp.value("msg").isObject()
        || !resp.value("msg").toObject().contains("salt")) {

        qWarning() << QString("WhatsminerDevice: [%1] salt не получен. Неверный ответ прибора").arg(ip_address_);
        return {};
    }
    QString salt = resp.value("msg").toObject().value("salt").toString();

    QString cmdName = cmd.value("cmd").toString();
    qint64  ts      = QDateTime::currentSecsSinceEpoch();
    QString token   = compute_token_(cmdName, ts, salt);

    // Собираем итоговый объект
    QJsonObject envelope = cmd;
    envelope["ts"]      = ts;
    envelope["token"]   = token;
    envelope["account"] = u"super"_s;  // уровень доступа: super / user1 / user2

    return send_read_command_(QString::fromUtf8(QJsonDocument(envelope).toJson(QJsonDocument::Compact)));
}

QString WhatsminerDeviceV3::Reboot()
{
    // Перезагрузка устройства
    // {"cmd":"set.system.reboot","ts":...,"token":"...","account":"super"}
    QJsonObject cmd;
    cmd["cmd"] = u"set.system.reboot"_s;

    QJsonObject resp = send_write_command_(cmd);
    if (resp.isEmpty()) {
        qWarning() << QString("WhatsminerDevice: [%1] Reboot: нет ответа").arg(ip_address_);
        return u"Нет ответа от устройства"_s;
    }

    int code = resp.value("code").toInt(-1);
    if (code != 0) {
        qWarning() << QString("WhatsminerDevice: [%1] Reboot: ошибка код=%1 msg=%2")
                          .arg(ip_address_).arg(code)
                          .arg(resp.value("msg").toString());
        return QString("Reboot: ошибка код=%2 msg=%3").arg(code).arg(resp.value("msg").toString());;
    }

    qInfo() << QString("WhatsminerDevice: [%1] Reboot: выполнено успешно").arg(ip_address_);
    return u"Reboot: выполнено успешно"_s;
}

QString WhatsminerDeviceV3::SetPowerLimit(int watts)
{
    // Установить ограничение мощности
    // {"cmd":"set.miner.power_limit","ts":...,"token":"...","account":"super","param":3000}

    if (watts <= 0) {
        qWarning() << QString("WhatsminerDevice: [%1] SetPowerLimit: некорректное значение %2 Вт").arg(ip_address_).arg(watts);
        return QString("SetPowerLimit: некорректное значение %1 Вт").arg(watts);
    }

    QJsonObject cmd;
    cmd["cmd"]   = u"set.miner.power_limit"_s;
    cmd["param"] = watts;

    QJsonObject resp = send_write_command_(cmd);
    if (resp.isEmpty()) {
        qWarning() << QString("WhatsminerDevice: [%1] SetPowerLimit: нет ответа").arg(ip_address_);
        return u"SetPowerLimit: нет ответа"_s;
    }

    int code = resp.value("code").toInt(-1);
    if (code != 0) {
        qWarning() << QString("WhatsminerDevice: [%1] SetPowerLimit: ошибка код=%2 msg=%3")
                          .arg(ip_address_).arg(code)
                          .arg(resp.value("msg").toString());
        return QString("SetPowerLimit: ошибка код=%1 msg=%2").arg(code).arg(resp.value("msg").toString());
    }

    qInfo() << QString("WhatsminerDevice: [%1] SetPowerLimit: установлено %2 Вт").arg(ip_address_).arg(watts);
    return QString("SetPowerLimit: установлено %1 Вт").arg(watts);;
}

std::shared_ptr<WhatsminerDevice> Whatsminer::MakeWhatsminerDevice(const QJsonObject &obj)
{
    if(!obj.contains("address") || !obj.value("address").isString()
        || !obj.contains("version") || !obj.value("version").isString()) {

        qWarning() << QString("WhatsminerDevice: ошибка восстановления устройства из JSON");
        return nullptr;
    }

    quint16 port = 4028;
    if(obj.contains("port") && obj.value("port").isDouble()) {
        port = obj.value("port").toInt();
    }

    QString id = obj.value("id").toString();
    QString version = obj.value("version").toString();
    QString password = obj.contains("password") ? obj.value("password").toString() : u"super"_s;

    std::shared_ptr<WhatsminerDevice> ret_ptr;

    if(version == u"V2"_s) {
        WhatsminerDeviceV2* device = new WhatsminerDeviceV2(id);
        device->SetAddress(obj.value("address").toString(), port);
        ret_ptr.reset(device);
    }

    if(version == u"V3"_s) {
        WhatsminerDeviceV3* device = new WhatsminerDeviceV3(id);
        device->SetAddress(obj.value("address").toString(), port);
        ret_ptr.reset(device);
    }

    ret_ptr->SetPassword(password);

    return ret_ptr;
}