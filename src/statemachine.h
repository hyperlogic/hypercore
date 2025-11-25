/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "core/log.h"

namespace hyper {

template <typename State>
class StateMachine {
 public:
  using ProcessCallback = std::function<void(float)>;
  using VoidCallback = std::function<void(void)>;
  using BoolCallback = std::function<bool(void)>;

 protected:
  struct TransitionStruct {
    TransitionStruct(const BoolCallback& cb_in, State state_in,
                     std::string name_in) :
        cb(cb_in), state(state_in), name(name_in) {}
    BoolCallback cb;
    State state;
    std::string name;
  };
  struct StateStruct {
    StateStruct(const VoidCallback& enter_in, const VoidCallback& exit_in,
                const ProcessCallback& process_in) :
        enter(enter_in), exit(exit_in), process(process_in) {}
    VoidCallback enter;
    VoidCallback exit;
    ProcessCallback process;
    std::vector<TransitionStruct> transition_vec;
  };
  using StatePair = std::pair<State, StateStruct>;

 public:
  explicit StateMachine(State default_state) :
      state_(default_state), debug_(false) {}

  void AddState(State state, const std::string& name,
                const VoidCallback& enter, const VoidCallback& exit,
                const ProcessCallback& process) {
    StatePair sp(state, StateStruct(enter, exit, process));
    state_struct_map_.insert(sp);
    state_name_map_.insert(std::pair<State, std::string>(state, name));
  }

  void AddTransition(State state, State new_state, const std::string& name,
                     const BoolCallback& transition_cb) {
    state_struct_map_.at(state).transition_vec.push_back(
        TransitionStruct(transition_cb, new_state, name));
  }

  void Process(float dt) {
    for (auto&& trans : state_struct_map_.at(state_).transition_vec) {
      if (trans.cb()) {
        ChangeState(trans.state, trans.name);
      }
    }
    state_struct_map_.at(state_).process(dt);
  }

  void ChangeState(State new_state, const std::string& reason) {
    if (debug_) {
      Log::D("StateChange from %s -> %s, (%s)\n",
             state_name_map_.at(state_).c_str(),
             state_name_map_.at(new_state).c_str(),
             reason.c_str());
    }
    state_struct_map_.at(state_).exit();
    state_struct_map_.at(new_state).enter();
    state_ = new_state;
  }

  void SetDebug(bool debug_in) { debug_ = debug_in; }

 protected:
  State state_;
  std::map<State, StateStruct> state_struct_map_;
  std::map<State, std::string> state_name_map_;
  bool debug_;
};

}  // namespace hyper
