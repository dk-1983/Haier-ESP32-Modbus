from pathlib import Path
from html import escape
import resvg_py

out=Path(__file__).resolve().parents[1] / 'docs' / 'assets'
s=['<svg xmlns="http://www.w3.org/2000/svg" width="1800" height="1350" viewBox="0 0 1800 1350"><rect width="1800" height="1350" fill="white"/><style>text{font-family:Arial,sans-serif;fill:#172b40} .w{fill:none;stroke:#263b4c;stroke-width:2.5;stroke-linejoin:round}</style>']
def text(x,y,t,size=20,anchor='start',color='#172b40'):
 s.append(f'<text x="{x}" y="{y}" font-size="{size}" text-anchor="{anchor}" style="fill:{color}">{escape(t)}</text>')
def wire(*p):
 s.append('<polyline class="w" points="'+' '.join(f'{x},{y}' for x,y in p)+'"/>')
def box(x,y,w,h): s.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="white" stroke="#263b4c" stroke-width="2.5"/>')
def dot(x,y): s.append(f'<circle cx="{x}" cy="{y}" r="4" fill="#263b4c"/>')
def gnd(x,y):
 wire((x,y),(x,y+12));wire((x-14,y+12),(x+14,y+12));wire((x-9,y+18),(x+9,y+18));wire((x-4,y+24),(x+4,y+24))
def res(x,y,name,val,vertical=False):
 if vertical:
  wire((x,y),(x,y+15));box(x-8,y+15,16,50);wire((x,y+65),(x,y+80));text(x+18,y+36,name,17);text(x+18,y+59,val,17)
 else:
  wire((x,y),(x+15,y));box(x+15,y-8,50,16);wire((x+65,y),(x+80,y));text(x+40,y-34,name,17,'middle');text(x+40,y-14,val,17,'middle')
def cap(x,y,name,val,polar=False):
 wire((x,y),(x,y+40));wire((x-16,y+40),(x+16,y+40));wire((x-16,y+49),(x+16,y+49));wire((x,y+49),(x,y+90));gnd(x,y+90)
 text(x+25,y+40,name,17);text(x+25,y+65,val,17)
 if polar: text(x-28,y+33,'+',19)
def net(x,y,label):text(x,y-12,label,18,'middle')

text(50,52,'Haier • ESP32-S3 • Modbus RS-485',34)
text(50,85,'Принципиальная схема макета  /  GPIO сверены с haier-s3.yaml  /  21.09.2026',19)
wire((50,105),(1750,105))
text(50,144,'01  ПИТАНИЕ',23)
wire((90,210),(470,210));net(110,210,'+5V HAIER')
cap(170,210,'C1','220 µF / 10 V',True);cap(340,210,'C2','0.1 µF')
dot(170,210);dot(340,210)
box(470,175,260,105);text(600,163,'U2  LM1117-3.3',22,'middle');text(484,219,'3  IN',18);text(714,219,'OUT  2',18,'end');text(600,268,'1  GND',18,'middle')
wire((600,280),(600,315));gnd(600,315)
wire((730,210),(1240,210));net(1230,210,'+3V3');cap(850,210,'C3','100 µF / 10 V',True);cap(1080,210,'C4','0.1 µF');dot(850,210);dot(1080,210)
text(1330,205,'LM1117: SOT-223',19);text(1330,238,'Теплоотвод / TAB = OUT',18);text(1330,271,'C3: проверить тип и ESR',18);text(1330,304,'Все земли общие',18)
text(50,385,'02  UART HAIER • 9600 8N1',23);text(1050,385,'03  RS-485 • 19200 8N1 (стенд)',23)
# Module symbol: pin numbers at the boundary, function labels inside.
box(610,445,350,475);text(785,476,'U1  ESP32-S3',25,'middle');text(785,505,'WROOM-1 N16R8',21,'middle')
for y,pin,label in [(555,'10','GPIO17 / TX'),(630,'11','GPIO18 / RX'),(800,'3','EN'),(865,'27','GPIO0 / BOOT')]:
 wire((575,y),(610,y));text(600,y-10,pin,16,'end');text(626,y+7,label,19)
for y,pin,label in [(555,'8','GPIO15 / TX'),(630,'9','GPIO16 / RX'),(790,'23','GPIO21 / DIR')]:
 wire((960,y),(995,y));text(970,y-10,pin,16);text(944,y+7,label,19,'end')
wire((785,445),(785,415));net(785,415,'+3V3');text(800,436,'2',16)
wire((785,920),(785,950));gnd(785,950);text(805,946,'GND: 1, 40, 41 (EPAD)',17)
text(100,493,'Сигналы платы Haier',21)
wire((100,555),(575,555));text(100,544,'RX HAIER ←',19)
wire((100,630),(280,630));res(280,630,'R2','10 kΩ');wire((360,630),(575,630));text(100,618,'TX HAIER →',19)
dot(430,630);wire((430,630),(430,657));res(430,657,'R3','20 kΩ',True);gnd(430,737)
wire((100,700),(170,700));gnd(170,700);text(100,685,'GND',18)
net(260,800,'+3V3');wire((260,800),(320,800));res(320,800,'R1','10 kΩ');wire((400,800),(575,800))
wire((575,865),(340,865));s.append('<circle cx="340" cy="865" r="4" fill="white" stroke="#263b4c" stroke-width="2"/><circle cx="290" cy="865" r="4" fill="white" stroke="#263b4c" stroke-width="2"/>');wire((292,860),(333,840));wire((290,865),(260,865));gnd(260,865);text(355,896,'SB1 · PGM (NO)',17)
# MAX485, crossed nets avoided using separate horizontal rows.
box(1390,490,200,355);text(1605,467,'U3  MAX485',23)
for y,pin,label in [(555,'4','DI'),(630,'1','RO'),(755,'2','/RE'),(790,'3','DE')]:
 wire((1350,y),(1390,y));text(1380,y-10,pin,16,'end');text(1405,y+7,label,20)
wire((995,555),(1350,555))
wire((995,630),(1180,630));res(1180,630,'R4','10 kΩ');wire((1260,630),(1350,630))
dot(1080,630);res(1080,650,'R5','20 kΩ',True);wire((1080,630),(1080,650));gnd(1080,730)
wire((995,790),(1310,790),(1310,755),(1350,755));wire((1310,790),(1350,790));dot(1310,790)
dot(1190,790);wire((1190,790),(1190,820));res(1190,820,'R6*','10 kΩ',True);gnd(1190,900)
wire((1490,490),(1490,420));net(1490,420,'+5V');text(1505,486,'8 VCC',16)
wire((1490,845),(1490,892));gnd(1490,892);text(1510,872,'5 GND',16)
for y,pin,label in [(590,'6','A'),(690,'7','B')]:
 wire((1590,y),(1720,y));text(1575,y+7,label,20,'end');text(1605,y-10,pin,16);text(1720,y-12,'RS-485 '+label,17,'end')
text(1390,947,'DIR: 0 = приём; 1 = передача',17)
wire((50,1000),(1750,1000));text(50,1042,'04  ПРОГРАММИРОВАНИЕ И ПРИМЕЧАНИЯ',23)
text(50,1085,'UART0: GPIO43 / pad 37 (TXD0) → RX адаптера; GPIO44 / pad 36 (RXD0) ← TX адаптера.',20)
text(50,1118,'Уровни адаптера 3.3 V; общая GND. SB1 замыкает GPIO0 на GND при включении питания.',20)
text(50,1156,'Оба делителя: 10 kΩ от источника, 20 kΩ к GND. При входных 5.0 V → расчётные 3.33 V.',20)
text(50,1189,'R6* — предусмотренная подтяжка DIR. Добавить 100 nF между VCC и GND у MAX485.',19)
text(50,1222,'Для PCB: проверить EN RC, питание/нагрев, терминацию и смещение RS-485; см. SCHEMATIC.md.',19)
text(50,1255,'Числа у U1 — площадки модуля, не GPIO. Порядок контактов разъёма Haier здесь не задаётся.',19)
text(50,1307,'dk-1983 / Haier-ESP32-Modbus',19);text(1750,1307,'Макет • без гальванической развязки • не чертёж PCB',18,'end')
s.append('</svg>');svg=''.join(s);out.mkdir(exist_ok=True,parents=True)
(out/'haier-s3-schematic.svg').write_text(svg,encoding='utf-8')
(out/'haier-s3-schematic.png').write_bytes(resvg_py.svg_to_bytes(svg_string=svg))
print(out/'haier-s3-schematic.png')

