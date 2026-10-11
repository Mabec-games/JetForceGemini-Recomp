#pragma once
#include "jfg/runtime/pc_input.hpp"
#include "jfg/mod/navigation_mod.hpp"

namespace jfg {
// Conservative prefilter: possible contacts still use the guest collision query.
inline bool pc_camera_box_possible(PcOrbitPoint from,PcOrbitPoint to,
                                   PcOrbitPoint low,PcOrbitPoint high) {
  float enter=0.0F,leave=1.0F;
  const std::array<float,3> a{from.x,from.y,from.z},b{to.x,to.y,to.z},
    lo{low.x,low.y,low.z},hi{high.x,high.y,high.z};
  for(unsigned i=0;i<3;++i) {
    if(!std::isfinite(a[i]) || !std::isfinite(b[i]) || !std::isfinite(lo[i]) ||
       !std::isfinite(hi[i]) || lo[i]>hi[i])return true;
    const float d=b[i]-a[i];
    if(std::abs(d)<0.00001F) {if(a[i]<lo[i]-1.0F || a[i]>hi[i]+1.0F)return false;continue;}
    float first=(lo[i]-1.0F-a[i])/d,last=(hi[i]+1.0F-a[i])/d;
    if(first>last)std::swap(first,last);
    enter=std::max(enter,first);leave=std::min(leave,last);
    if(enter>leave)return false;
  }
  return true;
}
inline bool pc_camera_triangle_possible(PcOrbitPoint from,PcOrbitPoint to,
    PcOrbitPoint a,PcOrbitPoint b,PcOrbitPoint c) {
  const auto sub=[](PcOrbitPoint p,PcOrbitPoint q){return PcOrbitPoint{p.x-q.x,p.y-q.y,p.z-q.z};};
  const auto cross=[](PcOrbitPoint p,PcOrbitPoint q){return PcOrbitPoint{p.y*q.z-p.z*q.y,p.z*q.x-p.x*q.z,p.x*q.y-p.y*q.x};};
  const auto dot=[](PcOrbitPoint p,PcOrbitPoint q){return double(p.x)*q.x+double(p.y)*q.y+double(p.z)*q.z;};
  for(const auto p:std::array<PcOrbitPoint,5>{from,to,a,b,c})
    if(!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))return true;
  if(!pc_camera_box_possible(from,to,
      {std::min({a.x,b.x,c.x}),std::min({a.y,b.y,c.y}),std::min({a.z,b.z,c.z})},
      {std::max({a.x,b.x,c.x}),std::max({a.y,b.y,c.y}),std::max({a.z,b.z,c.z})}))return false;
  const auto direction=sub(to,from),edge1=sub(b,a),edge2=sub(c,a),normal=cross(edge1,edge2);
  const double area2=dot(normal,normal);
  if(!std::isfinite(area2) || area2<0.000001)return true;
  const auto p=cross(direction,edge2);const double determinant=dot(edge1,p);
  if(!std::isfinite(determinant))return true;
  if(std::abs(determinant)<0.000001) {
    const double distance=dot(sub(from,a),normal);
    return !std::isfinite(distance) || distance*distance<=area2;
  }
  const auto offset=sub(from,a),q=cross(offset,edge1);
  const double u=dot(offset,p)/determinant,v=dot(direction,q)/determinant,
    t=dot(edge2,q)/determinant;
  if(!std::isfinite(u) || !std::isfinite(v) || !std::isfinite(t))return true;
  // Loose edge/end tolerances admit near contacts for the guest to decide.
  return u>=-0.01 && v>=-0.01 && u+v<=1.01 && t>=-0.05 && t<=1.05;
}
class PcCameraBroadphase {
  mod::Mesh mesh_;
  mod::CollisionInventory models_;
  bool known_=false;
public:
  // Read fresh geometry and registered moving-model bounds every update.
  explicit PcCameraBroadphase(const mod::Memory& ram) {
    try {
      mesh_=mod::decode_mesh(ram,ram.u32(0x800A0D60U));
      models_=mod::collision_models(ram,mod::actors(ram));
      known_=models_.known && !mesh_.triangles.empty();
    } catch(const std::runtime_error&) {known_=false;}
  }
  bool possible(PcOrbitPoint from,PcOrbitPoint to)const {
    if(!known_)return true;
    for(const auto& model:models_.models)if(model.enabled &&
      pc_camera_box_possible(from,to,{model.lower.x,model.lower.y,model.lower.z},
                                    {model.upper.x,model.upper.y,model.upper.z}))return true;
    for(const auto& triangle:mesh_.triangles) {
      std::array<PcOrbitPoint,3> points{};
      for(unsigned i=0;i<3;++i) {
        if(triangle.vertices[i]>=mesh_.vertices.size())return true;
        const auto v=mesh_.vertices[triangle.vertices[i]];points[i]={v.x,v.y,v.z};
      }
      if(pc_camera_triangle_possible(from,to,points[0],points[1],points[2]))return true;
    }
    return false;
  }
};
} // namespace jfg
