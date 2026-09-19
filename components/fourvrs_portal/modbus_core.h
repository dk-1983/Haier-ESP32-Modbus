// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace haier_bridge {
enum class Table { COIL, HOLDING, INPUT_REGISTER };
struct Change { Table table; uint16_t address; uint16_t value; };
struct Backend {
  virtual ~Backend() = default;
  virtual uint8_t read(Table table, uint16_t address, uint16_t &value) = 0;
  // Validate the WHOLE batch before applying. Zero means accepted, not hOn-confirmed.
  virtual uint8_t write(const Change *changes, size_t count) = 0;
};
inline uint16_t word(const uint8_t *p) { return uint16_t(p[0]) << 8 | p[1]; }
inline void put(uint8_t *p, uint16_t v) { p[0] = v >> 8; p[1] = v; }
inline uint16_t crc16(const uint8_t *p, size_t n) {
  uint16_t crc = 0xffff;
  while (n--) { crc ^= *p++; for (int i=0;i<8;++i) crc = (crc>>1) ^ ((crc&1)?0xa001:0); }
  return crc;
}
inline size_t exception(uint8_t *out, uint8_t fc, uint8_t code) { out[0]=fc|0x80;out[1]=code;return 2; }
// Caller provides at least 253 output bytes. No allocation, no network dependencies.
inline size_t pdu(Backend &b, const uint8_t *in, size_t n, uint8_t *out) {
  if (!n || n>253) return 0;
  uint8_t fc=in[0];
  if (fc!=1 && fc!=3 && fc!=4 && fc!=5 && fc!=6 && fc!=15 && fc!=16) return exception(out,fc,1);
  if(n<5) return exception(out,fc,3);
  uint16_t a=word(in+1),q=word(in+3);uint8_t error=0;
  if(fc==1 || fc==3 || fc==4) {
    if(n!=5 || !q || q>(fc==1?2000:125))return exception(out,fc,3);
    if(uint32_t(a)+q>65536)return exception(out,fc,2);
    out[0]=fc;out[1]=fc==1?(q+7)/8:q*2;std::memset(out+2,0,out[1]);
    for(uint16_t i=0;i<q;++i){uint16_t v=0;error=b.read(fc==1?Table::COIL:fc==3?Table::HOLDING:Table::INPUT_REGISTER,a+i,v);
      if(error)return exception(out,fc,error);
      if(fc==1)out[2+i/8]|=(v?1:0)<<(i%8);else put(out+2+2*i,v);
    }
    return out[1]+2;
  }
  Change changes[8]{};size_t count=1;
  if(fc==5 || fc==6) {
    if(n!=5 || (fc==5 && q!=0 && q!=0xff00))return exception(out,fc,3);
    changes[0]={fc==5?Table::COIL:Table::HOLDING,a,uint16_t(fc==5?(q!=0):q)};
  } else {
    if(n<6 || !q || q>(fc==15?1968:123) || in[5]!=(fc==15?(q+7)/8:q*2) || n!=size_t(6+in[5]))return exception(out,fc,3);
    if(uint32_t(a)+q>65536 || q>8)return exception(out,fc,2);
    count=q;
    for(size_t i=0;i<count;++i) changes[i]={fc==15?Table::COIL:Table::HOLDING,uint16_t(a+i),uint16_t(fc==15?((in[6+i/8]>>(i%8))&1):word(in+6+2*i))};
  }
  error=b.write(changes,count);if(error)return exception(out,fc,error);
  std::memcpy(out,in,5);return 5;
}
inline size_t rtu(Backend &b,const uint8_t *in,size_t n,uint8_t unit,uint8_t *out) {
  if(n<4 || n>256 || (in[0]!=unit && in[0]!=0) || crc16(in,n)!=0)return 0;
  if(in[0]==0 && in[1]!=5 && in[1]!=6 && in[1]!=15 && in[1]!=16)return 0;
  out[0]=unit;size_t count=pdu(b,in+1,n-3,out+1);
  if(in[0]==0 || !count)return 0;
  uint16_t crc=crc16(out,count+1);out[count+1]=crc;out[count+2]=crc>>8;return count+3;
}
inline size_t tcp(Backend &b,const uint8_t *in,size_t n,uint8_t unit,uint8_t *out) {
  if(n<8 || n>260 || word(in+2)!=0 || word(in+4)!=n-6)return 0;
  std::memcpy(out,in,7);
  size_t count=in[6]==unit?pdu(b,in+7,n-7,out+7):exception(out+7,in[7],0x0b);
  if(!count)return 0;put(out+4,count+1);return count+7;
}
// TCP segmentation/coalescing: append one byte, consume each completed ADU.
struct TcpFrame {
  uint8_t bytes[260]{};size_t used=0;
  int push(uint8_t v) {
    if(used>=sizeof(bytes))return -1;bytes[used++]=v;
    if(used>=6){uint16_t len=word(bytes+4);if(word(bytes+2)!=0 || len<2 || len>254)return -1;
      if(used==size_t(len+6))return 1;}
    return 0;
  }
  void clear(){used=0;}
};
} // namespace haier_bridge
