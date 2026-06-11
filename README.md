# Телеграм Бот для устройств Whatsminer

Телеграм бот для устройств Whatsminer с поддержкой API V2 и API V3. Бот читает данные с устройств в локальной сети (добавляются пользователем) и передает данные через Telegramm.
Реализованы команды перезагрузки и установки мощности через команды в чате бота.
Реализован контроль доступа пользователей на запись команд. 

Для работы бота необходимо указать IP адреса устройств в локальной сети. Токен бота задается через конфигурационный файл формата json.
Для обмена с устройтсвом необходимо предварительно через служебную утилиту Whatsmainer разрешить использование API а также изменить пароль администратора. API на запись не работает с паролем по умолчанию.
Бот реализован на библиотеке [tgbot-cpp](https://github.com/reo7sp/tgbot-cpp)

Зависимости для сборки:
- [] Qt6.10
- [] OpenSSL
- [] Libcurl
- [] Boost (Asio)
- [] ZLib

![](https://github.com/jkapter/WhatsminerTgBot/blob/main/img/WmBotData.png)
![](https://github.com/jkapter/WhatsminerTgBot/blob/main/img/WmBotSettings.png)
![](https://github.com/jkapter/WhatsminerTgBot/blob/main/img/WmBotUsers.png)