/*
    Copyright (c) 2025 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "geom.h"

#include <map>
#include <utility>

#include "src/util.h"

namespace hyper {

Geom::Geom() = default;

Geom::~Geom() = default;

Geom::Geom(const Geom& other)
    : pos_vec_(other.pos_vec_),
      norm_vec_(other.norm_vec_),
      index_vec_(other.index_vec_) {}

Geom& Geom::operator=(const Geom& other) {
  if (this != &other) {
    pos_vec_ = other.pos_vec_;
    norm_vec_ = other.norm_vec_;
    index_vec_ = other.index_vec_;
  }
  return *this;
}

Geom::Geom(Geom&& other) noexcept
    : pos_vec_(std::move(other.pos_vec_)),
      norm_vec_(std::move(other.norm_vec_)),
      index_vec_(std::move(other.index_vec_)) {}

Geom& Geom::operator=(Geom&& other) noexcept {
  if (this != &other) {
    pos_vec_ = std::move(other.pos_vec_);
    norm_vec_ = std::move(other.norm_vec_);
    index_vec_ = std::move(other.index_vec_);
  }
  return *this;
}

Geom Geom::MoveFromBuffers(std::vector<glm::vec3> pos,
                            std::vector<glm::vec3> norm,
                            std::vector<uint32_t> indices) {
  Geom result;
  result.pos_vec_ = std::move(pos);
  result.norm_vec_ = std::move(norm);
  result.index_vec_ = std::move(indices);
  return result;
}

void Geom::Clear() {
  pos_vec_.clear();
  norm_vec_.clear();
  index_vec_.clear();
}

void Geom::Push(const Geom& other) {
  size_t vertex_start = pos_vec_.size();
  pos_vec_.insert(pos_vec_.end(), other.pos_vec_.begin(), other.pos_vec_.end());
  norm_vec_.insert(norm_vec_.end(), other.norm_vec_.begin(), other.norm_vec_.end());
  size_t index_start = index_vec_.size();
  index_vec_.insert(index_vec_.end(), other.index_vec_.begin(), other.index_vec_.end());
  for (size_t i = index_start; i < index_vec_.size(); i++) {
    index_vec_[i] += vertex_start;
  }
}

Geom Geom::MakeSphere(glm::vec3 center, float radius, int num_subdivs) {
  // Build icosahedron vertices (unit sphere).
  const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;
  std::vector<glm::vec3> verts = {
      glm::normalize(glm::vec3(-1, t, 0)),  glm::normalize(glm::vec3(1, t, 0)),
      glm::normalize(glm::vec3(-1, -t, 0)), glm::normalize(glm::vec3(1, -t, 0)),
      glm::normalize(glm::vec3(0, -1, t)),  glm::normalize(glm::vec3(0, 1, t)),
      glm::normalize(glm::vec3(0, -1, -t)), glm::normalize(glm::vec3(0, 1, -t)),
      glm::normalize(glm::vec3(t, 0, -1)),  glm::normalize(glm::vec3(t, 0, 1)),
      glm::normalize(glm::vec3(-t, 0, -1)), glm::normalize(glm::vec3(-t, 0, 1)),
  };

  std::vector<uint32_t> indices = {
      0, 11, 5,  0, 5, 1,   0, 1, 7,   0, 7, 10,  0, 10, 11,
      1, 5, 9,   5, 11, 4,  11, 10, 2,  10, 7, 6,  7, 1, 8,
      3, 9, 4,   3, 4, 2,   3, 2, 6,   3, 6, 8,   3, 8, 9,
      4, 9, 5,   2, 4, 11,  6, 2, 10,  8, 6, 7,   9, 8, 1,
  };

  // Subdivide: split each triangle into 4 by inserting edge midpoints.
  using EdgeKey = std::pair<uint32_t, uint32_t>;
  auto make_edge_key = [](uint32_t a, uint32_t b) -> EdgeKey {
    return a < b ? EdgeKey(a, b) : EdgeKey(b, a);
  };

  for (int s = 0; s < num_subdivs; s++) {
    std::map<EdgeKey, uint32_t> mid_cache;
    std::vector<uint32_t> new_indices;
    new_indices.reserve(indices.size() * 4);

    auto get_mid = [&](uint32_t a, uint32_t b) -> uint32_t {
      EdgeKey key = make_edge_key(a, b);
      auto it = mid_cache.find(key);
      if (it != mid_cache.end()) return it->second;
      uint32_t idx = static_cast<uint32_t>(verts.size());
      verts.push_back(glm::normalize((verts[a] + verts[b]) * 0.5f));
      mid_cache[key] = idx;
      return idx;
    };

    size_t num_tris = indices.size() / 3;
    for (size_t i = 0; i < num_tris; i++) {
      uint32_t v0 = indices[i * 3 + 0];
      uint32_t v1 = indices[i * 3 + 1];
      uint32_t v2 = indices[i * 3 + 2];
      uint32_t m01 = get_mid(v0, v1);
      uint32_t m12 = get_mid(v1, v2);
      uint32_t m20 = get_mid(v2, v0);
      new_indices.insert(new_indices.end(), {v0, m01, m20, v1, m12, m01, v2, m20, m12, m01, m12, m20});
    }
    indices = std::move(new_indices);
  }

  // Normals are the unit sphere positions; scale for final positions.
  std::vector<glm::vec3> pos_vec(verts.size());
  std::vector<glm::vec3> norm_vec(verts.size());
  for (size_t i = 0; i < verts.size(); i++) {
    norm_vec[i] = verts[i];
    pos_vec[i] = (verts[i] * radius) + center;
  }

  return MoveFromBuffers(std::move(pos_vec), std::move(norm_vec), std::move(indices));
}

Geom Geom::MakeCylinder(glm::vec3 start, glm::vec3 end, float radius,
                        int num_circle_subdivs, int num_length_subdivs) {
  glm::vec3 axis = end - start;
  float length = glm::length(axis);
  glm::vec3 axis_dir = axis / length;

  // Build an orthonormal basis around the cylinder axis.
  glm::vec3 up = (std::abs(glm::dot(axis_dir, glm::vec3(0, 1, 0))) < 0.99f)
                     ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
  glm::vec3 u = glm::normalize(glm::cross(axis_dir, up));
  glm::vec3 v = glm::cross(axis_dir, u);

  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> normals;
  std::vector<uint32_t> indices;

  // Generate rings along the length.
  int num_rings = num_length_subdivs + 1;
  for (int ring = 0; ring <= num_rings; ring++) {
    float t = static_cast<float>(ring) / static_cast<float>(num_rings);
    glm::vec3 center = start + axis * t;
    for (int seg = 0; seg <= num_circle_subdivs; seg++) {
      float angle = 2.0f * glm::pi<float>() * static_cast<float>(seg) / static_cast<float>(num_circle_subdivs);
      glm::vec3 normal = u * std::cos(angle) + v * std::sin(angle);
      positions.push_back(center + normal * radius);
      normals.push_back(normal);
    }
  }

  // Build triangle indices between adjacent rings.
  int verts_per_ring = num_circle_subdivs + 1;
  for (int ring = 0; ring < num_rings; ring++) {
    for (int seg = 0; seg < num_circle_subdivs; seg++) {
      uint32_t curr = ring * verts_per_ring + seg;
      uint32_t next = curr + verts_per_ring;
      indices.insert(indices.end(), {curr, next, curr + 1, curr + 1, next, next + 1});
    }
  }

  // Cap centers.
  uint32_t start_cap_idx = static_cast<uint32_t>(positions.size());
  positions.push_back(start);
  normals.push_back(-axis_dir);
  uint32_t end_cap_idx = static_cast<uint32_t>(positions.size());
  positions.push_back(end);
  normals.push_back(axis_dir);

  // Start cap - duplicate ring 0 verts with cap normal.
  uint32_t start_ring_base = static_cast<uint32_t>(positions.size());
  for (int seg = 0; seg <= num_circle_subdivs; seg++) {
    float angle = 2.0f * glm::pi<float>() * static_cast<float>(seg) / static_cast<float>(num_circle_subdivs);
    glm::vec3 offset = u * std::cos(angle) + v * std::sin(angle);
    positions.push_back(start + offset * radius);
    normals.push_back(-axis_dir);
  }
  for (int seg = 0; seg < num_circle_subdivs; seg++) {
    indices.insert(indices.end(), {
        start_cap_idx,
        start_ring_base + static_cast<uint32_t>(seg) + 1,
        start_ring_base + static_cast<uint32_t>(seg) });
  }

  // End cap - duplicate last ring verts with cap normal.
  uint32_t end_ring_base = static_cast<uint32_t>(positions.size());
  for (int seg = 0; seg <= num_circle_subdivs; seg++) {
    float angle = 2.0f * glm::pi<float>() * static_cast<float>(seg) / static_cast<float>(num_circle_subdivs);
    glm::vec3 offset = u * std::cos(angle) + v * std::sin(angle);
    positions.push_back(end + offset * radius);
    normals.push_back(axis_dir);
  }
  for (int seg = 0; seg < num_circle_subdivs; seg++) {
    indices.insert(indices.end(), {
        end_cap_idx,
        end_ring_base + static_cast<uint32_t>(seg),
        end_ring_base + static_cast<uint32_t>(seg) + 1 });
  }

  return MoveFromBuffers(std::move(positions), std::move(normals), std::move(indices));
}

Geom Geom::MakeCone(glm::vec3 start, glm::vec3 end, float radius,
                    int num_circle_subdivs, int num_length_subdivs) {
  glm::vec3 axis = end - start;
  float length = glm::length(axis);
  glm::vec3 axis_dir = axis / length;

  // Build an orthonormal basis around the cone axis.
  glm::vec3 up = (std::abs(glm::dot(axis_dir, glm::vec3(0, 1, 0))) < 0.99f)
                     ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
  glm::vec3 u = glm::normalize(glm::cross(axis_dir, up));
  glm::vec3 v = glm::cross(axis_dir, u);

  // The half-angle of the cone surface, used for normals.
  float slope_angle = std::atan2(radius, length);
  float cos_slope = std::cos(slope_angle);
  float sin_slope = std::sin(slope_angle);

  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> normals;
  std::vector<uint32_t> indices;

  // Generate rings from base (start) to tip (end). Radius tapers linearly to 0.
  int num_rings = num_length_subdivs + 1;
  for (int ring = 0; ring <= num_rings; ring++) {
    float t = static_cast<float>(ring) / static_cast<float>(num_rings);
    glm::vec3 center = start + axis * t;
    float ring_radius = radius * (1.0f - t);
    for (int seg = 0; seg <= num_circle_subdivs; seg++) {
      float angle = 2.0f * glm::pi<float>() * static_cast<float>(seg) / static_cast<float>(num_circle_subdivs);
      glm::vec3 radial = u * std::cos(angle) + v * std::sin(angle);
      // Surface normal points outward and slightly toward the tip.
      glm::vec3 normal = radial * cos_slope + axis_dir * sin_slope;
      positions.push_back(center + radial * ring_radius);
      normals.push_back(normal);
    }
  }

  // Build triangle indices between adjacent rings.
  int verts_per_ring = num_circle_subdivs + 1;
  for (int ring = 0; ring < num_rings; ring++) {
    for (int seg = 0; seg < num_circle_subdivs; seg++) {
      uint32_t curr = ring * verts_per_ring + seg;
      uint32_t next = curr + verts_per_ring;
      indices.insert(indices.end(), {curr, next, curr + 1, curr + 1, next, next + 1});
    }
  }

  // Base cap.
  uint32_t cap_center_idx = static_cast<uint32_t>(positions.size());
  positions.push_back(start);
  normals.push_back(-axis_dir);

  uint32_t cap_ring_base = static_cast<uint32_t>(positions.size());
  for (int seg = 0; seg <= num_circle_subdivs; seg++) {
    float angle = 2.0f * glm::pi<float>() * static_cast<float>(seg) / static_cast<float>(num_circle_subdivs);
    glm::vec3 offset = u * std::cos(angle) + v * std::sin(angle);
    positions.push_back(start + offset * radius);
    normals.push_back(-axis_dir);
  }
  for (int seg = 0; seg < num_circle_subdivs; seg++) {
    indices.insert(indices.end(), {
        cap_center_idx,
        cap_ring_base + static_cast<uint32_t>(seg) + 1,
        cap_ring_base + static_cast<uint32_t>(seg) });
  }
  return MoveFromBuffers(std::move(positions), std::move(normals), std::move(indices));
}

Geom Geom::MakeBoneOctahedron(glm::vec3 start, glm::quat rot, glm::vec3 end, float radius) {
  glm::vec3 axis = end - start;
  glm::vec3 axis_dir = SafeNormalize(axis, glm::vec3(0.0f, 1.0f, 0.0f));
  glm::vec3 center = glm::mix(start, end, 0.25f);

  // Build an orthonormal basis around the primary axis and a secondary vector.
  // Where the secondary vector is the local axis of rot that has the largest
  // angle from the primary axis.
  // This ensures that the faces of the octahedron face toward the local axis of the joint.
  glm::vec3 x = rot * glm::vec3(1.0, 0.0f, 0.0f);
  glm::vec3 y = rot * glm::vec3(0.0, 1.0f, 0.0f);
  glm::vec3 z = rot * glm::vec3(0.0, 0.0f, 1.0f);
  float dx = std::abs(glm::dot(glm::normalize(x), axis_dir));
  float dy = std::abs(glm::dot(glm::normalize(y), axis_dir));
  float dz = std::abs(glm::dot(glm::normalize(z), axis_dir));
  glm::vec3 secondary;
  if (dx <= dy && dx <= dz) {
    secondary = x;
  } else if (dy <= dx && dy <= dz) {
    secondary = y;
  } else {
    secondary = z;
  }
  glm::vec3 u = glm::normalize(glm::cross(axis_dir, secondary));
  glm::vec3 v = glm::cross(axis_dir, u);

  // 6 vertices: start tip, end tip, and 4 middle ring vertices.
  glm::vec3 verts[6] = {
      start,                       // 0: start tip
      end,                         // 1: end tip
      center + (u + v) * radius,   // 2: u+v
      center + (-u + v) * radius,  // 3: -u+v
      center + (-u - v) * radius,  // 4: -u-v
      center + (u - v) * radius,   // 5: u-v
  };

  // 8 triangular faces. Each face gets its own 3 vertices with a flat normal.
  // Start-half faces: 0-3-2, 0-4-3, 0-5-4, 0-2-5
  // End-half faces:   1-2-3, 1-3-4, 1-4-5, 1-5-2
  uint32_t faces[8][3] = {
      {0, 3, 2}, {0, 4, 3}, {0, 5, 4}, {0, 2, 5},
      {1, 2, 3}, {1, 3, 4}, {1, 4, 5}, {1, 5, 2},
  };

  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> normals;
  std::vector<uint32_t> indices;
  positions.reserve(24);
  normals.reserve(24);
  indices.reserve(24);

  for (int f = 0; f < 8; f++) {
    glm::vec3 a = verts[faces[f][0]];
    glm::vec3 b = verts[faces[f][1]];
    glm::vec3 c = verts[faces[f][2]];
    glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
    uint32_t base = static_cast<uint32_t>(positions.size());
    positions.push_back(a);
    positions.push_back(b);
    positions.push_back(c);
    normals.push_back(normal);
    normals.push_back(normal);
    normals.push_back(normal);
    indices.insert(indices.end(), {base, base + 1, base + 2});
  }
  return MoveFromBuffers(std::move(positions), std::move(normals), std::move(indices));
}

}  // namespace hyper

