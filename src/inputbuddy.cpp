/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "src/inputbuddy.h"

#include <SDL2/SDL.h>

#include "src/log.h"

InputBuddy::InputBuddy() {
  int num_joysticks = SDL_NumJoysticks();
  if (num_joysticks > 0) {
    SDL_Joystick* joystick = SDL_JoystickOpen(0);
    Log::I("Found joystick \"%s\"\n", SDL_JoystickName(joystick));
  } else {
    Log::I("No joystick found\n");
  }
}

void InputBuddy::ProcessEvent(const SDL_Event& event) {
  switch (event.type) {
    case SDL_QUIT:
      quit_callback_();
      break;
    case SDL_KEYDOWN:
    case SDL_KEYUP: {
      SDL_Keycode keycode = event.key.keysym.sym;
      uint16_t mod = event.key.keysym.mod;
      bool down = (event.key.type == SDL_KEYDOWN);
      auto iter = key_callback_map_.find(keycode);
      if (iter != key_callback_map_.end() && !event.key.repeat) {
        iter->second(down, mod);
      }
    }
    break;
    case SDL_JOYAXISMOTION:
      UpdateJoypadAxis(event.jaxis);
      break;
    case SDL_JOYHATMOTION:
      UpdateJoypadHat(event.jhat);
      break;
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
      UpdateJoypadButton(event.jbutton);
      break;

    case SDL_WINDOWEVENT:
      if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
        resize_callback_(event.window.data1, event.window.data2);
      }
      break;

    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
      if (event.button.clicks == 1) {
        mouse_button_callback_(event.button.button,
                               event.button.state == SDL_PRESSED,
                               glm::ivec2(event.button.x, event.button.y));
      }
      break;
    case SDL_MOUSEMOTION:
      mouse_motion_callback_(glm::ivec2(event.motion.x, event.motion.y),
                             glm::ivec2(event.motion.xrel, event.motion.yrel));
      break;
  }
}

void InputBuddy::OnKey(Keycode key, const KeyCallback& cb) {
  key_callback_map_.insert(std::pair<Keycode, KeyCallback>(key, cb));
}

void InputBuddy::OnQuit(const VoidCallback& cb) {
  quit_callback_ = cb;
}

void InputBuddy::OnResize(const ResizeCallback& cb) {
  resize_callback_ = cb;
}

void InputBuddy::OnMouseButton(const MouseButtonCallback& cb) {
  mouse_button_callback_ = cb;
}

void InputBuddy::OnMouseMotion(const MouseMotionCallback& cb) {
  mouse_motion_callback_ = cb;
}

void InputBuddy::SetRelativeMouseMode(bool val) {
  SDL_SetRelativeMouseMode(val ? SDL_TRUE : SDL_FALSE);
}

const uint8_t kLeftStickXAxis = 0;
const uint8_t kLeftStickYAxis = 1;
const uint8_t kRightStickXAxis = 2;
const uint8_t kRightStickYAxis = 3;
const uint8_t kLeftTriggerAxis = 4;
const uint8_t kRightTriggerAxis = 5;

static float Deadspot(float v) {
  const float kDeadspot = 0.15f;
  return fabs(v) > kDeadspot ? v : 0.0f;
}

void InputBuddy::UpdateJoypadAxis(const SDL_JoyAxisEvent& event) {
  // only support one joypad
  if (event.which != 0) {
    return;
  }

  const float kAxisMax = static_cast<float>(SDL_JOYSTICK_AXIS_MAX);
  switch (event.axis) {
    case kLeftStickXAxis:
      joypad_.left_stick.x = Deadspot(event.value / kAxisMax);
      break;
    case kLeftStickYAxis:
      joypad_.left_stick.y = Deadspot(-event.value / kAxisMax);
      break;
    case kRightStickXAxis:
      joypad_.right_stick.x = Deadspot(event.value / kAxisMax);
      break;
    case kRightStickYAxis:
      joypad_.right_stick.y = Deadspot(-event.value / kAxisMax);
      break;
    case kLeftTriggerAxis:
      // transform from (-1, 1) to (0, 1)
      joypad_.left_trigger = ((event.value / kAxisMax) * 0.5f) + 0.5f;
      break;
    case kRightTriggerAxis:
      // transform from (-1, 1) to (0, 1)
      joypad_.right_trigger = ((event.value / kAxisMax) * 0.5f) + 0.5f;
      break;
  }
}

const uint8_t kDpadHat = 0;

void InputBuddy::UpdateJoypadHat(const SDL_JoyHatEvent& event) {
  // only support one joypad
  if (event.which != 0) {
    return;
  }

  switch (event.hat) {
    case kDpadHat:
      joypad_.up = (event.value & SDL_HAT_UP) ? true : false;
      joypad_.right = (event.value & SDL_HAT_RIGHT) ? true : false;
      joypad_.down = (event.value & SDL_HAT_DOWN) ? true : false;
      joypad_.left = (event.value & SDL_HAT_LEFT) ? true : false;
      break;
  }
}

const uint8_t kAButton = 0;
const uint8_t kBButton = 1;
const uint8_t kXButton = 2;
const uint8_t kYButton = 3;
const uint8_t kLeftBumperButton = 4;
const uint8_t kRightBumperButton = 5;
const uint8_t kMenuButton = 6;
const uint8_t kViewButton = 7;
const uint8_t kLeftStickButton = 8;
const uint8_t kRightStickButton = 9;

void InputBuddy::UpdateJoypadButton(const SDL_JoyButtonEvent& event) {
  // only support one joypad
  if (event.which != 0) {
    return;
  }

  switch (event.button) {
    case kAButton:
      joypad_.a = event.state ? true : false;
      break;
    case kBButton:
      joypad_.b = event.state ? true : false;
      break;
    case kXButton:
      joypad_.x = event.state ? true : false;
      break;
    case kYButton:
      joypad_.y = event.state ? true : false;
      break;
    case kLeftBumperButton:
      joypad_.lb = event.state ? true : false;
      break;
    case kRightBumperButton:
      joypad_.rb = event.state ? true : false;
      break;
    case kMenuButton:
      joypad_.menu = event.state ? true : false;
      break;
    case kViewButton:
      joypad_.view = event.state ? true : false;
      break;
    case kLeftStickButton:
      joypad_.ls = event.state ? true : false;
      break;
    case kRightStickButton:
      joypad_.rs = event.state ? true : false;
      break;
  }
}
