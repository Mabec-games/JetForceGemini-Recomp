#include "jfg/audio/vi_playback_clock.hpp"
#include "jfg/boot/original_timing_ai_dma.hpp"
#include "jfg/boot/original_timing_pi_dma.hpp"
#include <vector>
#include "jfg/runtime/window_input_mailbox.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
namespace {
void check(bool okay, const char* text) { if (!okay) { std::cerr << text << '\n'; std::exit(1); } }
}
int main() {
  using Clock = jfg::audio::ViPlaybackClock;
  using namespace std::chrono_literals;
  Clock clock;const Clock::Time origin{};
  clock.anchor(0,origin,0);
  check(clock.deadline(46875000,origin+1100ms,0)==origin+1s,"ordinary delay must catch up");
  check(clock.deadline(46875000,origin+1200ms,1)==origin+1200ms,"observed starvation reanchors playback");
  check(clock.deadline(93750000,origin+2100ms,1)==origin+2200ms,"new origin retains physical Count rate");
  check(clock.deadline(93750000,origin+2400ms,1)==origin+2200ms,"one underrun must not reanchor twice");
  jfg::WindowInputMailbox input;std::array<bool,256> down{},pressed{};bool close=false;
  input.key(32,true);input.key(32,false);input.consume(down,pressed,close);
  check(!down[32] && pressed[32],"short press survives between game polls");
  input.motion(9000,-9000);input.wheel(10000);input.lose_focus();input.consume(down,pressed,close);
  auto motion=input.take_motion();check(!pressed[32] && motion.focus_lost && !motion.x && !motion.y && !motion.wheel,"focus loss clears old input");
  input.motion(9000,-9000);input.wheel(10000);motion=input.take_motion();
  check(motion.x==4096 && motion.y==-4096 && motion.wheel==3840,"unbounded host input cannot overflow accumulators");
  input.close();input.consume(down,pressed,close);check(close,"close request reaches game thread");
  std::thread producer([&]{for(int i=0;i<10000;++i){input.key(8,true);input.key(8,false);}input.key(9,true);});
  for(int i=0;i<10000;++i)(void)input.consume_frame(down,pressed,close);
  producer.join();input.consume(down,pressed,close);check(down[9] && !down[8],"mailbox final state matches producer");
  jfg::boot::OriginalTimingAiDma ai;
  check(ai.write(8,1,0,4096,783520) && ai.write(16,2210,0,4096,783520) &&
      ai.write(20,15,0,4096,783520),"AI format configuration");
  check(ai.write(4,2944,100,4096,783520),"first AI slot");
  const auto end=*ai.deadline();
  check(end==1566999,"AI uses physical Count/sample ratio");
  check(ai.write(4,2944,100,4096,783520) && !ai.write(4,2944,100,4096,783520),"AI has exactly two slots");
  ai.advance(end-1);check(!ai.interrupt(),"AI cannot complete early");
  ai.advance(end);check(ai.interrupt() && *ai.deadline()==end+(end-100),"queued AI buffer starts at original deadline");
  check(ai.write(12,0,end,4096,783520) && !ai.interrupt(),"AI interrupt acknowledgment");
  ai.advance(*ai.deadline());check(ai.interrupt() && !ai.deadline(),"AI second completion");
  jfg::boot::OriginalTimingPiDma pi;
  std::vector<std::uint8_t> ram(128),rom(128);rom[0]=0x12;rom[1]=0x34;
  check(pi.write(0,0,0,ram,rom) && pi.write(4,0x10000000,0,ram,rom),"PI address setup");
  check(pi.write(12,1,0,ram,rom),"PI short transfer");
  check(ram[3]==0x12 && ram[2]==0x34 && *pi.deadline()==14,"PI data byte order and cartridge clock");
  check(!pi.write(12,1,0,ram,rom),"PI rejects concurrent DMA");
  pi.advance(13);check(!pi.interrupt(),"PI cannot complete early");
  pi.advance(14);check(pi.interrupt() && !pi.deadline(),"PI completion at bus deadline");
  check(pi.write(16,2,14,ram,rom) && !pi.interrupt(),"PI interrupt acknowledgment");
  return 0;
}
