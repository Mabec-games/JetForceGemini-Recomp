#include "jfg/boot/native_event_batch.hpp"
#include "jfg/boot/native_periodic_deadline.hpp"
#include "jfg/boot/original_timing_pi_dma.hpp"
#include "jfg/boot/reference_si_dma.hpp"
#include "jfg/boot/sp_status.hpp"
#include "jfg/boot/execution_clock.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>
using namespace jfg::boot;
static unsigned checks = 0;
static void check(bool ok) { ++checks; if (!ok) std::abort(); }
int main() {
  check(!advance_native_periodic_deadline(1,1,0));
  check(!advance_native_periodic_deadline(UINT64_MAX,0,1));
  check(!advance_native_periodic_deadline(UINT64_MAX,UINT64_MAX,1));
  check(!advance_native_periodic_deadline(UINT64_MAX-1,UINT64_MAX-1,2));
  auto last_periodic=advance_native_periodic_deadline(UINT64_MAX-1,UINT64_MAX-1,1);
  check(last_periodic && last_periodic->next==UINT64_MAX && last_periodic->elapsed==1);
  for (std::uint64_t period=1;period<=17;++period) {
    for (std::uint64_t first=0;first<100;first+=7) {
      for (std::uint64_t now=0;now<512;now+=3) {
        auto expected=first;std::uint64_t elapsed=0;
        while (expected<=now) {expected+=period;++elapsed;}
        auto result=advance_native_periodic_deadline(now,first,period);
        check(result && result->next==expected && result->elapsed==elapsed);
      }
    }
  }
  constexpr std::uint32_t vi_period=783520;
  constexpr std::uint64_t origin=UINT64_C(0x180000000);
  for (std::uint64_t phase=0;phase<vi_period;phase+=997) {
    const auto now=origin+phase+120U*vi_period;
    const auto next=origin+vi_period;
    const auto advanced=advance_native_periodic_deadline(now,next,vi_period);
    check(advanced && advanced->elapsed==120 && advanced->next>now);
    check(native_periodic_phase(now,next,vi_period)==phase);
    check(native_periodic_phase(now,advanced->next,vi_period)==phase);
    check((native_periodic_phase(now,next,vi_period)*263U/vi_period)*2U<526U);
  }
  check(native_periodic_phase(0,0,0)==0);
  check(native_periodic_phase(1,3,5)==3);

  // Every nonempty combination and input permutation has the same observable
  // equal-time group; unrelated earlier/later deadlines are not folded into it.
  for (unsigned mask=1;mask<128;++mask) {
    std::vector<ReferenceEventCandidate> items;
    for (unsigned i=0;i<7;++i) if (mask&(1U<<i)) items.push_back({static_cast<ReferenceEvent>(i),100});
    do {
      const auto early=select_native_event_batch(items,99);check(early.valid && early.size==0);
      const auto due=select_native_event_batch(items,100);check(due.valid && due.size==items.size() && due.deadline==100);
      unsigned got=0;for (std::size_t i=0;i<due.size;++i) got|=1U<<static_cast<unsigned>(due.sources[i]);check(got==mask);
    } while (std::next_permutation(items.begin(),items.end(),[](auto a,auto b){return a.source<b.source;}));
  }
  std::array mixed{ReferenceEventCandidate{ReferenceEvent::pi,20},ReferenceEventCandidate{ReferenceEvent::sp,20},ReferenceEventCandidate{ReferenceEvent::vi,30}};
  auto due=select_native_event_batch(mixed,100);check(due.size==2 && due.deadline==20);
  mixed[2]={ReferenceEvent::pi,30};check(!select_native_event_batch(mixed,100).valid);
  mixed[2]={static_cast<ReferenceEvent>(99),30};check(!select_native_event_batch(mixed,100).valid);
  const std::array last{ReferenceEventCandidate{ReferenceEvent::vi,UINT64_MAX}};
  check(select_native_event_batch(last,UINT64_MAX).size==1);
  // Real PI/SI/SP device classes reproduce the tied completion. PI/SI copy
  // data at launch; advancing them only changes their independent status/IRQ.
  std::array<std::uint8_t,64> ram{},rom{};for(unsigned i=0;i<rom.size();++i)rom[i]=static_cast<std::uint8_t>(i);
  auto launch_pi=[&](OriginalTimingPiDma& pi,std::uint64_t now) {
    check(pi.write(0,0,now,ram,rom));check(pi.write(4,0x10000000,now,ram,rom));check(pi.write(12,15,now,ram,rom));
  };
  OriginalTimingPiDma probe;launch_pi(probe,0);const auto duration=*probe.deadline();
  std::array order{ReferenceEvent::pi,ReferenceEvent::si,ReferenceEvent::sp,ReferenceEvent::dp};
  unsigned permutations=0;
  do {
    OriginalTimingPiDma pi;launch_pi(pi,10000-duration);
    ReferenceSiDma si;check(si.boot_command(10000-2304));
    SpStatus sp;check(sp.initialize(0x40,false));
    const auto transferred=ram;unsigned pending=0;
    pi.advance(9999);si.advance(9999);check(!pi.interrupt() && !si.interrupt());
    for (auto event:order) {
      if(event==ReferenceEvent::pi) pi.advance(10000);
      else if(event==ReferenceEvent::si) si.advance(10000);
      else if(event==ReferenceEvent::sp) check(sp.complete_task());
      else pending|=32;
    }
    pending|=pi.interrupt()?16:0;pending|=si.interrupt()?2:0;pending|=sp.interrupt()?1:0;
    check(pending==51 && ram==transferred && sp.read()==0x243);
    InterruptLatch irq;irq.raise(pending);irq.mask(63);
    check(irq.deliverable(true,false,true)==51);
    check(irq.deliverable(false,false,true)==0 && irq.pending()==51);
    irq.acknowledge(16);check(irq.pending()==35);
    ++permutations;
  } while(std::next_permutation(order.begin(),order.end()));
  check(permutations==24);
  const std::array tied{ReferenceEventCandidate{ReferenceEvent::pi,100},ReferenceEventCandidate{ReferenceEvent::sp,100}};
  check(!select_reference_event(tied,100).qualified); // Reference contract unchanged.
  std::cout<<checks<<" native event-batch checks passed\n";
}
