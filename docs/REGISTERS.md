# Карта регистров

Одинакова для RTU и TCP. Все адреса **с нуля**. Coil0 не равен Holding0: это разные таблицы. Holding0 обычно отображается клиентом как40001, Input0 как30001. Заводские поля не перемещены; расширения начинаются сразу после них.

| Таблица | Адрес | Назначение | Доступ | Значения | Основа |
|---|---:|---|---|---|---|
| coil | 0 | power | RW | 0=off;1=on | YCJ-A002 |
| coil | 1 | quiet | RW | 0=off;1=on | extension |
| coil | 2 | display | RW | 0=off;1=on | extension |
| holding | 0 | setpoint | RW | 16..30 Celsius integer | YCJ-A002 |
| holding | 1 | mode | RW | 1=cool;2=heat;3=dry;4=fan;5=auto | YCJ-A002 |
| holding | 2 | fan | RW | 1=low;2=medium;3=high;4=auto | YCJ-A002 |
| holding | 3 | control_lock | RW | 1/2/3=unlock (read1);4=lock | YCJ-A002 |
| holding | 4 | swing | RW | 0=off/center;1=vertical;2=horizontal;3=both | extension |
| holding | 5 | preset | RW | 0=none;1=boost;2=sleep | extension |
| holding | 6 | vertical_position | RW | write1=health_up;2=max_up;3=health_down;4=up;6=center;8=down;read12/14=auto;10=max_down | extension |
| holding | 7 | horizontal_position | RW | write0=center;3=max_left;4=left;5=right;6=max_right;read7=auto | extension |
| input | 0 | room_temperature | R | Celsius integer;floor half-degree | YCJ-A002 |
| input | 1 | fault_code | R | 0=none;native hOn error_status for nonzero;YCJ equivalence unverified | YCJ-A002 |
| input | 2 | machine_number | R | always0 | YCJ-A002 |
| input | 3 | room_temperature_x10 | R | Celsius x10 | extension |
| input | 4 | status_age_seconds | R | 0..65535;saturates;65535=never received | extension |
| input | 5 | status_count_low | R | low16bits | extension |
| input | 6 | status_count_high | R | high16bits | extension |
| input | 7 | command_status | R | 0=idle;1=pending;2=confirmed;3=timeout | extension |
| input | 8 | link_flags | R | bit0=fresh_hOn;bit1=RTU_enabled;bit2=TCP_enabled | extension |

## Функции

- FC01: чтение coils; FC05/FC15(0x0F): запись одной/нескольких coils.
- FC03: чтение holding; FC06/FC16(0x10): запись одного/нескольких holding.
- FC04: чтение input.
- RTU CRC16 с младшим байтом первым; данные регистров big-endian. TCP использует MBAP, не RTU-over-TCP, порт502. UnitID1..247, по умолчанию1. RTU broadcast0 принимает только запись и не отвечает; TCP UnitID должен совпадать.

## Подтверждение и ошибки

Положительный ответ записи означает **принятие команды** для отправки hOn. Фактическое выполнение определяется двумя последующими совпадающими status-пакетами: Input7=2. Пока Input7=1, следующие записи через HTTP/RTU/TCP не принимаются. FC16/FC15 валидируются целиком до любого изменения. Регистры чтения всегда отражают последнюю полученную телеметрию; оптимистичная подстановка запроса не используется. Через30с без свежего status основные чтения возвращают исключение0x0B; диагностические Input4..8 доступны. Запись требует status не старше10с; ожидание подтверждения30с.

Исключения:01 unsupported function;02 illegal address/span;03 invalid value/quantity/combination;04 state cannot be represented;06 busy;0B no fresh response from Haier. Ошибочный CRC, чужой RTU-адрес и неполный кадр не вызывают команд. При отключении транспорта его незавершённые входящие кадры отбрасываются, уже принятая hOn-команда завершается.

## Совместимость и ограничения

- Power и mode независимы: запись Holding1 при выключенном блоке не включает питание. PowerON использует наблюдаемый режим Haier.
- В FAN_ONLY нет AUTO fan: явная запись этой комбинации отклоняется. При переходе в FAN_ONLY с прежним AUTO выбираетсяLOW.
- Holding3=2/3 нормализуется в разблокировку (чтение1), как в заводской таблице. Реализация использует hOn `lock_remote`; равенство всех эффектов заводской блокировке необходимо проверить на стенде.
- Input1 использует байт `error_status` штатного hOn status. Ненулевые коды ошибок не объявляются идентичными YCJ-A002 без отдельной таблицы соответствия. Чтение input2 возвращает0, как в заводской карте.
- Input0 округляет0.5°C вниз до заводского разрешения1°C; Input3 сохраняет0.5°C в масштабеx10. Заводская инструкция не описывает округление.
- Holding4=OFF фиксирует обе оси по центру. Holding6/7 задают фиксированное положение соответствующей оси и прекращают её качание. Одновременная запись Holding4 и6/7 отклоняется как неоднозначная.
- QUIET ON нельзя в OFF/FAN_ONLY/BOOST. BOOST/SLEEP нельзя включать в OFF/FAN_ONLY; BOOST отклоняется при QUIET ON.
- Дополнительные регистры не являются частью заводского YCJ-A002. Поддержка очистки/стерилизации и других неописанных функций не заявляется.

## Пример клиента

```python
from pymodbus.client import ModbusTcpClient
with ModbusTcpClient("DEVICE_IP", port=502) as client:
    # pymodbus3.11 API; unit1
    print(client.read_holding_registers(0, count=4, device_id=1))
    print(client.write_register(0, 22, device_id=1))
    # Poll input7 for confirmation, then read holding0 for actual setpoint.
```

Для RTU тот же набор адресов; выберите19200,8N1 и адрес1 либо значения, сохранённые на странице /modbus.
