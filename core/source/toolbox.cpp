//
// Created by Nadrino on 04/09/2019.
//

#include <toolbox.h>

#include <cstring>
#include <cmath>
#include <sys/stat.h>
#include <dirent.h>
#include <iostream>
#include <fstream>
#include <functional>
#include <iomanip> // stringstream
#include <sstream>
#include <filesystem> // cpp 17 functions -> does not work
#include <exception>
#include <chrono>

#include <switch.h>

#include <zlib.h>

#include <version_config.h>
#include <selector.h>
#include <numeric>
#include "GlobalObjects.h"

namespace toolbox{

  static std::time_t _last_timestamp_;
  static double _last_displayed_value_ = -1;
  static bool _CRC_check_is_enabled_ = true;

  static bool _native_switch_FS_is_enabled_ = false;
//  static void *addr;
  static FsFileSystem* _fileSystemBuffer_;

  static std::string _bufferString_;

  static std::map<std::string, double> _progress_map_;
  static std::map<std::string, std::string> _str_buffer_map_;


  //! printout functions :
  void display_loading(int current_index_, int end_index_, std::string title_, std::string prefix_, std::string &color_str_, bool force_display_) {

    int percent = int(round(double(current_index_) / end_index_ * 100.));
    if(_last_displayed_value_ == -1) {
      set_last_timestamp();
    }
    if(
      _last_displayed_value_ == -1 or std::time(nullptr) - _last_timestamp_ >= 1 // every second
      or current_index_ == 0 // first call
      or force_display_ // display every calls
      or current_index_ >= end_index_-1 // last entry
      ){
      set_last_timestamp();
      _last_displayed_value_ = percent;
      std::stringstream ss;
      ss << prefix_ << percent << "% / " << title_;
      print_left(ss.str(), color_str_, true);
      consoleUpdate(nullptr);
    }

  }
  void display_second_level_loading(int current_index_, int end_index_){

  }
  void print_right(std::string input_, std::string color_, bool flush_){
    std::string stripped_input_ = toolbox::remove_color_codes_in_string(input_);
    int nb_space_left = get_terminal_width() - input_.size();
    if(nb_space_left <= 0){ // cut the string
      input_ = input_.substr(0, input_.size() + nb_space_left - int(flush_)); // remove extra char if flush
      stripped_input_ = toolbox::remove_color_codes_in_string(input_);
      nb_space_left = get_terminal_width() - input_.size();
    }
    if(flush_){
      nb_space_left-=1;
      std::cout << "\r";
    }
    std::cout << color_ << repeat_string(" ", nb_space_left) << input_ << reset_color;
    if(flush_) std::cout << "\r";
    else if(int(input_.size()) > get_terminal_width()) std::cout << std::endl;
  }
  void print_left(std::string input_, std::string color_, bool flush_){
    std::string stripped_input_ = toolbox::remove_color_codes_in_string(input_);
    int nb_space_left = get_terminal_width() - stripped_input_.size();
    if(nb_space_left <= 0){ // cut the string
      input_ = input_.substr(0, input_.size() + nb_space_left - int(flush_)); // remove extra char if flush
//     input_ = input_.substr(-nb_space_left, input_.size() + nb_space_left); // NO because can't display the cursor properly
      stripped_input_ = toolbox::remove_color_codes_in_string(input_);
      nb_space_left = get_terminal_width() - stripped_input_.size();
    }
    if(flush_){
      nb_space_left-=1;
      std::cout << "\r";
    }
    std::cout << color_ << input_ << repeat_string(" ", nb_space_left) << reset_color;
    if(flush_) std::cout << "\r";
    else if(int(input_.size()) > get_terminal_width()) std::cout << std::endl;
  }
  void print_left_right(std::string input_left_, std::string input_right_, std::string color_){
    std::string stripped_input_left_ = toolbox::remove_color_codes_in_string(input_left_);
    std::string stripped_input_right_ = toolbox::remove_color_codes_in_string(input_right_);

    int nb_space_left = get_terminal_width() - stripped_input_left_.size() - stripped_input_right_.size();
    if(nb_space_left <= 0){
      input_left_ = input_left_.substr(0, input_left_.size() + nb_space_left);
//      input_left_ = input_left_.substr(-nb_space_left, input_left_.size() + nb_space_left); // NO because can't display the cursor properly
      stripped_input_left_ = toolbox::remove_color_codes_in_string(input_left_);
//      stripped_input_right_ = toolbox::remove_color_codes_in_string(input_right_);
      nb_space_left = get_terminal_width() - stripped_input_left_.size() - stripped_input_right_.size();
    }

    std::cout << color_ << input_left_;
    std::cout << repeat_string(" ", nb_space_left);
    std::cout << input_right_;
    std::cout << reset_color;
    if(get_terminal_width() < int(input_left_.size()) + int(input_right_.size())) std::cout << std::endl;
  }
  void make_pause(){

    std::cout << "PRESS A to continue or + to quit now." << std::endl;
    consoleUpdate(nullptr);

    std::chrono::high_resolution_clock::time_point clock_buffer = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> time_span = clock_buffer - clock_buffer;
    while(appletMainLoop()){
      padUpdate(&GlobalObjects::gPad);;
      u64 kDown = padGetButtonsDown(&GlobalObjects::gPad);
      u64 kHeld = padGetButtons(&GlobalObjects::gPad);
      if (kDown & HidNpadButton_A or (kHeld & HidNpadButton_A and time_span.count() > 100)) {
        break; // break in order to return to hbmenu
      }
      if (kDown & HidNpadButton_Plus) {
        consoleExit(nullptr);
        exit(EXIT_SUCCESS);
      }
      time_span += std::chrono::high_resolution_clock::now() - clock_buffer;
      clock_buffer = std::chrono::high_resolution_clock::now();
    }

  }
  void set_buffer_string(std::string str_){
    _bufferString_ = str_;
  }

  std::string debug_string(std::string str_){

    if(not _native_switch_FS_is_enabled_){
      enableEmbeddedSwitchFS();
    }

    std::stringstream ss;
    ss << str_ << ": ";

    char pathBuffer[FS_MAX_PATH];
    FsDir DirBuffer;
    s64 counter;
    Result rc;

    strcpy(pathBuffer, str_.c_str());
    fsFsOpenDirectory(_fileSystemBuffer_, &pathBuffer[0], FsDirOpenMode_ReadDirs, &DirBuffer);
//    fsDirGetEntryCount(&DirBuffer, &counter);
//    ss << counter+1 << "-> ";
//    counter = 0;

    FsDirectoryEntry dirEntry;

    for(s64 i_count = 0 ; i_count < counter; i_count++){
      rc = fsDirRead(&DirBuffer, &counter, 1, &dirEntry);
      if( R_FAILED(rc) ){
        continue;
      }
      ss<<dirEntry.name<<",";
    }

    fsDirClose(&DirBuffer);

    return ss.str();
  }
  std::string get_file_size_string(std::string& file_path_){
    auto size = get_file_size(file_path_); // in bytes
    return parse_size_unit(size);
  }
  std::string parse_size_unit(unsigned int size_){
    if(size_ / 1000 == 0 ){ // print in bytes
      return std::to_string(size_) + " B";
    }
    size_ = size_ / 1000; // in KB
    if(size_ / 1000 == 0){ // print in KB
      return std::to_string(size_) + " KB";
    }
    size_ = size_ / 1000; // in MB
    if(size_ / 1000 == 0){ // print in MB
      return std::to_string(size_) + " MB";
    }
    size_ = size_ / 1000; // in GB
    return std::to_string(size_) + " GB";
  }
  std::string get_user_string(std::string default_str_) {

    SwkbdConfig kbd;
    Result rc=0;
    rc = swkbdCreate(&kbd, 0);

    char tmpoutstr[32] = {0};

    if (R_SUCCEEDED(rc)) {

      swkbdConfigMakePresetDefault(&kbd);
      swkbdConfigSetInitialText(&kbd, default_str_.c_str());
      rc = swkbdShow(&kbd, tmpoutstr, sizeof(tmpoutstr));
      swkbdClose(&kbd);

    }

    return std::string(tmpoutstr);
  }
  std::string ask_question(std::string question_, std::vector<std::string> answers_,
    std::vector<std::vector<std::string>> descriptions_) {

    std::string answer;
    auto sel = selector();

    int nb_lines_layout = 0;
    nb_lines_layout++; // toolbox::print_right("SimpleModManager v"+toolbox::get_app_version());
    nb_lines_layout++; // std::cout << toolbox::repeat_string("*",toolbox::get_terminal_width());
    nb_lines_layout += int(question_.size()) / toolbox::get_terminal_width();
    nb_lines_layout++; // std::cout << toolbox::repeat_string("*",toolbox::get_terminal_width());
    nb_lines_layout++; // std::cout << toolbox::repeat_string("*",toolbox::get_terminal_width());
    nb_lines_layout++; // toolbox::print_left_right(" A: Select", "B: Back ");
    sel.set_max_items_per_page(toolbox::get_terminal_height() - nb_lines_layout);

    sel.set_selection_list(answers_);
    if(not descriptions_.empty() and descriptions_.size() == answers_.size()){
      sel.set_description_list(descriptions_);
    }

    u64 kDown = 1;
    while(appletMainLoop()){

      if(kDown != 0) {
        consoleClear();
        toolbox::print_right("SimpleModManager v"+toolbox::get_app_version());
        std::cout << toolbox::repeat_string("*",toolbox::get_terminal_width());
        std::cout << question_ << std::endl;
        std::cout << toolbox::repeat_string("*",toolbox::get_terminal_width());
        sel.print_selector();
        std::cout << toolbox::repeat_string("*",toolbox::get_terminal_width());
        toolbox::print_left_right(" A: Select", "B: Back ");
        consoleUpdate(nullptr);
      }

      //Scan all the inputs. This should be done once for each frame
      padUpdate(&GlobalObjects::gPad);

      //hidKeysDown returns information about which buttons have been just pressed (and they weren't in the previous frame)
      kDown = padGetButtonsDown(&GlobalObjects::gPad);

      if(kDown & HidNpadButton_AnyDown){
        sel.increment_cursor_position();
      } else if(kDown & HidNpadButton_AnyUp){
        sel.decrement_cursor_position();
      } else if(kDown & HidNpadButton_A){
        answer = sel.get_selected_string();
        break;
      } else if(kDown & HidNpadButton_B){
        break;
      }

    }
    consoleClear();
    return answer;
  }

  std::string remove_color_codes_in_string(std::string &input_string_){
    std::string stripped_str(input_string_);
    for(auto &color_code : toolbox::colors_list){
      stripped_str = toolbox::replace_substring_in_string(stripped_str, color_code, "");
    }
    return stripped_str;
  }

  //! toolbox vars management functions :
  void reset_last_displayed_value(){
    _last_displayed_value_ = -1;
  }
  void reset_progress_map(){
    _progress_map_.clear();
  }
  void reset_str_buffer_map(){
    _str_buffer_map_.clear();
  }

  void set_last_timestamp(){
    _last_timestamp_ = std::time(nullptr);
  }
  void set_CRC_check_is_enabled(bool CRC_check_is_enabled_){
    _CRC_check_is_enabled_ = CRC_check_is_enabled_;
  }
  void fill_progress_map(std::string key_, double value_){
    _progress_map_[key_] = value_;
  }
  void fill_str_buffer_map(std::string key_, std::string value_){
    _str_buffer_map_[key_] = std::move(value_);
  }

  double & get_progress(std::string key_){
    if(_progress_map_.find(key_) == _progress_map_.end()){
      _progress_map_[key_] = -1;
    }
    return _progress_map_[key_];
  }
  std::string & get_str_buffer(std::string key_){
    if(_str_buffer_map_.find(key_) == _str_buffer_map_.end()){
      _str_buffer_map_[key_] = "";
    }
    return _str_buffer_map_[key_];
  }
  bool get_CRC_check_is_enabled(){
    return _CRC_check_is_enabled_;
  }


  //! generic tools functions
  bool to_bool(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    std::istringstream is(str);
    bool b;
    is >> std::boolalpha >> b;
    return b;
  }
  bool do_string_contains_substring(std::string string_, std::string substring_){
    if(substring_.size() > string_.size()) return false;
    if(string_.find(substring_) != std::string::npos) return true;
    else return false;
  }
  bool do_string_starts_with_substring(std::string string_, std::string substring_){
    if(substring_.size() > string_.size()) return false;
    return (not string_.compare(0, substring_.size(), substring_));
  }
  bool do_string_ends_with_substring(std::string string_, std::string substring_){
    if(substring_.size() > string_.size()) return false;
    return (not string_.compare(string_.size() - substring_.size(), substring_.size(), substring_));
  }
  bool do_string_in_vector(std::string &str_, std::vector<std::string>& vector_){
    for(auto const &element : vector_){
      if(element == str_) return true;
    }
    return false;
  }

  std::string get_folder_path_from_file_path(std::string file_path_){
    std::string folder_path;
    if(file_path_[0] == '/') folder_path += "/";
    auto splitted_path = split_string(file_path_, "/");
    folder_path += join_vector_string(
      splitted_path,
      "/",
      0,
      int(splitted_path.size()) - 1
    );
    return folder_path;
  }
  std::string get_filename_from_file_path(std::string file_path_){
    auto splitted_path = split_string(file_path_, "/");
    return splitted_path.back();
  }
  std::string get_head_path_element_name(std::string folder_path_){
    auto elements = toolbox::split_string(folder_path_, "/");
    for(int i_element = elements.size()-1; i_element >= 0 ; i_element--){
      if(not elements[i_element].empty()) return elements[i_element];
    }
    return "";
  }
  std::string join_vector_string(std::vector<std::string> string_list_, std::string delimiter_, int begin_index_, int end_index_) {

    std::string joined_string;
    if(end_index_ == 0) end_index_ = int(string_list_.size());

    // circular permutation -> python style : tab[-1] = tab[tab.size - 1]
    if(end_index_ < 0 and int(string_list_.size()) > std::fabs(end_index_)) end_index_ = int(string_list_.size()) + end_index_;

    for(int i_list = begin_index_ ; i_list < end_index_ ; i_list++){
      if(not joined_string.empty()) joined_string += delimiter_;
      joined_string += string_list_[i_list];
    }

    return joined_string;
  }
  std::string remove_extra_doubled_characters(std::string input_str_, std::string doubled_char_){

    std::vector<std::string> substr_list = split_string(input_str_, doubled_char_);
    std::vector<std::string> cleaned_substr_list;
    for(int i_substr = 0 ; i_substr < int(substr_list.size()) ; i_substr++){
      if(not substr_list[i_substr].empty())
        cleaned_substr_list.emplace_back(substr_list[i_substr]);
    }
    std::string cleaned_input_str;
    if(do_string_starts_with_substring(input_str_, doubled_char_)) cleaned_input_str += doubled_char_;
    cleaned_input_str += join_vector_string(cleaned_substr_list, doubled_char_);
    if(do_string_ends_with_substring(input_str_, doubled_char_)) cleaned_input_str += doubled_char_;
    return cleaned_input_str;

  }
  std::string repeat_string(std::string str_, int n_times_){
    std::string out_str;
    for(int i_times = 0 ; i_times < n_times_ ; i_times++){
      out_str += str_;
    }
    return out_str;
  }
  std::string replace_substring_in_string(std::string &input_str_, std::string substr_to_look_for_, std::string substr_to_replace_){
    std::string stripped_str = input_str_;
    size_t index = 0;
    while ((index = stripped_str.find(substr_to_look_for_, index)) != std::string::npos) {
      stripped_str.replace(index, substr_to_look_for_.length(), substr_to_replace_);
      index += substr_to_replace_.length();
    }
    return stripped_str;
  }

  std::vector<std::string> split_string(std::string input_string_, std::string delimiter_){

    std::vector<std::string> output_splited_string;

    const char *src = input_string_.c_str();
    const char *next = src;

    std::string out_string_piece = "";

    while ((next = std::strstr(src, delimiter_.c_str())) != NULL) {
      out_string_piece = "";
      while (src != next){
        out_string_piece += *src++;
      }
      output_splited_string.emplace_back(out_string_piece);
      /* Skip the delimiter_ */
      src += delimiter_.size();
    }

    /* Handle the last token */
    out_string_piece = "";
    while (*src != '\0')
      out_string_piece += *src++;

    output_splited_string.emplace_back(out_string_piece);

    return output_splited_string;

  }


  //! Hardware functions :
  int get_terminal_width(){
    return consoleGetDefault()->consoleWidth;
  }
  int get_terminal_height(){
    return consoleGetDefault()->consoleHeight;
  }

  unsigned long get_RAM_info(std::string component_name_) {
    u64 used_ram = 0;
    if(component_name_ == "Total_application"){
      svcGetSystemInfo(&used_ram, 0, INVALID_HANDLE, 0);
    }
    else if(component_name_ == "Total_applet"){
      svcGetSystemInfo(&used_ram, 0, INVALID_HANDLE, 1);
    }
    else if(component_name_ == "Total_system"){
      svcGetSystemInfo(&used_ram, 0, INVALID_HANDLE, 2);
    }
    else if(component_name_ == "Total_systemunsafe"){
      svcGetSystemInfo(&used_ram, 0, INVALID_HANDLE, 3);
    }
    else if(component_name_ == "Used_application"){
      svcGetSystemInfo(&used_ram, 1, INVALID_HANDLE, 0);
    }
    else if(component_name_ == "Used_applet"){
      svcGetSystemInfo(&used_ram, 1, INVALID_HANDLE, 1);
    }
    else if(component_name_ == "Used_system"){
      svcGetSystemInfo(&used_ram, 1, INVALID_HANDLE, 2);
    }
    else if(component_name_ == "Used_systemunsafe"){
      svcGetSystemInfo(&used_ram, 1, INVALID_HANDLE, 3);
    }
    return used_ram;
  }
  std::string get_RAM_info_string(std::string component_name_){
    std::string out;
    unsigned long total = get_RAM_info("Total_" + component_name_);
    unsigned long used = get_RAM_info("Used_" + component_name_);
    out += component_name_;
    out += ": ";
    out += toolbox::parse_size_unit(used);
    out += "/";
    out += toolbox::parse_size_unit(total);
    return out;
  }

  //! direct filesystem functions :
  void setFileSystemBuffer(FsFileSystem* FileSystemBuffer_){
    _fileSystemBuffer_ = FileSystemBuffer_;
    _native_switch_FS_is_enabled_ = true;
  }
  void enableEmbeddedSwitchFS(){
    if(not _native_switch_FS_is_enabled_){
      fsdevMountSdmc();
      _fileSystemBuffer_ = fsdevGetDeviceFileSystem("sdmc");
//      fsOpenSdCardFileSystem(__FileSystemBuffer__);
      _native_switch_FS_is_enabled_ = true;
    }
  }
  void disableEmbeddedSwitchFS(){
    if(_native_switch_FS_is_enabled_){
      fsFsCommit(_fileSystemBuffer_);
      fsFsClose(_fileSystemBuffer_);
      _native_switch_FS_is_enabled_ = false;
    }
  }



  void dump_string_in_file(std::string str_, std::string& path_){ // don't use ref for str_ : str_ is modified

    if(toolbox::do_path_is_file(path_)){
      toolbox::delete_file(path_);
    }

    if(not _native_switch_FS_is_enabled_){
      std::ofstream out_file_stream;
      out_file_stream.open(path_.c_str());
      out_file_stream << str_ << std::endl;
      out_file_stream.close();
    }
    else{
      str_ += "\n";
      char path_buffer[FS_MAX_PATH];
      snprintf(path_buffer, FS_MAX_PATH, "%s", path_.c_str());

      // If we are talking specifically about std::string, then length() does return the number of bytes.
      // This is because a std::string is a basic_string of chars, and the C++ Standard defines the size of one char to be exactly one byte.
      // https://stackoverflow.com/questions/7743356/length-of-a-c-stdstring-in-bytes
      size_t length = str_.length();
      u64 size = 0;

      appletLockExit(); // prevent app to exit while writting

      if(R_SUCCEEDED(fsFsCreateFile(_fileSystemBuffer_, path_buffer, size + length, 0))){
        FsFile fs_file;
        if(R_SUCCEEDED(fsFsOpenFile(_fileSystemBuffer_, path_buffer, FsOpenMode_Write, &fs_file))){
          if(R_SUCCEEDED(fsFileWrite(&fs_file, 0, str_.c_str(), size+length, FsWriteOption_Flush))){
            fsFileFlush(&fs_file);
          }
        }
        fsFileClose(&fs_file);
      }
      appletUnlockExit();

    }

  }

  bool do_path_is_valid(std::string &path_){
    if(not _native_switch_FS_is_enabled_){
      struct stat buffer{};
      return (stat (path_.c_str(), &buffer) == 0);
    }
    else {
      if(do_path_is_folder(path_)){
        return true;
      }
      else return do_path_is_file(path_);
    }
  }
  bool do_path_is_folder(std::string &folder_path_){
    if(not _native_switch_FS_is_enabled_){
      struct stat info{};
      stat( folder_path_.c_str(), &info );
      return bool(S_ISDIR(info.st_mode));
    }
    else {
      bool result = false;
      FsDir fs_DirBuffer;
      char fs_pathBuffer[FS_MAX_PATH];
      strcpy(fs_pathBuffer, folder_path_.c_str());
      if(R_SUCCEEDED(fsFsOpenDirectory(_fileSystemBuffer_, fs_pathBuffer, FsDirOpenMode_ReadDirs, &fs_DirBuffer))){
        result = true;
      }
      fsDirClose(&fs_DirBuffer);
      return result;
    }
  }
  bool do_path_is_file(std::string &file_path_) {
    if(not _native_switch_FS_is_enabled_){
      if(not do_path_is_valid(file_path_)) return false;
      return (not do_path_is_folder(file_path_));
    }
    else{
      bool result = false;
      FsFile fs_FileBuffer;
      char fs_pathBuffer[FS_MAX_PATH];
      snprintf(fs_pathBuffer, FS_MAX_PATH, "%s", file_path_.c_str());
      if(R_SUCCEEDED(fsFsOpenFile(_fileSystemBuffer_, &fs_pathBuffer[0], FsOpenMode_Read, &fs_FileBuffer))) {
        result = true;
      }
      fsFileClose(&fs_FileBuffer);
      return result;
    }
  }
  bool do_files_are_the_same(std::string file1_path_, std::string file2_path_) {
    bool file_are_same = false;

    if(not _native_switch_FS_is_enabled_){
      if(not do_path_is_file(file1_path_)) return false;
      if(not do_path_is_file(file2_path_)) return false;
      if(toolbox::get_file_size(file1_path_) != toolbox::get_file_size(file2_path_)) return false; // very fast
      if(_CRC_check_is_enabled_){
        if(toolbox::get_hash_CRC32(file1_path_) != toolbox::get_hash_CRC32(file2_path_)) return false;
      }
      file_are_same = true;
    }
    else {

      // opening file1
      char path_buffer_file1[FS_MAX_PATH];
      FsFile fs_file1;
      snprintf(path_buffer_file1, FS_MAX_PATH, "%s", file1_path_.c_str());
      if(R_SUCCEEDED(fsFsOpenFile(_fileSystemBuffer_, path_buffer_file1, FsOpenMode_Read, &fs_file1))){
        // opening file2
        char path_buffer_file2[FS_MAX_PATH];
        FsFile fs_file2;
        snprintf(path_buffer_file2, FS_MAX_PATH, "%s", file2_path_.c_str());
        if(R_SUCCEEDED(fsFsOpenFile(_fileSystemBuffer_, path_buffer_file2, FsOpenMode_Read, &fs_file2))){

          // get size of file1
          s64 size_file1 = 0;
          if(R_SUCCEEDED(fsFileGetSize(&fs_file1, &size_file1))){
            // get size of file2
            s64 size_file2 = 0;
            if(R_SUCCEEDED(fsFileGetSize(&fs_file2, &size_file2))){
              if(size_file1 == size_file2){
                if(_CRC_check_is_enabled_){

//                  size_t copy_buffer_size = 0x10000; // 65 kB (65536 B) // too much for 2 files...
//                  size_t copy_buffer_size = 0x1000; // 4,096 B // on the safe side
                  size_t copy_buffer_size = 0xD000; // 53,248 B
                  s64 copy_buffer_size_s64 = s64(copy_buffer_size);
                  u8 data_buffer_file1[copy_buffer_size];
                  u8 data_buffer_file2[copy_buffer_size];
                  u64 bytes_read_counter_file1 = 0;
                  u64 bytes_read_counter_file2 = 0;
                  u64 read_offset = 0;
                  unsigned long  last_crc1 = crc32(0L, Z_NULL, 0);
                  unsigned long  last_crc2 = crc32(0L, Z_NULL, 0);
                  file_are_same = true; // true by default -> change if not
                  s64 counts = 0;
                  s64 expected_total_count = s64(size_file1 / copy_buffer_size_s64);
                  do {

                    toolbox::fill_progress_map("do_files_are_the_same", double(counts)/double(expected_total_count));
                    counts++;

                    // buffering file1
                    if(R_FAILED(fsFileRead(&fs_file1, read_offset, &data_buffer_file1[0], copy_buffer_size, FsReadOption_None, &bytes_read_counter_file1))){
                      file_are_same = false;
                      break;
                    }

                    // buffering file2
                    if(R_FAILED(fsFileRead(&fs_file2, read_offset, &data_buffer_file2[0], copy_buffer_size, FsReadOption_None, &bytes_read_counter_file2))){
                      file_are_same = false;
                      break;
                    }

                    // check read size
                    if(bytes_read_counter_file1 != bytes_read_counter_file2){
                      file_are_same = false;
                      break;
                    }

                    // check crc
                    last_crc1 = crc32(last_crc1, data_buffer_file1, bytes_read_counter_file1);
                    last_crc2 = crc32(last_crc2, data_buffer_file2, bytes_read_counter_file2);
                    if(last_crc1 != last_crc2){
                      file_are_same = false;
                      break;
                    }

                    // preparing next loop
                    read_offset += bytes_read_counter_file1;

                  }
                  while(s64(read_offset) < size_file1);

                } // CRC ? yes
                else {
                  // if CRC is disable and we reached this point, then we consider files are the same
                  file_are_same = true;
                } // CRC ? no

              } // size match ?
            } // size file 2
          } // size file 1
        } // open file 2
        fsFileClose(&fs_file2);
      } // open file 1
      fsFileClose(&fs_file1);

    } // switch fs

    toolbox::fill_progress_map("do_files_are_the_same", 1.);
    return file_are_same;

  }

  bool copy_file(std::string &source_file_path_, std::string &destination_file_path_){
    bool do_copy_is_success = false;

    if(do_path_is_file(destination_file_path_)){
      if(not delete_file(destination_file_path_)){
        return false;
      }
    }
    else{
      std::string destination_folder_path = get_folder_path_from_file_path(destination_file_path_);
      if(not do_path_is_folder(destination_folder_path)){
        mkdir_path(destination_folder_path);
      }
    }

    if(not _native_switch_FS_is_enabled_){

      if(not do_path_is_file(source_file_path_)){
        return false;
      }

      // faster but more ram is needed
      std::ifstream source(source_file_path_, std::ios::binary);
      std::ofstream dest(destination_file_path_, std::ios::binary);

      source.seekg(0, std::ios::end);
      std::ifstream::pos_type size = source.tellg();
      source.seekg(0);

      char* buffer = new char[size];

      source.read(buffer, size);
      dest.write(buffer, size);

      delete[] buffer;
      dest.close();
      source.close();

      // (alot) slower but less memory needed
//    FILE *in, *out;
//    int c;
//    if ((in = fopen(source_file_path_.c_str(),"rb")) == NULL)
//    {
//      printf("Could not open source file: %s\n",source_file_path_.c_str());
//      return 1;
//    }
//
//    if ((out = fopen(destination_file_path_.c_str(),"wb")) == NULL)
//    {
//      printf("Could not open destination file: %s\n",destination_file_path_.c_str());
//      return 1;
//    }
//
//    while ((c = fgetc(in)) != EOF)
//      fputc(c,out);
//
//    fclose(in);
//    fclose(out);

      do_copy_is_success = true;

    }
    else {

      // opening source file
      char path_buffer_source[FS_MAX_PATH];
      FsFile fs_file_source;
      snprintf(path_buffer_source, FS_MAX_PATH, "%s", source_file_path_.c_str());
      if(R_SUCCEEDED(fsFsOpenFile(_fileSystemBuffer_, &path_buffer_source[0], FsOpenMode_Read, &fs_file_source))){
        // get size of source file
        s64 source_size = 0;
        if(R_SUCCEEDED(fsFileGetSize(&fs_file_source, &source_size))){

          // create destination file
          char path_buffer_destination[FS_MAX_PATH];
          snprintf(path_buffer_destination, FS_MAX_PATH, "%s", destination_file_path_.c_str());
          if(R_SUCCEEDED(fsFsCreateFile(_fileSystemBuffer_, &path_buffer_destination[0], source_size, 0))){

            // open destination file
            FsFile fs_file_destination;
            if(R_SUCCEEDED(fsFsOpenFile(_fileSystemBuffer_, &path_buffer_destination[0], FsOpenMode_Write, &fs_file_destination))){

              size_t copy_buffer_size = 0x10000; // 65 kB
//              size_t copy_buffer_size = 0x1A000; // 106.496 kB
//              size_t copy_buffer_size = 0x1000; // 4 kB
              s64 copy_buffer_size_s64 = s64(copy_buffer_size);
              u8 data_buffer[copy_buffer_size];
              u64 bytes_read_counter = 0;
              u64 read_offset = 0;
              do_copy_is_success = true; // consider it worked by default -> will change if not

              s64 counts = 0;
              s64 expected_total_count = s64(source_size/copy_buffer_size_s64) + 1;
              do {

                toolbox::fill_progress_map("copy_file", double(counts)/double(expected_total_count));
                counts+=1;

                // buffering source file
                if(R_FAILED(fsFileRead(&fs_file_source, read_offset, &data_buffer[0], copy_buffer_size, FsReadOption_None, &bytes_read_counter))){
                  do_copy_is_success = false;
                  break;
                }

                // dumping data in destination file
                if(R_FAILED(fsFileWrite(&fs_file_destination, read_offset, &data_buffer[0], bytes_read_counter, FsWriteOption_Flush))){
                  do_copy_is_success = false;
                  break;
                }

                // preparing next loop
                read_offset += bytes_read_counter;

              }
              while(s64(read_offset) < source_size);

            }
            fsFileClose(&fs_file_destination);
          }

        }
      }
      fsFileClose(&fs_file_source);

    }

    toolbox::fill_progress_map("copy_file", 1.);
    return do_copy_is_success;
  }
  bool mv_file(std::string &source_file_path_, std::string &destination_file_path_){

    if(not do_path_is_file(source_file_path_)){
      return false;
    }

    if(do_path_is_file(destination_file_path_)){
      delete_file(destination_file_path_);
    }
    else{
      std::string destination_folder_path = get_folder_path_from_file_path(destination_file_path_);
      if(not do_path_is_folder(destination_folder_path)){
        mkdir_path(destination_folder_path);
      }
    }

    if(not _native_switch_FS_is_enabled_){
      std::rename(source_file_path_.c_str(), destination_file_path_.c_str());
      if(not toolbox::do_path_is_file(destination_file_path_) or toolbox::do_path_is_file(source_file_path_)){
        return false;
      }
    }
    else{
      char source_char[FS_MAX_PATH];
      char dest_char[FS_MAX_PATH];
      snprintf(source_char, FS_MAX_PATH, "%s", source_file_path_.c_str());
      snprintf(dest_char, FS_MAX_PATH, "%s", destination_file_path_.c_str());
      if(R_FAILED(fsFsRenameFile(_fileSystemBuffer_, &source_char[0], &dest_char[0]))){
        return false;
      }
    }

    return true;
  }
  bool delete_file(std::string file_path_){
    if(not do_path_is_file(file_path_)) return true;
    if(not _native_switch_FS_is_enabled_){
      std::remove(file_path_.c_str());
    }
    else{
      char fs_path_buffer[FS_MAX_PATH];
      snprintf(fs_path_buffer, FS_MAX_PATH, "%s", file_path_.c_str());
      fsFsDeleteFile(_fileSystemBuffer_, fs_path_buffer);
    }
    return not do_path_is_file(file_path_);
  }
  bool mkdir_path(std::string new_folder_path_){

    bool result = false;
    if(do_path_is_folder(new_folder_path_)) return true;

    std::string current_level;
    std::string level;
    std::stringstream ss(new_folder_path_);

    // split path using slash as a separator
    while (std::getline(ss, level, '/'))
    {
      current_level += level; // append folder to the current level
      if(current_level.empty()) current_level = "/";
      current_level = toolbox::remove_extra_doubled_characters(current_level, "/");
      // create current level
      if(not do_path_is_folder(current_level)){
        if(not _native_switch_FS_is_enabled_) {
          ::mkdir(current_level.c_str(), 0777);
          result = true;
        }
        else{
          char fs_pathBuffer[FS_MAX_PATH];
          snprintf(fs_pathBuffer, FS_MAX_PATH, "%s", current_level.c_str());
          if(R_SUCCEEDED(fsFsCreateDirectory(_fileSystemBuffer_, fs_pathBuffer))){
            result = true;
          }
        }
      }
      current_level += "/"; // don't forget to append a slash
    }


    return result;

  }
  bool delete_directory(std::string folder_path_){
    if(not do_folder_is_empty(folder_path_)) return false;
    if(not _native_switch_FS_is_enabled_){
      rmdir(folder_path_.c_str());
    }
    else{
      char fs_pathBuffer[FS_MAX_PATH];
      snprintf(fs_pathBuffer, FS_MAX_PATH, "%s", folder_path_.c_str());
      if(R_FAILED(fsFsDeleteDirectory(_fileSystemBuffer_, fs_pathBuffer))){
        return false;
      }
    }
    return not do_path_is_folder(folder_path_);
  }

  long int get_file_size(std::string &file_path_) {
    long int output_size = 0;
    if(not _native_switch_FS_is_enabled_){
      if(do_path_is_file(file_path_)){
        std::ifstream testFile(file_path_.c_str(), std::ios::binary);
        const auto begin = testFile.tellg();
        testFile.seekg (0, std::ios::end);
        const auto end = testFile.tellg();
        const auto fsize = (end-begin);
        output_size = fsize;
      }
    }
    else{
      s64 file_size;
      char fs_pathBuffer[FS_MAX_PATH];
      snprintf(fs_pathBuffer, FS_MAX_PATH, "%s", file_path_.c_str());
      FsFile fs_FileBuffer;
      if(R_SUCCEEDED(fsFsOpenFile(_fileSystemBuffer_, fs_pathBuffer, FsOpenMode_Read, &fs_FileBuffer))){
        if(R_SUCCEEDED(fsFileGetSize(&fs_FileBuffer, &file_size))){
          output_size = (long int)(file_size);
        }
      }
      fsFileClose(&fs_FileBuffer);
    }
    return output_size;
  }
  unsigned long get_hash_CRC32(std::string file_path_){

    unsigned long output_crc = crc32(0L, Z_NULL, 0);
    if(not _native_switch_FS_is_enabled_){
      if(toolbox::do_path_is_file(file_path_)){
        std::string data = toolbox::dump_file_as_string(file_path_);
        if(not data.empty()){
          output_crc = crc32(0L, Z_NULL, 0);
          output_crc = crc32(output_crc, (const unsigned char*)data.c_str(), data.size());
        }
      }
    }
    else {

      // opening source file
      char fs_pathBuffer[FS_MAX_PATH];
      snprintf(fs_pathBuffer, FS_MAX_PATH, "%s", file_path_.c_str());
      FsFile fs_FileBuffer;
      if(R_SUCCEEDED(fsFsOpenFile(_fileSystemBuffer_, fs_pathBuffer, FsOpenMode_Read, &fs_FileBuffer))){

        // get size of source file
        s64 source_size = 0;
        if(R_SUCCEEDED(fsFileGetSize(&fs_FileBuffer, &source_size))){

          // read file
          size_t copy_buffer_size = 0x10000;
          u8 data_buffer[copy_buffer_size];

          u64 bytes_read_counter = 0;
          u64 read_offset = 0;

          output_crc = crc32(0L, Z_NULL, 0);
          do {

            // buffering source file
            if(R_FAILED(fsFileRead(&fs_FileBuffer, read_offset, &data_buffer[0], copy_buffer_size, FsReadOption_None, &bytes_read_counter))){
              break;
            }

            output_crc = crc32_combine(output_crc, u64(data_buffer), bytes_read_counter);

            // preparing next loop
            read_offset += bytes_read_counter;

          }
          while(s64(read_offset) < source_size);
        }
      }
      fsFileClose(&fs_FileBuffer);
    }
    return output_crc;
  }

  std::string dump_file_as_string(std::string file_path_){
    std::string data;
    if(not _native_switch_FS_is_enabled_){
      if(do_path_is_file(file_path_)){
        std::ifstream input_file( file_path_.c_str(), std::ios::binary | std::ios::in );
        std::ostringstream ss;
        ss << input_file.rdbuf();
        data = ss.str();
        input_file.close();
      }
    }
    else {
      char fs_pathBuffer[FS_MAX_PATH];
      snprintf(fs_pathBuffer, FS_MAX_PATH, "%s", file_path_.c_str());
      FsFile fs_FileBuffer;
      if(R_FAILED(fsFsOpenFile(_fileSystemBuffer_, fs_pathBuffer, FsOpenMode_Read, &fs_FileBuffer))){
        fsFileClose(&fs_FileBuffer);
        return "";
      }
      s64 file_size = 0;
      if(R_FAILED(fsFileGetSize(&fs_FileBuffer, &file_size))){
        fsFileClose(&fs_FileBuffer);
        return "";
      }
      char *buf = (char *)malloc(file_size + 1);
      u64 file_size_u = (u64) file_size;
      u64 bytes_read;
      if(R_SUCCEEDED(fsFileRead(&fs_FileBuffer, 0, buf, file_size_u, FsReadOption_None, &bytes_read))){
        data = std::string(buf);
      }
      fsFileClose(&fs_FileBuffer);
      free(buf);
    }
    return data;
  }
  std::string get_working_directory(){
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    std::string output_cwd(cwd);
    std::string prefix = "sdmc:";
    if(toolbox::do_string_starts_with_substring(output_cwd, prefix)){
      output_cwd = output_cwd.substr(prefix.size(), output_cwd.size());
    }
    return output_cwd;
  }
  std::vector<std::string> dump_file_as_vector_string(std::string file_path_){
    std::vector<std::string> lines;
    if(do_path_is_file(file_path_)){
      std::string data = toolbox::dump_file_as_string(file_path_);
      lines = toolbox::split_string(data, "\n");
    }
    return lines;
  }

  std::vector<std::string> get_list_of_entries_in_folder(std::string folder_path_) {

    std::vector<std::string> entries_list;
    if(not _native_switch_FS_is_enabled_){
      if(not do_path_is_folder(folder_path_)) return entries_list;
      DIR* directory;
      directory = opendir(folder_path_.c_str()); //Open current-working-directory.
      if( directory == nullptr ) {
        std::cout << "Failed to open directory : " << folder_path_ << std::endl;
        return entries_list;
      }
      else {
        struct dirent* entry;
        while ( (entry = readdir(directory)) ) {
          entries_list.emplace_back(entry->d_name);
        }
        closedir(directory);
        return entries_list;
      }
    }
    else{
      char fs_pathBuffer[FS_MAX_PATH];
      snprintf(fs_pathBuffer, FS_MAX_PATH, "%s", folder_path_.c_str());
      FsDir fs_DirBuffer;
      if(R_FAILED(fsFsOpenDirectory(_fileSystemBuffer_, fs_pathBuffer, FsDirOpenMode_ReadDirs | FsDirOpenMode_ReadFiles, &fs_DirBuffer))){
        fsDirClose(&fs_DirBuffer);
        return entries_list;
      }
      s64 entry_count;
      if(R_FAILED(fsDirGetEntryCount(&fs_DirBuffer, &entry_count))){
        fsDirClose(&fs_DirBuffer);
        return entries_list;
      }
      size_t entry_count_size_t(entry_count);
      s64 total_entries;
      std::vector<FsDirectoryEntry> fs_directory_entries(entry_count_size_t);
      if(R_FAILED(fsDirRead(&fs_DirBuffer, &total_entries, entry_count_size_t, &fs_directory_entries[0]))){
        fsDirClose(&fs_DirBuffer);
        return entries_list;
      }
      for(u32 i_entry = 0 ; i_entry < entry_count_size_t ; i_entry++){
        std::string entry_name = fs_directory_entries[i_entry].name;
        if(entry_name == "." or entry_name == ".."){
          continue;
        }
        entries_list.emplace_back(fs_directory_entries[i_entry].name);
      }
      fsDirClose(&fs_DirBuffer);
      return entries_list;
    }

  }
  std::vector<std::string> get_list_of_subfolders_in_folder(std::string folder_path_) {
    std::vector<std::string> subfolders_list;
    if(not _native_switch_FS_is_enabled_){
      if(not do_path_is_folder(folder_path_)) return subfolders_list;
      DIR* directory;
      directory = opendir(folder_path_.c_str()); //Open current-working-directory.
      if( directory == nullptr ) {
        std::cout << "Failed to open directory : " << folder_path_ << std::endl;
        return subfolders_list;
      } else {
        struct dirent* entry;
        while ( (entry = readdir(directory)) ) {
          std::string folder_candidate = folder_path_ + "/" + std::string(entry->d_name);
          if(do_path_is_folder(folder_candidate)){
            subfolders_list.emplace_back(entry->d_name);
          }
        }
        closedir(directory);
        return subfolders_list;
      }
    }
    else{
      char fs_pathBuffer[FS_MAX_PATH];
      snprintf(fs_pathBuffer, FS_MAX_PATH, "%s", folder_path_.c_str());
      FsDir fs_DirBuffer;
      if(R_FAILED(fsFsOpenDirectory(_fileSystemBuffer_, fs_pathBuffer, FsDirOpenMode_ReadDirs, &fs_DirBuffer))){
        fsDirClose(&fs_DirBuffer);
        return subfolders_list;
      }
      s64 entry_count;
      if(R_FAILED(fsDirGetEntryCount(&fs_DirBuffer, &entry_count))){
        fsDirClose(&fs_DirBuffer);
        return subfolders_list;
      }
      size_t entry_count_size_t(entry_count);
      s64 total_entries;
      std::vector<FsDirectoryEntry> fs_directory_entries(entry_count_size_t);
      if(R_FAILED(fsDirRead(&fs_DirBuffer, &total_entries, entry_count_size_t, &fs_directory_entries[0]))){
        fsDirClose(&fs_DirBuffer);
        return subfolders_list;
      }
      for(u32 i_entry = 0 ; i_entry < entry_count_size_t ; i_entry++){
        if(fs_directory_entries[i_entry].type != FsDirEntryType_Dir) // should not be necessary
          continue;
        std::string entry_name = fs_directory_entries[i_entry].name;
        if(entry_name == "." or entry_name == ".."){
          continue;
        }
        subfolders_list.emplace_back(fs_directory_entries[i_entry].name);
      }
      fsDirClose(&fs_DirBuffer);
      return subfolders_list;
    }

  }
  std::vector<std::string> get_list_of_files_in_folder(std::string folder_path_){
    std::vector<std::string> files_list;
    if(not _native_switch_FS_is_enabled_){
      if(not do_path_is_folder(folder_path_)) return files_list;
      DIR* directory;
      directory = opendir(folder_path_.c_str()); //Open current-working-directory.
      if( directory == nullptr ) {
        std::cout << "Failed to open directory : " << folder_path_ << std::endl;
        return files_list;
      } else {
        struct dirent* entry;
        while ( (entry = readdir(directory)) ) {
          std::string file_candidate = folder_path_ + "/" + std::string(entry->d_name);
          if(do_path_is_file(file_candidate)){
            files_list.emplace_back(entry->d_name);
          }
        }
        closedir(directory);
        return files_list;
      }
    }
    else{
      char fs_pathBuffer[FS_MAX_PATH];
      snprintf(fs_pathBuffer, FS_MAX_PATH, "%s", folder_path_.c_str());
      FsDir fs_DirBuffer;
      if(R_FAILED(fsFsOpenDirectory(_fileSystemBuffer_, fs_pathBuffer, FsDirOpenMode_ReadFiles, &fs_DirBuffer))){
        fsDirClose(&fs_DirBuffer);
        return files_list;
      }
      s64 entry_count;
      if(R_FAILED(fsDirGetEntryCount(&fs_DirBuffer, &entry_count))){
        fsDirClose(&fs_DirBuffer);
        return files_list;
      }
      size_t entry_count_size_t(entry_count);
      s64 total_entries;
      std::vector<FsDirectoryEntry> fs_directory_entries(entry_count_size_t);
      if(R_FAILED(fsDirRead(&fs_DirBuffer, &total_entries, entry_count_size_t, &fs_directory_entries[0]))){
        fsDirClose(&fs_DirBuffer);
        return files_list;
      }
      for(u32 i_entry = 0 ; i_entry < entry_count_size_t ; i_entry++){
        if(fs_directory_entries[i_entry].type != FsDirEntryType_File)
          continue;
        std::string entry_name = fs_directory_entries[i_entry].name;
        if(entry_name == "." or entry_name == ".."){
          continue;
        }
        files_list.emplace_back(fs_directory_entries[i_entry].name);
      }
      fsDirClose(&fs_DirBuffer);
      return files_list;
    }
  }


  //! direct filesystem (not native switch fs) functions :
  size_t get_hash_file(std::string file_path_){

    std::string data;
    std::ifstream input_file( file_path_.c_str(), std::ios::binary | std::ios::in );
    if( !input_file ) return 0;

    std::ostringstream ss;
    ss << input_file.rdbuf();
    data = ss.str();

    input_file.close();

    std::hash<std::string> hash_fn;
    return hash_fn(data);

  }
  int get_hash_bsdChecksumFromFilepath(std::string path_){
    if(do_path_is_file(path_)){
      FILE* f = std::fopen(path_.c_str(), "r");
      int checksum = get_hash_bsdChecksumFromFile(f);
      std::fclose(f);
      return checksum;
    } else{
      return -1;
    }
  }
  int get_hash_bsdChecksumFromFile(FILE *fp) {
    int checksum = 0;             /* The checksum mod 2^16. */

    for (int ch = getc(fp); ch != EOF; ch = getc(fp)) {
      checksum = (checksum >> 1) + ((checksum & 1) << 15);
      checksum += ch;
      checksum &= 0xffff;       /* Keep it within bounds. */
    }
    return checksum;
  }


  //! indirect filesystem (through dependencies) functions :
  bool do_folder_is_empty(std::string folder_path_){
    if(not do_path_is_folder(folder_path_)) return false;
    return get_list_of_entries_in_folder(folder_path_).empty();
  }

  std::vector<std::string> get_list_files_in_subfolders(std::string &folder_path_) {

    // WARNING : Recursive function
    std::vector<std::string> output_file_paths = get_list_of_files_in_folder(folder_path_);

    auto subfolder_names_list = get_list_of_subfolders_in_folder(folder_path_);
    for(auto &subfolder_name : subfolder_names_list){
      std::string subfolder_full_path = folder_path_;
      subfolder_full_path += "/";
      subfolder_full_path += subfolder_name;
      auto subfile_names_list = get_list_files_in_subfolders(subfolder_full_path);
      for(auto &subfile_name : subfile_names_list){
        std::string relative_subfile_path;
        relative_subfile_path += subfolder_name;
        relative_subfile_path += "/";
        relative_subfile_path += subfile_name;
        output_file_paths.emplace_back(toolbox::remove_extra_doubled_characters(relative_subfile_path, "/"));
      }
    }

    return output_file_paths;

  }

  std::string to_lower_case(std::string& input_str_){
    std::string output_str(input_str_);
    std::transform(output_str.begin(), output_str.end(), output_str.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return output_str;
  }



  std::string recursive_search_for_subfolder_name_like_tid(std::string folder_path_){

    std::string tid_example = "0100626011656000";

    // WARNING : Recursive function
    std::vector<std::string> subfolder_names = get_list_of_subfolders_in_folder(folder_path_);

    for(auto &subfolder_name : subfolder_names){
      if( subfolder_name.size() == tid_example.size() and subfolder_name[0] == tid_example[0] ){
        return subfolder_name;
      }
    }

    // if not found
    std::string output;
    std::string path;
    for(auto &subfolder_name : subfolder_names){
      path = folder_path_;
      path += "/";
      path += subfolder_name;
      output = recursive_search_for_subfolder_name_like_tid(path);
      if(not output.empty()){
        return output;
      }
    }

    return "";

  }


  //! External function
  std::string get_app_version(){
    std::stringstream ss;
    ss << get_version_major() << "." << get_version_minor() << "." << get_version_micro() << get_version_tag();
    return ss.str();
  }

}