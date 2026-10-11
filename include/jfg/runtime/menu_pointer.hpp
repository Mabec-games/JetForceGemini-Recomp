#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <istream>
#include <sstream>
#include <string>
#include <vector>

namespace jfg {
struct MenuPointerFrame {
  int x{}, y{}, width{}, height{};
  bool valid{}, moved{}, left_press{}, right_press{};
  int press_x{},press_y{};
};
enum class GuestMenuKind { none, title, start, saves, save_actions, pause_root };
struct GuestMenuState {
  GuestMenuKind kind{};
  int selected{};
  unsigned enabled{};
  bool ready{};
  std::uint32_t identity{};
  bool operator==(const GuestMenuState&) const = default;
};
struct MenuPointerPoint { float x{}, y{}; bool valid{}; };
// Client coordinates and the VI's displayed aspect use the same pixel space.
// Exclude the renderer's letterbox/pillarbox area before mapping to menu units.
inline MenuPointerPoint menu_pointer_point(const MenuPointerFrame& p,
                                           unsigned aspect_width, unsigned aspect_height) {
  if (!p.valid || p.width<=0 || p.height<=0 || !aspect_width || !aspect_height) return {};
  const double scale=std::min(double(p.width)/aspect_width,double(p.height)/aspect_height);
  const double w=scale*aspect_width,h=scale*aspect_height;
  const double x=p.x-(p.width-w)*.5,y=p.y-(p.height-h)*.5;
  if (x<0 || y<0 || x>=w || y>=h) return {};
  return {float(x*320/w),float(y*240/h),true};
}
inline int menu_pointer_target(const GuestMenuState& s, MenuPointerPoint p) {
  if (!s.ready || !p.valid) return -1;
  const auto rect=[&](float l,float t,float r,float b){return p.x>=l&&p.x<r&&p.y>=t&&p.y<b;};
  if (s.kind==GuestMenuKind::title) return 0;
  if (s.kind==GuestMenuKind::start) {
    for (int i=0;i<3;++i) if ((s.enabled&(1U<<i)) && rect(64.f+72*i,170,112.f+72*i,231)) return i;
  }
  if (s.kind==GuestMenuKind::saves) {
    for (int i=0;i<6;++i) if (rect(76.f+29*i,174,100.f+29*i,198)) return i;
    if (rect(90,95,230,149)) return s.selected;
  }
  if (s.kind==GuestMenuKind::save_actions && (rect(93,174,117,198)||(s.selected==0 && rect(90,95,230,149)))) return 0;
  if(s.kind==GuestMenuKind::pause_root) {
    for(int i=0;i<8;++i) {
      const float x=(i&1)?227.0F:63.0F,y=43.0F+32.0F*(i/2);
      if((s.enabled&(1U<<i)) && rect(x,y,x+31,y+30))return i;
    }
    if((s.enabled&256U) && rect(118,189,211,207))return 8;
  }
  return -1;
}
struct MenuPointerPacket { std::uint16_t buttons{}; int target=-1; };
class GuestMenuPointer {
  GuestMenuKind kind_{};
  std::uint32_t identity_{};
public:
  void reset() { *this={}; }
  MenuPointerPacket sample(const GuestMenuState& s, MenuPointerPoint p,
                           bool moved,bool click,bool back,bool allowed,bool manual) {
    if (!allowed || manual || !s.ready || s.kind==GuestMenuKind::none) { reset();return {}; }
    if (s.kind!=kind_ || s.identity!=identity_) {
      reset();kind_=s.kind;identity_=s.identity;
      // Discard edges already queued while the old screen was visible.
      return {};
    }
    if (!p.valid) return {};
    if (back) return {0x4000,-1};
    if (!moved && !click) return {};
    const int hit=menu_pointer_target(s,p);
    if (hit<0 || !(s.enabled&(1U<<hit))) return {};
    if (s.kind==GuestMenuKind::title) return {std::uint16_t(click?0x8000:0),-1};
    if(s.kind==GuestMenuKind::pause_root && hit==8)
      return click?MenuPointerPacket{0x2000,8}:MenuPointerPacket{};
    // Selection is a native-menu event. Never walk through intervening slots.
    // Activation is consumed by the original menu after its selection is set.
    return {std::uint16_t(click?0x8000:0),hit!=s.selected||click?hit:-1};
  }
};
// Diagnostic pointer scripts are consumed only by bounded, noninteractive
// controller replays. They never send host input or run during live play.
class MenuPointerReplay {
  struct Event { std::uint64_t retrace; int x,y,buttons; };
  std::vector<Event> events_;
  std::size_t next_{};
  MenuPointerFrame current_{};
public:
  bool load(std::istream& in) {
    std::string line;
    if (!std::getline(in,line) || line!="jfg-menu-pointer-v1") return false;
    std::vector<Event> parsed;
    while (std::getline(in,line)) {
      if (!line.empty() && line.back()=='\r') line.pop_back();
      if (line.empty() || line.size()>100 || parsed.size()>=1024) return false;
      std::replace(line.begin(),line.end(),',',' ');
      std::istringstream row(line); Event e{};std::string extra;
      if (!(row>>e.retrace>>e.x>>e.y>>e.buttons) || (row>>extra) ||
          e.retrace>1000000 || e.x< -320 || e.x>640 || e.y< -240 || e.y>480 ||
          e.buttons<0 || e.buttons>3 || (!parsed.empty()&&e.retrace<=parsed.back().retrace)) return false;
      parsed.push_back(e);
    }
    if (parsed.empty() || !in.eof()) return false;
    events_=std::move(parsed);next_=0;current_={};return true;
  }
  bool active() const { return !events_.empty(); }
  MenuPointerFrame sample(std::uint64_t retrace) {
    current_.moved=current_.left_press=current_.right_press=false;
    if(next_<events_.size()&&events_[next_].retrace<=retrace) {
      const auto e=events_[next_++];
      current_={e.x,e.y,320,240,true,true,(e.buttons&1)!=0,(e.buttons&2)!=0,e.x,e.y};
    }
    return current_;
  }
};
} // namespace jfg
