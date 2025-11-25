/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "src/textrenderer.h"

#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "src/glincludes.h"

#include "src/image.h"
#include "src/log.h"
#include "src/util.h"
#include "src/program.h"
#include "src/texture.h"
#include "src/vertexbuffer.h"

namespace hyper {

const int kTabSize = 4;
static TextRenderer::TextKey next_key = 1;

TextRenderer::TextRenderer() {}

bool TextRenderer::Init(const std::string& font_json_filename,
                        const std::string& font_png_filename) {
  std::ifstream f(GetRootPath() + font_json_filename);
  if (f.fail()) {
    return false;
  }

  try {
    nlohmann::json j = nlohmann::json::parse(f);
    texture_width_ = j["texture_width"].template get<float>();
    nlohmann::json metrics = j["glyph_metrics"];
    for (auto& iter : metrics.items()) {
      int key = iter.value()["ascii_index"].template get<int>();
      if (key < 0 && key > std::numeric_limits<uint8_t>::max()) {
        Log::W("TextRenderer(%s) glyph %d is out of range\n",
               font_json_filename.c_str(), key);
        continue;
      }
      Glyph g;
      nlohmann::json v2 = iter.value()["xy_lower_left"];
      g.xy_min = glm::vec2(v2[0].template get<float>(),
                           v2[1].template get<float>());
      v2 = iter.value()["xy_upper_right"];
      g.xy_max = glm::vec2(v2[0].template get<float>(),
                           v2[1].template get<float>());
      v2 = iter.value()["uv_lower_left"];
      g.uv_min = glm::vec2(v2[0].template get<float>(),
                           v2[1].template get<float>());
      v2 = iter.value()["uv_upper_right"];
      g.uv_max = glm::vec2(v2[0].template get<float>(),
                           v2[1].template get<float>());
      v2 = iter.value()["advance"];
      g.advance = glm::vec2(v2[0].template get<float>(),
                            v2[1].template get<float>());

      glyph_map_.insert(std::pair<uint8_t, Glyph>(static_cast<uint8_t>(key),
                                                  g));
    }
    // AJT(TODO) support kerning table, for variable width fonts
  } catch (const nlohmann::json::exception& e) {
    std::string s = e.what();
    Log::E("TextRenderer::Init(%s) exception: %s\n",
           font_json_filename.c_str(), s.c_str());
    return false;
  }

  // find the space_glyph_
  auto g_iter = glyph_map_.find(static_cast<uint8_t>(' '));
  assert(g_iter != glyph_map_.end());
  if (g_iter != glyph_map_.end()) {
    space_glyph_ = g_iter->second;
  }

  Image font_img;
  if (!font_img.Load(font_png_filename)) {
    Log::E("Error loading fontPng\n");
    return false;
  }

#if __APPLE__
  // opengles doesn't support LUMINANCE_ALPHA textures
  font_img.ConvertToRGBA();
#endif

  // AJT(TODO): get gamma correct.
  // font_img.is_srgb = isFramebufferSRGBEnabled;

  Texture::Params tex_params = {FilterType::LinearMipmapLinear,
                                FilterType::Linear,
                                WrapType::ClampToEdge,
                                WrapType::ClampToEdge};
  font_tex_ = std::make_shared<Texture>(font_img, tex_params);

  text_prog_ = std::make_shared<Program>();
  if (!text_prog_->LoadVertFrag("shader/text_vert.glsl",
                                "shader/text_frag.glsl")) {
    Log::E("Error loading TextRenderer shader!\n");
    return false;
  }

  return true;
}

void TextRenderer::Render(const glm::mat4& camera_mat,
                          const glm::mat4& proj_mat,
                          const glm::vec4& viewport,
                          const glm::vec2& near_far) {
  text_prog_->Bind();

  // use texture unit 0 for fontTexture
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, font_tex_->texture);
  text_prog_->SetUniform("fontTex", 0);

  glm::mat4 view_proj_mat = proj_mat * glm::inverse(camera_mat);
  float aspect = viewport.w / viewport.z;
  glm::mat4 aspect_mat = MakeMat4(glm::vec3(aspect, 1.0f, 1.0f),
                                  glm::quat(),
                                  glm::vec3(-aspect / aspect, 0.0f, 0.0f));
  for (auto&& t_iter : text_map_) {
    if (t_iter.second.is_screen_aligned) {
      text_prog_->SetUniform("modelViewProjMat",
                             aspect_mat * t_iter.second.xform);
    } else {
      text_prog_->SetUniform("modelViewProjMat",
                             view_proj_mat * t_iter.second.xform);
    }

    t_iter.second.vao->Bind();
    glDrawArrays(GL_TRIANGLES, 0,
                 static_cast<GLsizei>(t_iter.second.num_quads * 6));
    t_iter.second.vao->Unbind();
  }
}

// creates a new text and adds it to the scene
TextRenderer::TextKey TextRenderer::AddWorldText(
    const glm::mat4 xform,
    const glm::vec4& color,
    float line_height,
    const std::string& ascii_string) {
  Text text;
  text.xform = xform;
  text.is_screen_aligned = false;

  glm::vec3 pen(0.0f, 0.0f, 0.0f);
  BuildText(text, pen, line_height, color, ascii_string);

  uint32_t text_key = next_key++;
  text_map_.insert(std::pair<uint32_t, Text>(text_key, text));

  return text_key;
}

TextRenderer::TextKey TextRenderer::AddScreenText(
    const glm::ivec2& pos,
    int num_rows,
    const glm::vec4& color,
    const std::string& ascii_string) {
  const bool add_drop_shadow = false;
  return AddScreenTextImpl(pos, num_rows, color, ascii_string,
                           add_drop_shadow, glm::vec4());
}

TextRenderer::TextKey TextRenderer::AddScreenTextWithDropShadow(
    const glm::ivec2& pos,
    int num_rows,
    const glm::vec4& color,
    const glm::vec4& shadow_color,
    const std::string& ascii_string) {
  const bool add_drop_shadow = true;
  return AddScreenTextImpl(pos, num_rows, color, ascii_string,
                           add_drop_shadow, shadow_color);
}

void TextRenderer::SetTextXform(TextKey key, const glm::mat4 xform) {
  auto t_iter = text_map_.find(key);
  if (t_iter != text_map_.end()) {
    t_iter->second.xform = xform;
  }
}

// removes text object form the scene
void TextRenderer::RemoveText(TextKey key) {
  text_map_.erase(key);
}

void TextRenderer::BuildText(Text& text, const glm::vec3& pen,
                             float line_height, const glm::vec4& color,
                             const std::string& ascii_string) const {
  std::vector<glm::vec3> pos_vec;
  std::vector<glm::vec2> uv_vec;
  std::vector<glm::vec4> color_vec;

  bool add_drop_shadow = false;
  size_t vec_size = add_drop_shadow ?
                    (ascii_string.size() * 6 * 2) :
                    (ascii_string.size() * 6);

  pos_vec.reserve(vec_size);
  uv_vec.reserve(vec_size);
  color_vec.reserve(vec_size);

  int r = 0;
  int c = 0;
  glm::vec2 penxy = pen;
  float depth = pen.z;
  for (auto& ch : ascii_string) {
    if (ch == ' ') {
      penxy += line_height * space_glyph_.advance;
      c++;
    } else if (ch == '\n') {
      penxy = line_height * glm::vec2(0.0f, static_cast<float>(-(r + 1)));
      r++;
    } else if (ch == '\t') {
      int num_spaces = kTabSize - (c % kTabSize);
      penxy += line_height * static_cast<float>(num_spaces) *
               space_glyph_.advance;
      c += num_spaces;
    } else {
      auto g_iter = glyph_map_.find(static_cast<uint8_t>(ch));
      if (g_iter == glyph_map_.end()) {
        continue;
      }
      const Glyph& g = g_iter->second;

      pos_vec.push_back(glm::vec3(penxy + line_height * g.xy_min, depth));
      pos_vec.push_back(glm::vec3(penxy + line_height * g.xy_max, depth));
      pos_vec.push_back(glm::vec3(penxy + line_height *
                                  glm::vec2(g.xy_min.x, g.xy_max.y), depth));
      pos_vec.push_back(glm::vec3(penxy + line_height * g.xy_min, depth));
      pos_vec.push_back(glm::vec3(penxy + line_height *
                                  glm::vec2(g.xy_max.x, g.xy_min.y), depth));
      pos_vec.push_back(glm::vec3(penxy + line_height * g.xy_max, depth));

      uv_vec.push_back(g.uv_min);
      uv_vec.push_back(g.uv_max);
      uv_vec.push_back(glm::vec2(g.uv_min.x, g.uv_max.y));
      uv_vec.push_back(g.uv_min);
      uv_vec.push_back(glm::vec2(g.uv_max.x, g.uv_min.y));
      uv_vec.push_back(g.uv_max);

      for (int i = 0; i < 6; i++) {
        color_vec.push_back(color);
      }

      penxy += line_height * g.advance;
      c++;
    }
  }

  auto vao = std::make_shared<VertexArrayObject>();
  auto pos_buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, pos_vec);
  auto uv_buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, uv_vec);
  auto color_buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER,
                                                     color_vec);
  vao->SetAttribBuffer(text_prog_->GetAttribLoc("position"), pos_buffer);
  vao->SetAttribBuffer(text_prog_->GetAttribLoc("uv"), uv_buffer);
  vao->SetAttribBuffer(text_prog_->GetAttribLoc("color"), color_buffer);
  text.vao = vao;
  text.num_quads = pos_vec.size() / 6;
}

TextRenderer::TextKey TextRenderer::AddScreenTextImpl(
    const glm::ivec2& pos,
    int num_rows,
    const glm::vec4& color,
    const std::string& ascii_string,
    bool add_drop_shadow,
    const glm::vec4& shadow_color) {
  const float kTextLineHeight = 2.0f / num_rows;
  glm::vec3 origin(0.1f * kTextLineHeight,
                   1.0f - 0.75f * kTextLineHeight, 0.0f);
  glm::vec3 offset(
      static_cast<float>(pos.x) * space_glyph_.advance.x * kTextLineHeight,
      static_cast<float>(pos.y) * -kTextLineHeight, 0.0f);
  Text text;
  text.xform = MakeMat4(glm::quat(), origin + offset);
  text.is_screen_aligned = true;

  // TODO(AJT) Fix dropshadow, need to move this into BuildText
  /*
  if (add_drop_shadow) {
    glm::vec3 shadow_pen = glm::vec3(0.05f * kTextLineHeight,
                                     -0.05f * kTextLineHeight, 0.1f);
    BuildText(text, shadow_pen, kTextLineHeight,
              shadow_color, ascii_string);
  }
  */

  glm::vec3 pen(0.0f, 0.0f, 0.0f);
  BuildText(text, pen, kTextLineHeight, color, ascii_string);

  uint32_t text_key = next_key++;
  text_map_.insert(std::pair<uint32_t, Text>(text_key, text));

  return text_key;
}

}  // namespace hyper
