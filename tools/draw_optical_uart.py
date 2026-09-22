"""Generate electrically identical English/Russian UART optocoupler schematics."""
from pathlib import Path
from html import escape
import resvg_py

OUT=Path(__file__).resolve().parents[1]/'docs/assets'
def render(ru):
 s=['<svg xmlns="http://www.w3.org/2000/svg" width="1600" height="1130" viewBox="0 0 1600 1130"><rect width="1600" height="1130" fill="#f4f7fa"/><style>text{font-family:Arial,sans-serif;fill:#172b40}.w{stroke:#263b4c;stroke-width:3;fill:none;stroke-linejoin:round}</style><defs><marker id="arrow" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="5" markerHeight="5" orient="auto"><path d="M0 0 L10 5 L0 10 Z" fill="#263b4c"/></marker></defs>']
 def t(x,y,v,size=21):s.append(f'<text x="{x}" y="{y}" font-size="{size}">{escape(v)}</text>')
 def w(*pts):s.append('<polyline class="w" points="'+' '.join(f'{x},{y}' for x,y in pts)+'"/>')
 def box(x,y,a,b):s.append(f'<rect x="{x}" y="{y}" width="{a}" height="{b}" fill="white" stroke="#263b4c" stroke-width="2"/>')
 def dot(x,y):s.append(f'<circle cx="{x}" cy="{y}" r="5" fill="#263b4c"/>')
 def arrow(x,y,x2,y2):s.append(f'<path class="w" d="M{x} {y} L{x2} {y2}" marker-end="url(#arrow)"/>')
 def ground(x,y):
  w((x,y),(x,y+12));w((x-17,y+12),(x+17,y+12));w((x-11,y+20),(x+11,y+20));w((x-5,y+28),(x+5,y+28))
 t(60,56,'Haier UART · PC817C · 9600 bit/s',36)
 t(60,92,'Оптический интерфейс • по испытанному макету автора • 22.09.2026' if ru else 'Optical interface • based on the builder’s tested prototype • 2026-09-22',22)
 for i,(top,title,leftpower,leftsignal,rightpower,rightsignal,gnd,resin,resout) in enumerate([
  (125,'Haier TX → ESP RX','+5 V HAIER','TX HAIER (зелёный)' if ru else 'TX HAIER (green)','+3.3 V ESP','GPIO18 / RX ESP','GND ESP','680 Ω','1 kΩ'),
  (545,'ESP TX → Haier RX','+3.3 V ESP','GPIO17 / TX ESP','+5 V HAIER','RX HAIER (white)' if not ru else 'RX HAIER (белый)','GND HAIER','390 Ω','2 kΩ')]):
  s.append(f'<rect x="40" y="{top}" width="1520" height="390" rx="14" fill="white" stroke="#cbd5df"/>')
  t(70,top+38,f'0{i+1}  {title}',26)
  y=top+135;bottom=top+290
  box(580,top+80,410,250);t(688,top+106,f'U{i+1} · PC817C',22)
  # LED anode=1 at top, cathode=2 at bottom. TX sinks LED current.
  w((90,y),(295,y));box(295,y-10,90,20);w((385,y),(660,y),(660,y+55))
  t(90,y-28,leftpower);t(300,y-34,f'R{1+i*2}  {resin}')
  s.append(f'<path d="M642 {y+55} L678 {y+55} L660 {y+84} Z" fill="none" stroke="#263b4c" stroke-width="3"/>')
  w((641,y+85),(679,y+85));w((660,y+85),(660,bottom),(90,bottom))
  t(598,y-12,'1 · A',18);t(598,bottom-12,'2 · K',18);t(90,bottom+38,leftsignal,20)
  arrow(708,y+65,758,y+40);arrow(718,y+94,768,y+69)
  # Phototransistor: collector at top; emitter arrow points away from base.
  w((827,y+47),(827,y+113));w((827,y+61),(910,y+15),(910,y),(1460,y))
  w((827,y+99),(910,y+145),(910,bottom),(1320,bottom));arrow(862,y+118,895,y+137)
  t(933,y-12,'4 · C',18);t(934,bottom-12,'3 · E',18)
  w((1160,top+70),(1160,top+85));box(1150,top+85,20,35);w((1160,top+120),(1160,y));dot(1160,y)
  t(1120,top+58,rightpower);t(1202,top+108,f'R{2+i*2}  {resout}',20)
  t(1210,y+36,rightsignal,20);ground(1320,bottom);t(1360,bottom+20,gnd,20)
  s.append(f'<path d="M790 {top+120} V{top+312}" stroke="#a5b3c1" stroke-width="1.5" stroke-dasharray="6 6"/>')
 t(60,985,'Без инверсии: TX=0 → LED включён → RX=0; TX=1 → RX=1.' if ru else 'Non-inverting: TX=0 → LED on → RX=0; TX=1 → RX=1.',23)
 t(60,1025,'19200 bit/s Modbus: заменить оптопары на более быстрые и проверить форму импульсов.' if ru else '19200 bit/s Modbus: use faster optocouplers and verify the received waveform.',23)
 t(60,1063,'Полная развязка требует изолированного питания и отсутствия общей земли между сторонами.' if ru else 'Full galvanic isolation requires isolated power and no shared ground between sides.',21)
 t(60,1103,'dk-1983 / Haier-ESP32-Modbus · R1–R4 и U1–U2 относятся только к этой схеме.' if ru else 'dk-1983 / Haier-ESP32-Modbus · R1–R4 and U1–U2 are local to this schematic.',18)
 s.append('</svg>');svg=''.join(s);name='haier-uart-optical'+('' if ru else '-en')
 (OUT/f'{name}.svg').write_text(svg,encoding='utf-8');(OUT/f'{name}.png').write_bytes(resvg_py.svg_to_bytes(svg_string=svg))
if __name__=='__main__':
 OUT.mkdir(parents=True,exist_ok=True)
 render(False);render(True)
