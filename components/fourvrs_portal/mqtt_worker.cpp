// SPDX-License-Identifier: MIT
#include "mqtt_client_state.h"
#include <WiFi.h>
#include <cstdio>
namespace esphome::fourvrs_portal {
static void mqtt_event(void *arg,esp_event_base_t,int32_t id,void *data) {
  auto &r=*static_cast<MqttRuntime *>(arg);auto *e=static_cast<esp_mqtt_event_t *>(data);
  if(id==MQTT_EVENT_CONNECTED){
    r.assembly.clear();r.last_error=0;
    if(esp_mqtt_client_subscribe_single(e->client,r.subscription,0)<0){r.last_error=-2;return;}
    r.connected=true;++r.connections;
    esp_mqtt_client_enqueue(e->client,r.availability,"online",0,1,1,true);
  } else if(id==MQTT_EVENT_DISCONNECTED){r.connected=false;r.assembly.clear();}
  else if(id==MQTT_EVENT_ERROR){r.last_error=e->error_handle?int(e->error_handle->error_type):-1;}
  else if(id==MQTT_EVENT_PUBLISHED){r.last_ack=e->msg_id;}
  else if(id==MQTT_EVENT_DATA){
    if(e->dup || e->data_len<0 || e->topic_len<0 || e->current_data_offset<0 || e->total_data_len<0){r.assembly.clear();++r.dropped;return;}
    if(r.assembly.feed(e->topic,e->topic_len,e->data,e->data_len,e->current_data_offset,e->total_data_len,e->retain)){
      r.assembly.message.received_ms=millis();
      if(xQueueSend(r.rx_queue,&r.assembly.message,0)!=pdTRUE)++r.dropped;
    }
  }
}
static void stop_client(MqttRuntime &r) {
  if(!r.client)return;
  if(r.connected){
    r.last_ack=-1;
    int id=esp_mqtt_client_enqueue(r.client,r.availability,"offline",0,1,1,true);
    uint32_t started=millis();
    while(id>=0 && r.connected && r.last_ack!=id && uint32_t(millis()-started)<750)vTaskDelay(pdMS_TO_TICKS(20));
  }
  r.connected=false;
  esp_mqtt_client_stop(r.client);esp_mqtt_client_destroy(r.client);r.client=nullptr;
  r.assembly.clear();xQueueReset(r.rx_queue);xQueueReset(r.tx_queue);
}
void mqtt_worker_task(void *arg) {
  auto &r=*static_cast<MqttRuntime *>(arg);uint32_t retry_at=0;
  for(;;){
    MqttConfig next;
    if(xQueueReceive(r.config_queue,&next,pdMS_TO_TICKS(20))==pdTRUE){stop_client(r);r.active=next;retry_at=0;r.applying=false;}
    bool should_run=r.active.enabled && WiFi.status()==WL_CONNECTED;
    if(!should_run){stop_client(r);continue;}
    if(!r.client){
      if(retry_at && uint32_t(millis()-retry_at)<5000)continue;
      retry_at=millis();
      std::snprintf(r.availability,sizeof(r.availability),"%s/availability",r.active.prefix);
      std::snprintf(r.subscription,sizeof(r.subscription),"%s/set/+",r.active.prefix);
      esp_mqtt_client_config_t cfg{};
      cfg.broker.address.hostname=r.active.host;cfg.broker.address.port=r.active.port;cfg.broker.address.transport=MQTT_TRANSPORT_OVER_TCP;
      cfg.credentials.client_id=r.client_id;
      cfg.credentials.username=r.active.username[0]?r.active.username:nullptr;
      cfg.credentials.authentication.password=r.active.password[0]?r.active.password:nullptr;
      cfg.session.protocol_ver=MQTT_PROTOCOL_V_3_1_1;cfg.session.keepalive=30;
      cfg.session.last_will.topic=r.availability;cfg.session.last_will.msg="offline";cfg.session.last_will.qos=1;cfg.session.last_will.retain=1;
      cfg.network.reconnect_timeout_ms=5000;cfg.network.timeout_ms=2000;
      cfg.buffer.size=1024;cfg.buffer.out_size=2048;cfg.outbox.limit=4096;cfg.task.stack_size=6144;
      r.client=esp_mqtt_client_init(&cfg);
      if(!r.client){r.last_error=-3;continue;}
      if(esp_mqtt_client_register_event(r.client,MQTT_EVENT_ANY,mqtt_event,&r)!=ESP_OK || esp_mqtt_client_start(r.client)!=ESP_OK){
        esp_mqtt_client_destroy(r.client);r.client=nullptr;r.last_error=-4;continue;
      }
    }
    MqttPublish publish;
    // Bound per-iteration work and never queue stale telemetry while disconnected.
    for(unsigned i=0;i<4 && xQueueReceive(r.tx_queue,&publish,0)==pdTRUE;++i){
      if(r.connected && esp_mqtt_client_enqueue(r.client,publish.topic,publish.payload,0,0,0,true)<0)++r.dropped;
    }
  }
}
}
