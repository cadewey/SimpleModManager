//
// Created by Adrien BLANCHET on 13/02/2020.
//

#include <toolbox.h>
#include <mods_preseter.h>
#include <fstream>
//#include <switch/services/applet.h>
#include <switch.h>
#include <selector.h>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>

mods_preseter::mods_preseter() {
  reset();
}
mods_preseter::~mods_preseter() {
  reset();
}

void mods_preseter::initialize() {

}
void mods_preseter::reset() {

  _selected_mod_preset_index_ = -1;
  _mod_folder_ = "";
  _preset_file_path_ = "";
  _presets_list_.clear();
  _data_handler_.clear();

}

int mods_preseter::get_selected_mod_preset_index(){
  return _selected_mod_preset_index_;
}
std::string mods_preseter::get_selected_mod_preset(){
  if(_selected_mod_preset_index_ >= 0 and _selected_mod_preset_index_ < int(_presets_list_.size())){
    return _presets_list_[_selected_mod_preset_index_];
  } else{
    return "";
  }
}
std::vector<std::string> mods_preseter::get_mods_list(std::string preset_) {
  return _data_handler_[preset_];
}
std::vector<std::string>& mods_preseter::get_presets_list(){
  return _presets_list_;
}

void mods_preseter::read_parameter_file(std::string mod_folder_) {

  reset();
  _mod_folder_ = mod_folder_;
  _preset_file_path_ = mod_folder_ + "/mod_presets.conf";

  // check if file exist
  auto lines = toolbox::dump_file_as_vector_string(_preset_file_path_);
  std::string current_preset = "";
  for(auto &line : lines){
    if(line[0] == '#') continue;

    auto line_elements = toolbox::split_string(line, "=");
    if(line_elements.size() != 2) continue;

    // clean up for extra spaces characters
    for(auto &element : line_elements){
      while(element[0] == ' '){
        element.erase(element.begin());
      }
      while(element[element.size()-1] == ' '){
        element.erase(element.end()-1);
      }
    }

    if(line_elements[0] == "preset"){
      current_preset = line_elements[1];
      if(_selected_mod_preset_index_ == -1) _selected_mod_preset_index_ = 0;
      if (not toolbox::do_string_in_vector(current_preset, _presets_list_)){
        _presets_list_.emplace_back(current_preset);
      }
    } else {
      _data_handler_[current_preset].emplace_back(line_elements[1]);
    }
  }

}
void mods_preseter::recreate_preset_file() {

  std::stringstream ss;

  ss << "# This is a config file" << std::endl;
  ss << std::endl;
  ss << std::endl;

  for(auto const &preset : _presets_list_){
    ss << "########################################" << std::endl;
    ss << "# mods preset name" << std::endl;
    ss << "preset = " << preset << std::endl;
    ss << std::endl;
    ss << "# mods list" << std::endl;
    for(int i_entry = 0 ; i_entry < int(_data_handler_[preset].size()) ; i_entry++){
      ss << "mod" << i_entry << " = " << _data_handler_[preset][i_entry] << std::endl;
    }
    ss << "########################################" << std::endl;
    ss << std::endl;
  }

  std::string data = ss.str();
  toolbox::dump_string_in_file(data, _preset_file_path_);

}
void mods_preseter::select_mod_preset() {

  selector sel = fill_selector();

  bool is_first_loop = true;
  while(appletMainLoop()){

    hidScanInput();
    u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);
    u64 kHeld = hidKeysHeld(CONTROLLER_P1_AUTO);
    sel.scan_inputs(kDown, kHeld);
    if(kDown & KEY_B){
      break;
    }
    else if(kDown & KEY_A and not _presets_list_.empty()){
      _selected_mod_preset_index_ = sel.get_selected_entry();
      return;
    }
    else if(kDown & KEY_X and not _presets_list_.empty()){
      std::string answer = toolbox::ask_question(
        "Are you sure you want to remove this preset ?",
        std::vector<std::string>({"Yes","No"})
        );
      if(answer == "No") continue;
      _data_handler_[_presets_list_[sel.get_selected_entry()]].resize(0);
      _presets_list_.erase(_presets_list_.begin() + sel.get_selected_entry());
      sel = fill_selector();
      recreate_preset_file();
      read_parameter_file(_mod_folder_);
    }
    else if(kDown & KEY_PLUS){
      create_new_preset();
      sel = fill_selector();
      recreate_preset_file();
      read_parameter_file(_mod_folder_);
    }
    else if(kDown & KEY_Y){
      edit_preset(sel.get_selected_string(), _data_handler_[sel.get_selected_string()]);
      sel = fill_selector();
      recreate_preset_file();
      read_parameter_file(_mod_folder_);
    }

    if(kDown != 0 or kHeld != 0 or is_first_loop){
      is_first_loop = false;
      consoleClear();
      toolbox::print_right("SimpleModManager v"+toolbox::get_app_version());
      std::cout << toolbox::red_bg << std::setw(toolbox::get_terminal_width()) << std::left;
      std::cout << "Select mod preset" << toolbox::reset_color;
      std::cout << toolbox::repeat_string("*",toolbox::get_terminal_width());
      sel.print_selector();
      std::cout << toolbox::repeat_string("*",toolbox::get_terminal_width());
      toolbox::print_left("  Page (" + std::to_string(sel.get_current_page()+1) + "/" + std::to_string(sel.get_nb_pages()) + ")");
      std::cout << toolbox::repeat_string("*",toolbox::get_terminal_width());
      toolbox::print_left_right(" A : Select mod preset", " X : Delete mod preset ");
      toolbox::print_left_right(" Y : Edit preset", "+ : Create a preset ");
      toolbox::print_left(" B : Go back");
      consoleUpdate(nullptr);
    }

  }
}
void mods_preseter::create_new_preset(){

  int preset_id = 1;
  std::string default_preset_name;
  do{
    default_preset_name = "preset-" + std::to_string(_presets_list_.size()+preset_id);
    preset_id++;
  } while(toolbox::do_string_in_vector(default_preset_name, _presets_list_));

  std::vector<std::string> selected_mods_list;
  edit_preset(default_preset_name, selected_mods_list);

}
void mods_preseter::edit_preset(std::string preset_name_, std::vector<std::string> selected_mods_list_) {

  std::vector<std::string> mods_list = toolbox::get_list_of_subfolders_in_folder(_mod_folder_);
  std::sort(mods_list.begin(), mods_list.end());
  selector sel;
  sel.set_selection_list(mods_list);

  for(int i_entry = 0 ; i_entry < int(selected_mods_list_.size()) ; i_entry++){
    for(int j_entry = 0 ; j_entry < int(mods_list.size()) ; j_entry++){

      if(selected_mods_list_[i_entry] == mods_list[j_entry]){
        std::string new_tag = sel.get_tag(j_entry);
        if(not new_tag.empty()) new_tag += " & ";
        new_tag += "#" + std::to_string(i_entry);
        sel.set_tag(j_entry, new_tag);
        break;
      }

    }
  }

  bool is_first_loop = true;
  while(appletMainLoop()){

    hidScanInput();
    u64 kDown = hidKeysDown(CONTROLLER_P1_AUTO);
    u64 kHeld = hidKeysHeld(CONTROLLER_P1_AUTO);
    sel.scan_inputs(kDown, kHeld);
    if(kDown & KEY_A){
      selected_mods_list_.emplace_back(sel.get_selected_string());
      std::string new_tag = sel.get_tag(sel.get_selected_entry());
      if(not new_tag.empty()) new_tag += " & ";
      new_tag += "#" + std::to_string(selected_mods_list_.size());
      sel.set_tag(sel.get_selected_entry(), new_tag);
    } else if(kDown & KEY_X){
      for(int i_entry = int(selected_mods_list_.size()) - 1 ; i_entry >= 0 ; i_entry--){
        if(sel.get_selected_string() == selected_mods_list_[i_entry]){
          selected_mods_list_.erase(selected_mods_list_.begin() + i_entry);

          // reprocessing all tags
          for(int j_mod = 0 ; j_mod < int(mods_list.size()) ; j_mod++){
            sel.set_tag(j_mod, "");
          }
          for(int j_entry = 0 ; j_entry < int(selected_mods_list_.size()) ; j_entry++){
            for(int j_mod = 0 ; j_mod < int(mods_list.size()) ; j_mod++){
              if(selected_mods_list_[j_entry] == mods_list[j_mod]){
                std::string new_tag = sel.get_tag(j_mod);
                if(not new_tag.empty()) new_tag += " & ";
                new_tag += "#" + std::to_string(j_entry+1);
                sel.set_tag(j_mod, new_tag);
              }
            }
          }
          break; // for loop
        }
      }
    } else if(kDown & KEY_PLUS){
      break;
    } else if(kDown & KEY_B){
      return;
    }

    if(kDown != 0 or kHeld != 0 or is_first_loop){
      consoleClear();
      toolbox::print_right("SimpleModManager v"+toolbox::get_app_version());
      std::cout << toolbox::red_bg << std::setw(toolbox::get_terminal_width()) << std::left;
      std::string header_title = "Creating preset : " + preset_name_ + ". Select the mods you want.";
      std::cout << header_title << toolbox::reset_color;
      std::cout << toolbox::repeat_string("*",toolbox::get_terminal_width());
      sel.print_selector();
      std::cout << toolbox::repeat_string("*",toolbox::get_terminal_width());
      toolbox::print_left_right(" A : Add mod", "X : Cancel mod ");
      toolbox::print_left_right(" + : SAVE", "B : Abort / Go back ");
      consoleUpdate(nullptr);
    }
  }

  _data_handler_[preset_name_].clear();
  _data_handler_[preset_name_].resize(0);

  int preset_index = -1;
  for(int i_index = 0 ; i_index < int(_presets_list_.size()) ; i_index++){
    if(_presets_list_[i_index] == preset_name_) preset_index = i_index;
  }

  if(preset_index == -1){
    preset_index = _presets_list_.size();
    _presets_list_.emplace_back(preset_name_);
  }


  preset_name_ = toolbox::get_user_string(preset_name_);
  _presets_list_[preset_index] = preset_name_;

  for(int i_entry = 0 ; i_entry < int(selected_mods_list_.size()) ; i_entry++){
    _data_handler_[preset_name_].emplace_back(selected_mods_list_[i_entry]);
  }


}

void mods_preseter::select_previous_mod_preset(){
  if(_selected_mod_preset_index_ == -1) return;
  _selected_mod_preset_index_--;
  if(_selected_mod_preset_index_ < 0) _selected_mod_preset_index_ = int(_presets_list_.size()) - 1;
}
void mods_preseter::select_next_mod_preset(){
  if(_selected_mod_preset_index_ == -1) return;
  _selected_mod_preset_index_++;
  if(_selected_mod_preset_index_ >= int(_presets_list_.size())) _selected_mod_preset_index_ = 0;
}

selector mods_preseter::fill_selector(){
  selector sel;
  if(not _presets_list_.empty()){
    sel.set_selection_list(_presets_list_);
    for(int i_preset = 0 ; i_preset < int(_presets_list_.size()) ; i_preset++){
      std::vector<std::string> description_lines;
      for(int i_entry = 0 ; i_entry < int(_data_handler_[_presets_list_[i_preset]].size()) ; i_entry++){
        description_lines.emplace_back("  | " + _data_handler_[_presets_list_[i_preset]][i_entry]);
      }
      sel.set_description(i_preset, description_lines);
    }
  }
  else {
    std::vector<std::string> empty_list;
    empty_list.emplace_back("NO MODS PRESETS");
    sel.set_selection_list(empty_list);
  }
  return sel;
}
