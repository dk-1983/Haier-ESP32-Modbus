[English](README.md) | [Русский](README_RU.md)

# Исходные документы YCJ-A002

Карта сверена 2026-09-20 с:

1. [Итальянский дистрибьютор Haier — Lista variabili YCJ-A002](https://www.haiercondizionatori.it/media/10198/d-1/t-file/Lista-variabili-YCJ-A002.pdf), первая страница PDF, печатная 617. Указано 9600 бод, 8N1.
2. [Русское руководство Haier — YCJ-A002](https://haier-rus.ru/wp-content/uploads/2020/11/instrukcziya_ycj-a002.pdf), страница 8 PDF, печатная 8. Указано 19200 бод, 8N1.

Адреса и перечисления coils, holding и input совпадают. Скорости и некоторые описания DIP отличаются. Эта прошивка использует 19200 по умолчанию, предлагает 9600/19200/38400/57600/115200 и не имитирует физические DIP. Адреса — с нуля в PDU; 40001/30001 — способ отображения, не адреса на линии.

Документы не задают полного соответствия ошибок hOn/YCJ или округления полуградусных температур. Эти различия реализации описаны в [REGISTERS.md](../REGISTERS_RU.md).

Протокол: [Modbus Application Protocol V1.1b3](https://www.modbus.org/file/secure/modbusprotocolspecification.pdf). Реализация Haier: ESPHome 2026.6.5 / HaierProtocol 0.9.31, на основе [paveldn/haier-esphome](https://github.com/paveldn/haier-esphome).
