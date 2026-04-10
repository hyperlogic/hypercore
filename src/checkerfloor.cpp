/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for
    more details.
*/

#include "src/checkerfloor.h"

#include <memory>
#include <vector>

#include "src/glincludes.h"
#include "src/image.h"
#include "src/program.h"
#include "src/texture.h"
#include "src/vertexbuffer.h"

const float kFloorRadius = 100.0f;
const float kFloorTileCount = 50.0f;

namespace hyper {

CheckerFloor::CheckerFloor(const glm::mat4& floor_mat_in) :
    floor_mat_(floor_mat_in) {
}

bool CheckerFloor::Init(bool is_framebuffer_srgb_enabled_in) {
  is_framebuffer_srgb_enabled_ = is_framebuffer_srgb_enabled_in;

  Image floor_img;
  if (!floor_img.Load("texture/checkerboard-gray.png")) {
    Log::E("Error loading checkerboard-gray.png\n");
    return false;
  }
  floor_img.is_srgb = is_framebuffer_srgb_enabled_in;
  Texture::Params tex_params = {FilterType::LinearMipmapLinear,
                                 FilterType::Linear,
                                 WrapType::Repeat,
                                 WrapType::Repeat};
  floor_tex_ = std::make_shared<Texture>(floor_img, tex_params);

  floor_prog_ = std::make_shared<Program>();
  if (!floor_prog_->LoadVertFrag("shader/floor_vert.glsl",
                                 "shader/floor_frag.glsl")) {
    Log::E("Error loading floor shaders!\n");
    return false;
  }

  floor_vao_ = std::make_shared<VertexArrayObject>();
  std::vector<glm::vec3> pos_vec = {
    glm::vec3(-kFloorRadius, 0.0f, -kFloorRadius),
    glm::vec3(kFloorRadius, 0.0f, -kFloorRadius),
    glm::vec3(kFloorRadius, 0.0f, kFloorRadius),
    glm::vec3(-kFloorRadius, 0.0f, kFloorRadius)
  };
  auto pos_buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, pos_vec);

  std::vector<glm::vec2> uv_vec = {
    glm::vec2(0.0f, 0.0f),
    glm::vec2(kFloorTileCount, 0.0f),
    glm::vec2(kFloorTileCount, kFloorTileCount),
    glm::vec2(0.0f, kFloorTileCount)
  };
  auto uv_buffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, uv_vec);

  // build element array
  std::vector<uint32_t> index_vec = {
    0, 2, 1,
    0, 3, 2
  };
  auto index_buffer = std::make_shared<BufferObject>(GL_ELEMENT_ARRAY_BUFFER,
                                                      index_vec);

  // setup vertex array object with buffers
  floor_vao_->SetAttribBuffer(floor_prog_->GetAttribLoc("position"),
                              pos_buffer);
  floor_vao_->SetAttribBuffer(floor_prog_->GetAttribLoc("uv"), uv_buffer);
  floor_vao_->SetElementBuffer(index_buffer);

  return true;
}

void CheckerFloor::Render(const RenderParams& render_params) {
  floor_prog_->Bind();

  glm::mat4 model_view_mat = glm::inverse(render_params.camera_mat) * floor_mat_;
  floor_prog_->SetUniform("modelViewProjMat", render_params.proj_mat * model_view_mat);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, floor_tex_->texture);
  floor_prog_->SetUniform("colorTex", 0);
  floor_vao_->DrawElements(GL_TRIANGLES);
}

}  // namespace hyper
