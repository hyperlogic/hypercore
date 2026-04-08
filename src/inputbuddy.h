/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <stdint.h>
#include <string.h>

#include <functional>
#include <map>
#include <utility>

#include <glm/glm.hpp>

union SDL_Event;
struct SDL_JoyAxisEvent;
struct SDL_JoyHatEvent;
struct SDL_JoyButtonEvent;

namespace hyper {

class InputBuddy {
 public:
  using Keycode = int32_t;

  InputBuddy();

  using VoidCallback = std::function<void()>;
  using KeyCallback = std::function<void(bool, uint16_t)>;
  using ResizeCallback = std::function<void(int, int)>;
  // button 1 = LEFT, 2 = MIDDLE, 3 = RIGHT
  using MouseButtonCallback = std::function<void(uint8_t, bool, glm::ivec2)>;
  using MouseMotionCallback = std::function<void(glm::ivec2, glm::ivec2)>;

  void ProcessEvent(const SDL_Event& event);

  uint32_t SetOnKey(Keycode key, const KeyCallback& cb);
  void ClearOnKey(uint32_t id);
  uint32_t SetOnQuit(const VoidCallback& cb);
  void ClearOnQuit(uint32_t id);
  uint32_t SetOnResize(const ResizeCallback& cb);
  void ClearOnResize(uint32_t id);
  uint32_t SetOnMouseButton(const MouseButtonCallback& cb);
  void ClearOnMouseButton(uint32_t id);
  uint32_t SetOnMouseMotion(const MouseMotionCallback& cb);
  void ClearOnMouseMotion(uint32_t id);

  void SetRelativeMouseMode(bool val);

  // based on an xbox controler
  class Joypad {
   public:
    Joypad() {
      memset(this, 0, sizeof(Joypad));
    }

    glm::vec2 left_stick;
    glm::vec2 right_stick;
    float left_trigger;
    float right_trigger;
    bool down:1;
    bool up:1;
    bool left:1;
    bool right:1;
    bool view:1;
    bool menu:1;
    bool rs:1;
    bool ls:1;
    bool lb:1;
    bool rb:1;
    bool a:1;
    bool b:1;
    bool x:1;
    bool y:1;
  };

  const Joypad& GetJoypad() const { return joypad_; }

 protected:
  void UpdateJoypadAxis(const SDL_JoyAxisEvent& event);
  void UpdateJoypadHat(const SDL_JoyHatEvent& event);
  void UpdateJoypadButton(const SDL_JoyButtonEvent& event);

  Joypad joypad_;
  uint32_t next_callback_id_ = 1;
  std::map<uint32_t, std::pair<Keycode, KeyCallback>> key_callback_map_;
  std::map<uint32_t, VoidCallback> quit_callback_map_;
  std::map<uint32_t, ResizeCallback> resize_callback_map_;
  std::map<uint32_t, MouseButtonCallback> mouse_button_callback_map_;
  std::map<uint32_t, MouseMotionCallback> mouse_motion_callback_map_;
};

}  // namespace hyper
