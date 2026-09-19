# HTTP и управление

- GET `/`: текущее состояние и ссылки.
- GET `/health`: версия/память/Wi-Fi/OTA/uptime.
- GET `/haier/status`: свежее состояние, число status, возраст, command_state и request_id.
- GET `/control`: основной пульт; HTTP Basic `admin`/`ota_password`.
- GET `/haier/test-token`: токен текущего запуска, с авторизацией.
- POST `/haier/control`: token, request_id и target/mode/fan/swing/preset; исходные проверенные функции.
- POST `/haier/extended`: token, request_id и ровно одно quiet/display=ON|OFF или vertical_position/horizontal_position с именем из пульта.
- GET `/modbus`: независимые переключатели RTU/TCP, адрес и скорость.
- GET `/modbus/config`: конфигурация; авторизация обязательна.
- POST `/modbus/config`: token, rtu=0|1, tcp=0|1, unit=1..247, baud=9600|19200|38400|57600|115200. Настройки сохраняются в NVS. Запрос во время незавершённой команды возвращает409.
- `/wifi`, `/scan`, `/network`: настройка только со стороны setup AP.

Оба Modbus-транспорта выключены после чистой первой прошивки. Изменение их настроек не отключает Wi-Fi, веб-пульт или OTA. Отключение TCP закрывает слушатель и клиентов; отключение RTU прекращает ответы и сбрасывает буфер при применении конфигурации.

HTTP202 означает приём, `command_state=confirmed` — два совпадающих ответа Haier. Общий арбитр не допускает одновременно несколько команд из веба/RTU/TCP. Последний HTTP request_id повторно не отправляет команду; перезапуск очищает эту защиту. Modbus не имеет HTTP request_id и долговременной дедупликации.

OTA: ArduinoOTA UDP8266, пароль из локального secrets.yaml. Для ESP32 используйте espota.py из закреплённого Arduino-ESP32 framework: версия3.3.9 использует свой поддерживаемый механизм аутентификации. Образы ESP8266 несовместимы. Автоматический rollback без отдельной настройки и проверки не обещается.

MQTT: GET `/mqtt` — настройки; GET `/mqtt/config` — конфигурация и диагностика без пароля; POST `/mqtt/config` — token, enabled, host, port, username, password, clear_password и prefix. Все маршруты требуют Basic-аутентификации. Поля и семантика описаны в MQTT.md.
