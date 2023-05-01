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

#ifndef CONTEXT_GRAPH_H_
#define CONTEXT_GRAPH_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "fst/compose.h"
#include "fst/fst.h"
#include "fst/matcher.h"
#include "fst/vector-fst.h"

namespace asr {

using ArcIterator = fst::ArcIterator<fst::StdFst>;
using Matcher = fst::SortedMatcher<fst::StdFst>;
using Weight = fst::StdArc::Weight;

class ContextGraph {
 public:
  explicit ContextGraph(const std::string& unit_path,
                        const std::string& context_path,
                        float context_score = 0);
  explicit ContextGraph(const std::unordered_map<std::string, int>& unit_table,
                        const std::vector<std::string>& contexts,
                        float context_score = 0);
  virtual ~ContextGraph() = default;
  ContextGraph(const ContextGraph&) = delete;

  bool SplitContextToUnits(const std::string& str, std::vector<int>* units);
  int TraceContext(int cur_state, int unit_id, int* final_state);
  void BuildContextGraph();

  void ConvertToAC();

  int GetNextState(int cur_state, int unit_id, float* score,
                   std::unordered_set<std::string>* contexts = nullptr);

  std::shared_ptr<fst::SymbolTable> unit_table_;

 private:
  std::unordered_map<std::string, float> contexts_;

  std::unique_ptr<fst::StdVectorFst> graph_;
  std::unordered_map<int, int> fallback_finals_;  // States fallback to final
  std::unordered_map<int, std::string> context_table_;  // Finals to context
};

}  // namespace asr

#endif  // CONTEXT_GRAPH_H_
