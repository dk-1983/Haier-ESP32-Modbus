// SPDX-License-Identifier: MIT
#include "fourvrs_portal.h"
// SPDX-License-Identifier: MIT
// Adapted from 4VRS-Display; see LICENSE-4VRS.
#include <Arduino.h>
#include <algorithm>
#include <atomic>
#include <esp_heap_caps.h>
#include <time.h>
#include <Preferences.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_ota_ops.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include "UpdateJson.h"
#include "update_model.h"
#include "UpdateTrust.h"
#include "UpdateCertificates.h"

namespace HaierUpdate {
enum class ScreenStage : unsigned { Idle, Checking, Downloading, Verifying, Restarting, Error, Blocked };
static std::atomic<ScreenStage> screenStage{ScreenStage::Idle};
static std::atomic<uint32_t> downloadBytes{0},downloadSize{0};
inline unsigned downloadPercent() {
  const uint32_t total=downloadSize.load(),done=downloadBytes.load();
  return total?unsigned(std::min<uint64_t>(100,uint64_t(done)*100/total)):0;
}
static constexpr char PROFILE[]="haier-s3-n16r8-v1";
#ifdef HAIER_TEST_FEED
static constexpr char FEED[]="https://raw.githubusercontent.com/dk-1983/Haier-ESP32-Modbus/main/releases/testing.json";
#else
static constexpr char FEED[]="https://raw.githubusercontent.com/dk-1983/Haier-ESP32-Modbus/main/releases/stable.json";
#endif
static constexpr char ASSET_PREFIX[]="https://github.com/dk-1983/Haier-ESP32-Modbus/releases/download/v";
struct Settings { uint32_t magic=0x55504431,revision=0; bool web=true,ha=true,managed=false; };
static Settings settings;
RTC_NOINIT_ATTR static uint32_t traceStage;
static uint32_t previousTrace=traceStage;
static std::atomic<unsigned> stackFree{0};
static Preferences storage;
static SemaphoreHandle_t mutex=nullptr,flashGate=nullptr,policyGate=nullptr;
static std::atomic<bool> webEnabled{true},haEnabled{true},managed{false},busy{false},manual{false},canvasReleased{false},checkRequested{false},bootConfirmed{false},networkReady{false},restartRequested{false};
static std::atomic<bool> installRequested{false};
static bool softwareTrial=false;static uint32_t trialPrevious=0;
static std::atomic<uint32_t> leaseUntil{0},changes{0};
static char installed[24]{},available[24]{},phase[32]="starting",error[64]{},blockedHash[65]{};
static std::atomic<int> httpCode{0},transportCode{0},tlsCode{0},tlsFlags{0};
static uint32_t bootStart=0,healthySince=0;
static bool pendingBoot=false,storageReady=false;

inline void state(const char *p,const char *e="") {
  screenStage=!strcmp(p,"checking")?ScreenStage::Checking:
    !strcmp(p,"downloading")?ScreenStage::Downloading:
    !strcmp(p,"verifying")?ScreenStage::Verifying:
    !strcmp(p,"restarting")?ScreenStage::Restarting:
    !strcmp(p,"error")?ScreenStage::Error:
    !strcmp(p,"blocked")?ScreenStage::Blocked:ScreenStage::Idle;
  xSemaphoreTake(mutex,portMAX_DELAY);strlcpy(phase,p,sizeof(phase));strlcpy(error,e,sizeof(error));xSemaphoreGive(mutex);++changes;
}
inline bool permitted() {return webEnabled;}
inline bool saveSettings(const Settings &next) {
  if(!storageReady||!policyGate)return false;
  xSemaphoreTake(policyGate,portMAX_DELAY);
  bool ok=!restartRequested&&storage.putBytes("settings",&next,sizeof(next))==sizeof(next);
  if(ok){settings=next;webEnabled=next.web;haEnabled=next.ha;managed=next.managed;++changes;}
  xSemaphoreGive(policyGate);return ok;
}
inline bool localPolicy(bool enabled) {Settings next=settings;next.web=enabled;bool ok=saveSettings(next);if(ok&&enabled)checkRequested=true;return ok;}
using haier_management::newer;
inline void hex(const unsigned char *in,size_t n,char *out) {for(size_t i=0;i<n;++i)sprintf(out+i*2,"%02x",in[i]);}
inline int digit(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;}
inline bool unhex(const char *s,unsigned char *out,size_t n){if(strlen(s)!=n*2)return false;for(size_t i=0;i<n;++i){int a=digit(s[2*i]),b=digit(s[2*i+1]);if(a<0||b<0)return false;out[i]=(a<<4)|b;}return true;}
struct Manifest {char version[24]{},url[256]{},sha[65]{};uint32_t size=0;};
inline bool decode(const char *text,size_t size,Manifest &m) {
  cJSON *envelope=HaierUpdateJson::parse(text,size);if(!envelope)return false;
  const cJSON *payload=cJSON_GetObjectItemCaseSensitive(envelope,"payload"),*signature=cJSON_GetObjectItemCaseSensitive(envelope,"signature");
  bool ok=false;
  if(cJSON_IsString(payload)&&cJSON_IsString(signature)&&strlen(payload->valuestring)<=2048&&strlen(signature->valuestring)<=144) {
    unsigned char sig[72],digest[32];size_t sigSize=strlen(signature->valuestring)/2;
    mbedtls_pk_context key;mbedtls_pk_init(&key);
    if(sigSize>=64&&unhex(signature->valuestring,sig,sigSize)&&mbedtls_pk_parse_public_key(&key,(const unsigned char*)UPDATE_PUBLIC_KEY,strlen(UPDATE_PUBLIC_KEY)+1)==0&&mbedtls_sha256((const unsigned char*)payload->valuestring,strlen(payload->valuestring),digest,0)==0&&mbedtls_pk_verify(&key,MBEDTLS_MD_SHA256,digest,32,sig,sigSize)==0) {
      cJSON *p=HaierUpdateJson::parse(payload->valuestring,strlen(payload->valuestring));char profile[48],channel[16];uint32_t schema;
      if(p){ok=HaierUpdateJson::numberField(p,"schema",schema,1,1)&&HaierUpdateJson::textField(p,"profile",profile,sizeof(profile))&&!strcmp(profile,PROFILE)&&HaierUpdateJson::textField(p,"channel",channel,sizeof(channel))&&!strcmp(channel,"stable")&&HaierUpdateJson::textField(p,"version",m.version,sizeof(m.version))&&HaierUpdateJson::textField(p,"url",m.url,sizeof(m.url))&&!strncmp(m.url,ASSET_PREFIX,strlen(ASSET_PREFIX))&&HaierUpdateJson::textField(p,"sha256",m.sha,sizeof(m.sha))&&HaierUpdateJson::numberField(p,"size",m.size,128,0x7c0000);cJSON_Delete(p);}
      unsigned char expected[32];if(ok)ok=unhex(m.sha,expected,32);
    }
    mbedtls_pk_free(&key);
  }
  cJSON_Delete(envelope);return ok;
}
inline esp_http_client_handle_t open(const char *url) {
  char current[2048];if(strlcpy(current,url,sizeof(current))>=sizeof(current))return nullptr;
  httpCode=0;transportCode=0;tlsCode=0;tlsFlags=0;
  // A fresh client resets request/parser state and releases the previous TLS
  // allocation. GitHub CDN URLs also need more than the default 512-byte TX buffer.
  for(unsigned redirects=0;redirects<4;++redirects) {
    if(strncmp(current,"https://",8)){transportCode=ESP_ERR_INVALID_ARG;return nullptr;}
    struct RedirectTarget {char *url;size_t capacity;bool present=false,valid=false;} target{current,sizeof(current)};
    esp_http_client_config_t c{};c.url=current;c.cert_pem=UPDATE_CA_CERTS;
    c.user_data=&target;
    c.event_handler=[](esp_http_client_event_t *event)->esp_err_t {
      if(event->user_data&&event->event_id==HTTP_EVENT_ON_HEADER&&event->header_key&&!strcasecmp(event->header_key,"Location")) {
        auto *next=static_cast<RedirectTarget*>(event->user_data);next->present=true;
        next->valid=event->header_value&&strlcpy(next->url,event->header_value,next->capacity)<next->capacity;
      }
      return ESP_OK;
    };
    c.timeout_ms=15000;c.buffer_size=1024;c.buffer_size_tx=2304;
    c.disable_auto_redirect=true;c.user_agent="Haier-ESP32-Modbus/1";
    auto h=esp_http_client_init(&c);if(!h){transportCode=ESP_ERR_NO_MEM;return nullptr;}
    traceStage=10+redirects;stackFree=uxTaskGetStackHighWaterMark(nullptr);
    transportCode=esp_http_client_open(h,0);
    if(transportCode!=ESP_OK){int code=0,flags=0;esp_http_client_get_and_clear_last_tls_error(h,&code,&flags);tlsCode=code;tlsFlags=flags;esp_http_client_cleanup(h);return nullptr;}
    traceStage=20+redirects;stackFree=uxTaskGetStackHighWaterMark(nullptr);
    int64_t length=esp_http_client_fetch_headers(h);
    if(length<0){transportCode=(int)length;esp_http_client_cleanup(h);return nullptr;}
    int status=esp_http_client_get_status_code(h);httpCode=status;
    if(status==200){esp_http_client_set_user_data(h,nullptr);return h;}
    if(status!=301&&status!=302&&status!=307&&status!=308){esp_http_client_cleanup(h);return nullptr;}
    // get_url() omits the query string; CDN authorization requires the full Location.
    bool next=target.present&&target.valid;
    traceStage=30+redirects;esp_http_client_cleanup(h);
    if(!next){transportCode=ESP_ERR_INVALID_SIZE;return nullptr;}
  }
  transportCode=ESP_ERR_HTTP_MAX_REDIRECT;return nullptr;
}
inline bool fetchManifest(Manifest &m) {
  auto h=open(FEED);if(!h)return false;
  char text[3073];size_t filled=0;bool ok=true;
  while(filled<sizeof(text)-1){int n=esp_http_client_read(h,text+filled,sizeof(text)-1-filled);if(n<0){ok=false;break;}if(!n)break;filled+=n;}
  ok=ok&&esp_http_client_is_complete_data_received(h)&&filled<sizeof(text)-1;
  esp_http_client_cleanup(h);text[filled]=0;return ok&&decode(text,filled,m);
}
inline void install(const Manifest &m) {
  if(!permitted()){state("blocked");return;}
  const esp_partition_t *partition=esp_ota_get_next_update_partition(nullptr);
  if(!partition||m.size>partition->size){state("error","partition_size");return;}
  traceStage=40;auto h=open(m.url);if(!h){state("error","download_https");return;}
  if(esp_http_client_get_content_length(h)!=m.size){esp_http_client_cleanup(h);state("error","download_size");return;}
  esp_ota_handle_t handle=0;
  // Erase sectors as they are written; bulk erase can starve the task watchdog.
  traceStage=50;stackFree=uxTaskGetStackHighWaterMark(nullptr);
  if(esp_ota_begin(partition,OTA_WITH_SEQUENTIAL_WRITES,&handle)!=ESP_OK){esp_http_client_cleanup(h);state("error","ota_begin");return;}
  downloadSize=m.size;downloadBytes=0;state("downloading");
  mbedtls_sha256_context sha;mbedtls_sha256_init(&sha);mbedtls_sha256_starts(&sha,0);
  unsigned char buffer[1024];uint32_t total=0;bool ok=true;
  while(total<m.size) {
    if(!permitted()){ok=false;break;}
    int n=esp_http_client_read(h,(char*)buffer,std::min<size_t>(sizeof(buffer),m.size-total));
    if(n<=0){ok=false;break;}
    if(!total){
      while(n>0&&n<36){int more=esp_http_client_read(h,(char*)buffer+n,36-n);if(more<=0){n=-1;break;}n+=more;}
      if(n<36||buffer[0]!=0xe9||buffer[12]!=9||buffer[13]!=0||memcmp(buffer+32,"\x32\x54\xcd\xab",4)){ok=false;break;}
    }
    traceStage=60;
    if(esp_ota_write(handle,buffer,n)!=ESP_OK){ok=false;break;}
    mbedtls_sha256_update(&sha,buffer,n);total+=n;downloadBytes=total;vTaskDelay(1);
  }
  unsigned char digest[32];char actual[65];mbedtls_sha256_finish(&sha,digest);mbedtls_sha256_free(&sha);hex(digest,32,actual);
  ok=ok&&total==m.size&&!strcmp(actual,m.sha)&&esp_http_client_is_complete_data_received(h);esp_http_client_cleanup(h);
  if(!ok||!permitted()){esp_ota_abort(handle);state("error",permitted()?"download_or_hash":"update_blocked");return;}
  state("verifying");
  if(esp_ota_end(handle)!=ESP_OK){state("error","image_validation");return;}
  if(!permitted()){state("blocked");return;}
  // Serialize acceptance of policy changes with the irreversible slot selection.
  xSemaphoreTake(policyGate,portMAX_DELAY);
  const char *failure=nullptr;
  if(!permitted())failure="update_blocked";
  else if(storage.putString("attempt_sha",m.sha)!=64||storage.putString("attempt_ver",m.version)!=strlen(m.version))failure="attempt_storage";
  else if(!permitted())failure="update_blocked";
  else if(storage.putUInt("trial_previous",esp_ota_get_running_partition()->address)!=4||storage.putUInt("trial_target",partition->address)!=4||storage.putUInt("trial_boots",0)!=4||storage.putBool("trial_pending",true)!=1)failure="trial_storage";
  else if(esp_ota_set_boot_partition(partition)!=ESP_OK)failure="boot_partition";
  else {strlcpy(blockedHash,m.sha,sizeof(blockedHash));restartRequested=true;}
  xSemaphoreGive(policyGate);
  state(failure?"error":"restarting",failure?failure:"");
}
inline void worker(void *) {
  uint32_t next=millis()+60000;
  for(;;){vTaskDelay(pdMS_TO_TICKS(250));
    if(!bootConfirmed||!networkReady||manual||(!checkRequested&&int32_t(millis()-next)<0))continue;
    bool explicitCheck=checkRequested.exchange(false);bool explicitInstall=installRequested.exchange(false);next=millis()+21600000+(esp_random()%60000);
    if(xSemaphoreTake(flashGate,0)!=pdTRUE)continue;
    bool expected=false;if(!busy.compare_exchange_strong(expected,true)){xSemaphoreGive(flashGate);continue;}
    downloadBytes=0;downloadSize=0;canvasReleased=false;
    while(!canvasReleased)vTaskDelay(pdMS_TO_TICKS(10));
    state("checking");Manifest manifest;
    if(fetchManifest(manifest)) {
      xSemaphoreTake(mutex,portMAX_DELAY);strlcpy(available,manifest.version,sizeof(available));xSemaphoreGive(mutex);
      if(!newer(manifest.version,installed))state("up_to_date");
      else if(!strcmp(manifest.sha,blockedHash))state("error","previous_attempt_failed");
      else if(!permitted())state("blocked");
      else if(explicitInstall||!explicitCheck)install(manifest);
      else state("available");
    } else state("error","manifest_https_or_signature");
    busy=false;canvasReleased=false;xSemaphoreGive(flashGate);
  }
}
inline bool begin(const char *version) {
  strlcpy(installed,version,sizeof(installed));mutex=xSemaphoreCreateMutex();flashGate=xSemaphoreCreateMutex();policyGate=xSemaphoreCreateMutex();if(!mutex||!flashGate||!policyGate)return false;
  storageReady=storage.begin("haier-update",false);if(!storageReady)return false;
  if(storage.isKey("settings")){
    if(storage.getBytesLength("settings")!=sizeof(settings)||storage.getBytes("settings",&settings,sizeof(settings))!=sizeof(settings)||settings.magic!=0x55504431){webEnabled=false;haEnabled=false;state("error","settings_invalid");return false;}
  }
  webEnabled=settings.web;haEnabled=settings.ha;managed=settings.managed;
  String attempt=storage.getString("attempt_sha","");strlcpy(blockedHash,attempt.c_str(),sizeof(blockedHash));
  esp_ota_img_states_t status;pendingBoot=esp_ota_get_state_partition(esp_ota_get_running_partition(),&status)==ESP_OK&&status==ESP_OTA_IMG_PENDING_VERIFY;
  trialPrevious=storage.getUInt("trial_previous",0);
  softwareTrial=storage.getBool("trial_pending",false)&&storage.getUInt("trial_target",0)==esp_ota_get_running_partition()->address;
  if(softwareTrial){
    uint32_t boots=storage.getUInt("trial_boots",0);
    if(boots>=1){
      auto it=esp_partition_find(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_ANY,nullptr);
      while(it){const auto *p=esp_partition_get(it);if(p->address==trialPrevious){esp_partition_iterator_release(it);if(esp_ota_set_boot_partition(p)==ESP_OK){storage.putBool("trial_pending",false);ESP.restart();}break;}it=esp_partition_next(it);}
    }
    if(storage.putUInt("trial_boots",boots+1)!=4){state("error","trial_storage");return false;}
  }
  bootStart=millis();bootConfirmed=!pendingBoot&&!softwareTrial;state((pendingBoot||softwareTrial)?"confirming_boot":"idle");
  return xTaskCreate(worker,"haier-github",12288,nullptr,1,nullptr)==pdPASS;
}
inline void tick(bool localHealthy,bool connected) {
  networkReady=connected&&time(nullptr)>1700000000;
  if(pendingBoot||softwareTrial){
    if(!localHealthy)healthySince=0;else if(!healthySince)healthySince=millis();
#ifdef FOURVRS_TEST_FAIL_BOOT
    if(millis()-bootStart>10000)ESP.restart();
#endif
    if(healthySince&&millis()-healthySince>=45000){
      if(!pendingBoot||esp_ota_mark_app_valid_cancel_rollback()==ESP_OK){
        if(storage.putBool("trial_pending",false)==1){softwareTrial=false;pendingBoot=false;bootConfirmed=true;state("idle");}
      }
    } else if(millis()-bootStart>120000){
      if(pendingBoot)esp_ota_mark_app_invalid_rollback_and_reboot();
      const esp_partition_t *previous=nullptr;
      auto it=esp_partition_find(ESP_PARTITION_TYPE_APP,ESP_PARTITION_SUBTYPE_ANY,nullptr);
      while(it){auto p=esp_partition_get(it);if(p->address==trialPrevious){previous=p;esp_partition_iterator_release(it);break;}it=esp_partition_next(it);}
      if(previous&&previous!=esp_ota_get_running_partition()&&esp_ota_set_boot_partition(previous)==ESP_OK){storage.putBool("trial_pending",false);ESP.restart();}
      state("error","rollback_unavailable");
    }
  }
  if(restartRequested){delay(200);ESP.restart();}
}
inline String status() {
  cJSON *j=cJSON_CreateObject();
  cJSON_AddStringToObject(j,"partition",esp_ota_get_running_partition()->label);
  cJSON_AddBoolToObject(j,"software_trial",softwareTrial);
  cJSON_AddNumberToObject(j,"download_bytes",downloadBytes.load());
  cJSON_AddNumberToObject(j,"download_size",downloadSize.load());
  cJSON_AddNumberToObject(j,"download_percent",downloadPercent());
  cJSON_AddNumberToObject(j,"reset_reason",esp_reset_reason());cJSON_AddNumberToObject(j,"previous_update_stage",previousTrace);cJSON_AddNumberToObject(j,"update_stage",traceStage);cJSON_AddNumberToObject(j,"worker_stack_free",stackFree);
  cJSON_AddNumberToObject(j,"epoch",(double)time(nullptr));cJSON_AddNumberToObject(j,"largest_internal_block",heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));cJSON_AddNumberToObject(j,"http_status",httpCode);cJSON_AddNumberToObject(j,"transport_error",transportCode);cJSON_AddNumberToObject(j,"tls_error",tlsCode);cJSON_AddNumberToObject(j,"tls_flags",tlsFlags);
  cJSON_AddStringToObject(j,"installed",installed);cJSON_AddBoolToObject(j,"web_enabled",webEnabled);cJSON_AddBoolToObject(j,"ha_enabled",haEnabled);cJSON_AddBoolToObject(j,"ha_managed",managed);cJSON_AddNumberToObject(j,"revision",settings.revision);cJSON_AddBoolToObject(j,"effective_enabled",permitted());cJSON_AddBoolToObject(j,"boot_confirmed",bootConfirmed);cJSON_AddBoolToObject(j,"busy",busy);
  if(mutex){xSemaphoreTake(mutex,portMAX_DELAY);cJSON_AddStringToObject(j,"available",available);cJSON_AddStringToObject(j,"phase",phase);cJSON_AddStringToObject(j,"error",error);xSemaphoreGive(mutex);}
  char *raw=cJSON_PrintUnformatted(j);String result=raw?raw:"{}";cJSON_free(raw);cJSON_Delete(j);return result;
}
}

#include "UpdatesPage.h"
#include "version.h"
namespace esphome::fourvrs_portal {
void Portal::updates_setup_(){
  configTime(0,0,"pool.ntp.org","time.cloudflare.com");
  #ifdef HAIER_TEST_FEED
  const char *update_version="1.0.99"; // Private end-to-end update test; never a public binary.
#else
  const char *update_version=HAIER_FIRMWARE_VERSION;
#endif
  if(!HaierUpdate::begin(update_version))ESP_LOGE("updates","Update service unavailable");
}
bool Portal::updates_busy_(){return HaierUpdate::busy||HaierUpdate::restartRequested;}
void Portal::updates_ota_(){
  if(!HaierUpdate::flashGate)return;
  if(xSemaphoreTake(HaierUpdate::flashGate,0)==pdTRUE){ArduinoOTA.handle();xSemaphoreGive(HaierUpdate::flashGate);}
}
void Portal::updates_loop_(){
  if(!HaierUpdate::mutex)return;
  if(HaierUpdate::busy&&!test_pending_&&!pending_&&!scanning_&&!wifi_reset_pending_&&!credentials_restart_)HaierUpdate::canvasReleased=true;
  HaierUpdate::tick(credentials_ready_&&storage_ok_&&WiFi.status()==WL_CONNECTED,WiFi.status()==WL_CONNECTED);
}
void Portal::updates_web_(){
  web_.on("/updates",HTTP_GET,[this](){if(test_auth_())send_page_(UPDATES_PAGE);});
  web_.on("/updates/status",HTTP_GET,[this](){if(!test_auth_())return;web_.sendHeader("Cache-Control","no-store");web_.send(200,"application/json",HaierUpdate::status());});
  web_.on("/updates/config",HTTP_POST,[this](){
    if(!test_auth_())return;if(web_.arg("token")!=token_){web_.send(403,"text/plain","Invalid token");return;}
    String enabled=web_.arg("enabled");if(enabled!="0"&&enabled!="1"){web_.send(400,"text/plain","Invalid enabled flag");return;}
    if(!HaierUpdate::localPolicy(enabled=="1")){web_.send(503,"text/plain","Could not save policy");return;}
    web_.send(200,"application/json","{}");
  });
  auto request=[this](bool install){
    if(!test_auth_())return;if(web_.arg("token")!=token_){web_.send(403,"text/plain","Invalid token");return;}
    if(!HaierUpdate::mutex||!HaierUpdate::bootConfirmed){web_.send(503,"text/plain","Update service not ready");return;}
    if(install&&!HaierUpdate::permitted()){web_.send(409,"text/plain","Enable updates before installing");return;}
    HaierUpdate::installRequested=install;HaierUpdate::checkRequested=true;web_.send(202,"application/json","{}");
  };
  web_.on("/updates/check",HTTP_POST,[request](){request(false);});web_.on("/updates/install",HTTP_POST,[request](){request(true);});
}
}
