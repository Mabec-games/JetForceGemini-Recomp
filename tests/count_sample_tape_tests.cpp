#include "jfg/boot/count_sample_tape.hpp"
#include <sstream>
#include <iostream>
#include <cstdlib>
using jfg::boot::CountSampleTape;
unsigned checks=0;
void check(bool value,const char* text){++checks;if(!value){std::cerr<<text<<'\n';std::exit(1);}}
int main(){
 CountSampleTape off;check(off.sample(1,2,3,0,99)==99,"off changed Count");check(off.finish(),"off finish");
 std::stringstream stream;CountSampleTape record;check(record.record(stream),"record header");
 for(unsigned i=0;i<1000;++i)check(record.sample(1000+i,0x80010000U+4*i,0x24210001U,i%2?5000+i:0,5000+i)==5000+i,"record sample");
 check(record.samples()==1000&&record.finish(),"record complete");
 const auto bytes=stream.str();CountSampleTape replay;check(replay.replay(stream),"replay header");
 for(unsigned i=0;i<1000;++i)check(replay.sample(1000+i,0x80010000U+4*i,0x24210001U,i%2?5000+i:0,std::nullopt)==5000+i,"replay Count");
 check(replay.samples()==1000&&replay.finish(),"replay complete");check(!replay.sample(1,2,3,0,99),"use after finish");
 for(unsigned field=0;field<4;++field){
  std::stringstream input(bytes);CountSampleTape t;check(t.replay(input),"mismatch header");
  check(!t.sample(field==0?1001:1000,field==1?0x80010004U:0x80010000U,field==2?0U:0x24210001U,field==3?1:0,std::nullopt),"boundary mismatch accepted");check(!t.finish(),"failed replay passed");
 }
 {std::stringstream input(bytes);CountSampleTape t;check(t.replay(input)&&!t.finish(),"unconsumed tail accepted");}
 for(unsigned size: {0U,1U,15U,16U,17U,47U}){
  std::stringstream input(bytes.substr(0,size));CountSampleTape t;const auto ok=t.replay(input);
  check(size<16?!ok:ok,"truncated header status");
  if(ok)check(!t.sample(1000,0x80010000,0x24210001,0,std::nullopt),"truncated row accepted");
 }
 {auto bad=bytes;bad[0]^=1;std::stringstream input(bad);CountSampleTape t;check(!t.replay(input),"wrong magic");}
 {std::stringstream output;CountSampleTape t;check(t.record(output),"nonmonotonic header");check(t.sample(2,4,0,0,100)==100,"first monotonic sample");check(!t.sample(3,4,0,0,99),"backward Count");}
 {std::stringstream output;CountSampleTape t;check(t.record(output),"instruction header");check(t.sample(2,4,0,0,100)==100,"instruction sample");check(!t.sample(1,4,0,0,101),"backward instruction ordinal");}
 {std::stringstream output;CountSampleTape t;check(t.record(output),"idle header");check(!t.sample(1,4,0,101,100),"early idle return");}
 {std::stringstream output;CountSampleTape t;check(t.record(output),"output header");output.setstate(std::ios::badbit);check(!t.sample(1,4,0,0,100)&&!t.finish(),"output failure ignored");}
 std::cout<<checks<<" Count sample transcript contracts passed\n";
}
