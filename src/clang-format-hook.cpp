/**
 * Copyright 2020 Florian Kramer
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstring>
#include <filesystem>
#include <fstream>
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <unistd.h>

namespace fs = std::filesystem;

struct CmdResult {
  std::string output;
  int return_status;
};

struct Settings {
  std::string clang_format_exe = "clang-format";
  std::vector<std::string> inputs;
};

void printHelp(char *exe_name) {
  std::cout << "Usage: " << exe_name << " [options] <inputs>\n";
  std::cout << "Checks the given inputs for code style changes\n";
  std::cout << "\n";
  std::cout << "Options:\n";
  std::cout << std::setw(20) << std::right << "-c, --clang-format"
            << std::setw(0) << "    "
            << "The clang-format executable to use" << std::endl;
  std::cout.flush();
}

Settings parseSettings(int argc, char **argv) {
  Settings settings;

  struct option opt[] = {{"clang-format", required_argument, NULL, 'c'},
                         {NULL, 0, NULL, 0}};

  int longind = 0;
  int o;
  while ((o = getopt_long(argc, argv, ":c:", opt, &longind)) >= 0) {
    switch (o) {
    case 'c':
      settings.clang_format_exe = optarg;
      break;
    case ':':
      std::cerr << "The option " << argv[optind - 1] << " requires an argument"
                << std::endl;
      printHelp(argv[0]);
      exit(1);
      break;
    case '?':
      std::cerr << "Unknown options " << argv[optind - 1] << std::endl;
      printHelp(argv[0]);
      exit(1);
      break;
    }
  }

  for (int i = optind; i < argc; ++i) {
    settings.inputs.push_back(argv[i]);
  }

  return settings;
}

std::vector<std::filesystem::path>
listSrcFiles(const std::filesystem::path &root) {
  std::vector<fs::path> src_files;
  std::vector<fs::path> to_process{root};
  while (!to_process.empty()) {
    fs::path current = to_process.back();
    to_process.pop_back();
    if (is_directory(current)) {
      for (const fs::directory_entry &child : fs::directory_iterator(current)) {
        to_process.push_back(child.path());
      }
    } else if (current.has_extension()) {
      fs::path ext = current.extension();
      if (ext == ".cpp" || ext == ".h") {
        src_files.push_back(current);
      }
    }
  }
  return src_files;
}

CmdResult runCmdForOutput(std::vector<std::string> args) {
  CmdResult res;

  if (args.empty()) {
    return res;
  }

  size_t cmd_size = 0;
  for (const std::string &arg : args) {
    cmd_size += arg.size();
  }
  cmd_size += args.size() - 1;
  std::vector<char> cmd(cmd_size + 1);
  size_t cmd_offset = 0;
  for (size_t i = 0; i < args.size(); ++i) {
    std::memcpy(cmd.data() + cmd_offset, args[i].c_str(), args[i].size());
    cmd_offset += args[i].size();
    if (i + 1 < args.size()) {
      cmd[cmd_offset] = ' ';
      cmd_offset++;
    }
  }
  cmd.back() = 0;

  FILE *pipe = popen(cmd.data(), "r");
  if (!pipe) {
    throw std::runtime_error("Unable to run " + std::string(cmd.data()));
  }

  char buffer[1024];
  while (fgets(buffer, sizeof(buffer), pipe) != 0) {
    res.output += buffer;
  }
  res.return_status = pclose(pipe);
  return res;
}

std::optional<std::string>
getFileFormatDiff(const fs::path &file, const std::string &clang_format_exe) {
  // Read the file
  std::string file_contents;
  {
    std::ifstream in(file);
    if (!in.is_open()) {
      throw std::runtime_error("Unable to open src file " + std::string(file));
    }
    in.seekg(0, std::ios::end);
    size_t file_size = in.tellg();
    in.seekg(0, std::ios::beg);
    file_contents.resize(file_size, 0);
    in.read(file_contents.data(), file_size);
  }

  // run clang-format
  CmdResult res = runCmdForOutput({clang_format_exe, file});
  if (res.return_status != 0) {
    return "Got return code " + std::to_string(res.return_status) +
           " when executing " + clang_format_exe + " " + file.c_str();
  }

  if (file_contents != res.output) {
    return std::string(file.c_str()) + " changes when formatted.";
  } else {
    return std::nullopt;
  }
}

int main(int argc, char **argv) {
  Settings settings = parseSettings(argc, argv);
  std::vector<fs::path> input_files;
  for (const std::string &path : settings.inputs) {
    std::vector<fs::path> files = listSrcFiles(path);
    input_files.insert(input_files.end(), files.begin(), files.end());
  }

  bool inputs_need_formatting = false;

  std::mutex output_mutex;
  auto printDiff = [&output_mutex](const fs::path &path,
                                   const std::string &diff) {
    std::lock_guard lock(output_mutex);
    std::cout << "File " << path << " needs formatting\n";
    std::cout << diff << std::endl;
  };

#pragma omp parallel for
  for (size_t input_idx = 0; input_idx < input_files.size(); ++input_idx) {
    fs::path input = input_files[input_idx];
    std::optional<std::string> diff =
        getFileFormatDiff(input, settings.clang_format_exe);
    if (diff) {
      inputs_need_formatting = true;
      printDiff(input, *diff);
    }
  }
  return inputs_need_formatting ? 2 : 0;
}
