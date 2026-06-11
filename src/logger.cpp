#include "logger.h"

#include <QFile>
#include <QDateTime>

QFile* Logger::logfile_ = nullptr;
QtMessageHandler Logger::original_handler_ = nullptr;
const std::unordered_map<QtMsgType, QString> Logger::type_names_ = {
    {QtMsgType::QtDebugMsg,		" Debug  "},
    {QtMsgType::QtInfoMsg,		"  Info  "},
    {QtMsgType::QtWarningMsg,	"Warning "},
    {QtMsgType::QtCriticalMsg,	"Critical"},
    {QtMsgType::QtFatalMsg,		" Fatal  "}
};

unsigned int Logger::max_size_ = 200000; //200kb

QString Logger::file_name_ = "new_log.txt";
QString Logger::app_directory_{};

QMutex Logger::logger_mtx_{};

void Logger::check_and_rename_()
{
    if(!logfile_) return;
    if(logfile_->size() > max_size_) {
        logfile_->write(QString("========================================================================\r\n").toUtf8());
        logfile_->write(QString("Создание нового файла лога").toUtf8());
        logfile_->flush();
        logfile_->rename(QString("%1/%2_%3").arg(app_directory_, QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss").toUtf8(), file_name_));
            logfile_->setFileName(QString("%1/%2").arg(app_directory_, file_name_));

        if(!logfile_->open(QIODevice::Append)) {
            delete logfile_;
            logfile_ = nullptr;
            if(original_handler_) {
                qInstallMessageHandler(original_handler_);
            }
        }
    }
}

Logger::Logger(const QString &dir, const QString &name)
{
    Logger::Init(dir, name);
}

void Logger::Init(const QString &file_directory, const QString &file_name)
{
    file_name_ = file_name;
    app_directory_ = file_directory;

    if(logfile_) return;

    logfile_ = new QFile();

    logfile_->setFileName(QString("%1/%2").arg(app_directory_, file_name_));
    if(logfile_->open(QIODevice::Append)) {
        logfile_->write(QString("========================================================================\r\n").toUtf8());
        logfile_->write(QString("%1\t-\tСтарт программы\r\n").arg(QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss")).toUtf8());
        logfile_->flush();

        original_handler_ = qInstallMessageHandler(&Logger::LogMessage);
    } else {
        delete logfile_;
        logfile_ = nullptr;
    }

}

void Logger::Clear()
{
    if (logfile_) {
        logfile_->write(QString("%1\t-\tСтоп программы\r\n").arg(QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss")).toUtf8());
        logfile_->write(QString("========================================================================\r\n").toUtf8());
        logfile_->flush();
        logfile_->close();
        delete logfile_;
        logfile_ = nullptr;
    }

    if(original_handler_) {
        qInstallMessageHandler(original_handler_);
    }
}

void Logger::SetMaxSize(unsigned int new_size)
{
    max_size_ = new_size;
}

void Logger::LogMessage(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    QMutexLocker locker(&logger_mtx_);
    check_and_rename_();

    if(original_handler_) {
        original_handler_(type, context, message);
    }

    if(!logfile_) {
        return;
    }

    auto it = type_names_.find(type);
    QString type_str = (it != type_names_.end()) ? it->second : "Unknown ";

    QString log_line = QString("%1 | %2 | ").arg(QDateTime::currentDateTime().toString("dd.MM.yyyy hh:mm:ss.z"), -25)
                                            .arg(type_str);

    if(context.line > 0) {
        log_line.append(QString("%1 | %2 | %3 | ").arg(context.line, -5)
                        .arg(QString(context.file).section('/', -1), -20) // File name without file path
                        .arg(QString(context.function).section('(', -2, -2).section(' ', -1).section(':', -1), -40)	// Function name only
                        );
    }

    log_line.append(message);
    log_line.append(QString("\n"));

    logfile_->write(log_line.toUtf8());
    logfile_->flush();
}

Logger::~Logger()
{
    Logger::Clear();
}
