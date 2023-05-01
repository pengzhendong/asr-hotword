// Copyright (c) 2023 Zhendong Peng (pzd17@tsinghua.org.cn)
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "hotword/context_graph.h"

DEFINE_string(unit_path, "", "unit file path of acoustic model");
DEFINE_string(context_path, "", "context file path");
DEFINE_double(context_score, 0, "context score");

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);

  // asr::ContextGraph graph(FLAGS_unit_path, FLAGS_context_path,
  //                         FLAGS_context_score);

  std::unordered_map<std::string, int> unit_table = {
      {"<blank>", 0}, {"<unk>", 1}, {"天", 2}, {"行", 3}, {"健", 4}};
  asr::ContextGraph graph(unit_table, {"天行健"}, FLAGS_context_score);

  int cur_state = 0;
  int unit_id = 2;
  float score = 0;
  std::unordered_set<std::string> contexts;
  int next_state = graph.GetNextState(cur_state, unit_id, &score, &contexts);

  LOG(INFO) << "Next state: " << next_state;
  LOG(INFO) << "Score: " << score;
  for (const auto& context : contexts) {
    LOG(INFO) << "Context: " << context;
  }
  return 0;
}
