# Телеграм Бот с клиентом OPC DA 2.0

Конфигурируемый телеграм бот с возможностью чтения и записи переменных в/из промышленные ПЛК по интерфейсу ОРС DA 2.0 (Windows). 
В утилите конфигурируются сообщения, встраиваемые кнопки, команды. Сообщения могут быть отправлены по команде пользователя (команда или нажатие кнопки), по изменению значения переменной в ПЛК (по условию больше, меньше, равно) или по расписанию (ежеминутно, ежечасно, ежедневно, еженедельно, ежемесячно или ежегодно)

Значения из ПЛК могут передаваться без обработки (как есть в ПЛК), либо с обработкой (умножить на коэффициент, задать текстовые значения для произвольных значений тэга ПЛК)

Бот реализован на библиотеке [tgbot-cpp](https://github.com/reo7sp/tgbot-cpp)

Зависимости для сборки:
- [] Qt6.7
- [] [OPC Core Components](https://opcfoundation.org/developer-tools/samples-and-tools-classic/core-components/)  
- [] OpenSSL
- [] Libcurl
- [] Boost

![](https://github.com/jkapter/OPC_DA_Telegram_bot/blob/main/img/OPC_DA_Telegram_bot_opc_main.png)
![](https://github.com/jkapter/OPC_DA_Telegram_bot/blob/main/img/OPC_DA_Telegram_bot_messages_example.png)
![](https://github.com/jkapter/OPC_DA_Telegram_bot/blob/main/img/Screenshot_20260117-142327_Telegram.jpg)
