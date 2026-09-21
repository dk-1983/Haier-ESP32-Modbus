// SPDX-License-Identifier: MIT
// Adapted from 4VRS-Display; see LICENSE-4VRS.
#pragma once
#include <Arduino.h>
#include <cJSON.h>
#include <math.h>

namespace HaierUpdateJson {
constexpr size_t MAX_PAYLOAD = 15360;
inline bool textField(const cJSON *obj, const char *key, char *out, size_t size, bool empty=false) {
  const cJSON *v=cJSON_GetObjectItemCaseSensitive(obj,key);
  if(!cJSON_IsString(v) || !v->valuestring) return false;
  const char *s=v->valuestring; size_t n=strlen(s);
  if(n>=size || (!empty && !n)) return false;
  // Reject controls and malformed UTF-8 before passing text to the renderer.
  for(size_t i=0;i<n;) {
    uint8_t a=(uint8_t)s[i++];
    if(a<32 || a==127) return false;
    if(a<128) continue;
    unsigned more; uint32_t cp, minimum;
    if(a>=0xc2 && a<=0xdf) {more=1;cp=a&31;minimum=0x80;}
    else if(a>=0xe0 && a<=0xef) {more=2;cp=a&15;minimum=0x800;}
    else if(a>=0xf0 && a<=0xf4) {more=3;cp=a&7;minimum=0x10000;}
    else return false;
    if(i+more>n) return false;
    while(more--) {uint8_t b=(uint8_t)s[i++];if((b&0xc0)!=0x80)return false;cp=(cp<<6)|(b&63);}
    if(cp<minimum || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) return false;
  }
  memcpy(out,s,n+1); return true;
}
inline bool numberField(const cJSON *obj,const char *key,uint32_t &out,uint32_t lo,uint32_t hi) {
  const cJSON *v=cJSON_GetObjectItemCaseSensitive(obj,key);
  if(!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || v->valuedouble<lo || v->valuedouble>hi || floor(v->valuedouble)!=v->valuedouble) return false;
  out=(uint32_t)v->valuedouble;return true;
}
inline bool uniqueKeys(const cJSON *obj,unsigned depth=0) {
  if(depth>8) return false;
  for(const cJSON *a=obj->child;a;a=a->next) {
    if(cJSON_IsObject(obj)) for(const cJSON *b=a->next;b;b=b->next)
      if(a->string && b->string && !strcmp(a->string,b->string)) return false;
    if((cJSON_IsObject(a)||cJSON_IsArray(a))&&!uniqueKeys(a,depth+1)) return false;
  } return true;
}
inline cJSON *parse(const char *data,size_t length) {
  // cJSON accepts embedded escaped NULs; reject them to prevent truncated keys/text.
  if(!length || length>MAX_PAYLOAD || memchr(data,0,length)) return nullptr;
  for(size_t i=0;i+5<length;i++) if(data[i]=='\\' && !memcmp(data+i+1,"u0000",5)) return nullptr;
  unsigned depth=0;bool quoted=false,escaped=false;
  for(size_t i=0;i<length;i++) {char c=data[i];if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;}
    else if(c=='"')quoted=true;else if(c=='{'||c=='['){if(++depth>8)return nullptr;}else if(c=='}'||c==']'){if(!depth)return nullptr;--depth;}}
  if(depth||quoted)return nullptr;
  const char *end=nullptr; cJSON *root=cJSON_ParseWithLengthOpts(data,length,&end,false);
  if(!root)return nullptr;
  while(end<data+length && (*end==' '||*end=='\r'||*end=='\n'||*end=='\t'))++end;
  if(end!=data+length || !cJSON_IsObject(root) || !uniqueKeys(root)){cJSON_Delete(root);return nullptr;}
  return root;
}
inline bool allowedKeys(const cJSON *obj,const char *const *keys,size_t count) {
  for(const cJSON *v=obj->child;v;v=v->next){bool found=false;for(size_t i=0;i<count;i++)if(v->string&&!strcmp(v->string,keys[i]))found=true;if(!found)return false;}return true;
}
}
