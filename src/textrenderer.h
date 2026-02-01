/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace hyper {

class Program;
class Texture;
class VertexArrayObject;

class TextRenderer {
 public:
  TextRenderer();

  bool Init(const std::string& font_json_filename,
            const std::string& font_png_filename);

  // viewport = (x, y, width, height)
  void Render(const glm::mat4& camera_mat, const glm::mat4& proj_mat,
              const glm::vec4& viewport, const glm::vec2& near_far);

  using TextKey = uint32_t;

  // creates a new text and adds it to the scene
  TextKey AddWorldText(const glm::mat4 xform, const glm::vec4& color,
                       float line_height, const std::string& ascii_string);
  TextKey AddScreenText(const glm::ivec2& pos,
                        int num_rows,
                        const glm::vec4& color,
                        const std::string& ascii_string);
  TextKey AddScreenTextWithDropShadow(const glm::ivec2& pos,
                                      int num_rows,
                                      const glm::vec4& color,
                                      const glm::vec4& shadow_color,
                                      const std::string& ascii_string);
  void SetTextXform(TextKey key, const glm::mat4 xform);

  // removes text object form the scene
  void RemoveText(TextKey key);

 protected:
  struct Glyph {
    glm::vec2 xy_min;
    glm::vec2 xy_max;
    glm::vec2 uv_min;
    glm::vec2 uv_max;
    glm::vec2 advance;
  };

  struct Text {
    glm::mat4 xform;
    std::shared_ptr<VertexArrayObject> vao;
    size_t num_quads;
    bool is_screen_aligned;
  };

  void BuildText(Text& text,
                 const glm::vec3& pen,
                 float line_height,
                 const glm::vec4& color,
                 const std::string& ascii_string) const;
  TextKey AddScreenTextImpl(const glm::ivec2& pos, int num_rows,
                            const glm::vec4& color,
                            const std::string& ascii_string,
                            bool add_drop_shadow,
                            const glm::vec4& shadow_color);

  std::unordered_map<uint8_t, Glyph> glyph_map_;
  float texture_width_;
  std::shared_ptr<Program> text_prog_;
  std::shared_ptr<Texture> font_tex_;
  std::unordered_map<uint32_t, Text> text_map_;
  Glyph space_glyph_;
};

}  // namespace hyper
