# YCJ-A002 source documents

Register map checked on2026-09-20 against:

1. [Haier Italian distributor — Lista variabili YCJ-A002](https://www.haiercondizionatori.it/media/10198/d-1/t-file/Lista-variabili-YCJ-A002.pdf), first PDF page, printed617. Lists9600baud8N1.
2. [Haier Russian manual — YCJ-A002](https://haier-rus.ru/wp-content/uploads/2020/11/instrukcziya_ycj-a002.pdf), PDF page8, printed8. Lists19200baud8N1.

The two documents agree on the coil, holding and input register addresses and enums. Their baud rates and some DIP descriptions differ. This software defaults to19200, provides9600/19200/38400/57600/115200 and does not emulate physical DIP switches. Table addresses below are zero-based PDU addresses; 40001/30001 are display conventions, not wire addresses.

Neither document defines a full mapping from hOn fault numbers to YCJ fault numbers or a rounding rule for half-degree temperature. Those implementation differences are explicitly documented in REGISTERS.md.

Protocol reference: [Modbus Application Protocol V1.1b3](https://www.modbus.org/file/secure/modbusprotocolspecification.pdf). Haier implementation: ESPHome2026.6.5 / HaierProtocol0.9.31, derived from [paveldn/haier-esphome](https://github.com/paveldn/haier-esphome).
