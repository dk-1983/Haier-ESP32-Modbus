#pragma once
static const char SETUP_PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="ru"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Haier PoC — Wi-Fi</title>__STYLE__
</head><body data-page="wifi">__NAV__<main><h1>Haier PoC · Wi-Fi</h1><small>Выберите сеть 2,4 ГГц. Для скрытой сети введите имя вручную.</small>
<button id="scan" type="button">Найти сети</button><div id="scanStatus" role="status"></div><select id="networks" aria-label="Найденные сети"><option value="">Выберите сеть</option></select>
<form id="form"><label>Имя сети (SSID)<input id="ssid" name="ssid" maxlength="32" required></label><label>Пароль сети<input name="password" type="password" maxlength="63" autocomplete="new-password"></label><button id="connect">Подключить</button></form>
<p id="status" role="status">Чтение состояния…</p><a id="address" hidden></a>
<script>
const token='__TOKEN__', $=id=>document.getElementById(id);let submitted=false,connected=false;
async function state(){try{let r=await fetch('/network',{cache:'no-store'});if(!r.ok)return;let s=await r.json();if(!$('ssid').value&&s.ssid)$('ssid').value=s.ssid;connected=s.connected;
$('status').textContent=s.connected?'Подключено к '+s.ssid+'\nIP: '+s.ip+'\nТочка настройки выключится после 15 секунд устойчивой связи.':(s.ssid?'Подключаемся к '+s.ssid+'.\nПоследний код отключения: '+s.reason:'Сеть ещё не выбрана.');
if(s.connected){$('address').hidden=false;$('address').href='http://'+s.ip+'/';$('address').textContent='Открыть устройство: '+s.ip;}
}catch(e){$('status').textContent=connected?'Точка настройки отключена. Вернитесь в локальную сеть и откройте показанный IP.':'Связь с платой потеряна. Проверьте подключение к её Wi-Fi.';}}
$('networks').onchange=()=>{if($('networks').value)$('ssid').value=$('networks').value;};
$('scan').onclick=async()=>{let b=$('scan');b.disabled=true;$('scanStatus').textContent='Поиск сетей…';try{let r=await fetch('/scan',{method:'POST',body:new URLSearchParams({token})});if(!r.ok)throw Error(await r.text());
for(let i=0;i<30;i++){await new Promise(r=>setTimeout(r,500));r=await fetch('/scan',{cache:'no-store'});if(r.status===202)continue;if(!r.ok)throw Error(await r.text());let list=await r.json();$('networks').replaceChildren(new Option('Выберите сеть',''));for(let n of list){if(n.ssid)$('networks').add(new Option(n.ssid+' · '+n.rssi+' dBm'+(n.open?' · открытая':''),n.ssid));}$('scanStatus').textContent='Найдено сетей: '+list.length;return;}throw Error('Поиск не завершился. Повторите.');
}catch(e){$('scanStatus').textContent=e.message;}finally{b.disabled=false;}};
$('form').onsubmit=async e=>{e.preventDefault();$('connect').disabled=true;try{let data=new URLSearchParams(new FormData(e.target));data.set('token',token);let r=await fetch('/wifi',{method:'POST',body:data});if(!r.ok)throw Error(await r.text());submitted=true;$('status').textContent='Сеть сохранена. Подключаемся…';}catch(e){$('status').textContent=e.message;}finally{$('connect').disabled=false;}};
state();setInterval(state,2000);
</script></main></body></html>)HTML";
