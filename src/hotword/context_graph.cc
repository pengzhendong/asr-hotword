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

#include <fstream>
#include <queue>
#include <utility>

#include "fst/determinize.h"

#include "utils/string.h"

namespace asr {

ContextGraph::ContextGraph(const std::string& unit_path,
                           const std::string& context_path,
                           float context_score) {
  unit_table_ =
      std::shared_ptr<fst::SymbolTable>(fst::SymbolTable::ReadText(unit_path));

  std::ifstream context_file;
  context_file.open(context_path);
  CHECK(context_file.is_open()) << "Open context file failed: " << context_path;
  std::string line;
  while (std::getline(context_file, line)) {
    contexts_[Trim(line)] = context_score;
  }
  context_file.close();

  LOG(INFO) << "Num of contexts: " << contexts_.size();
  if (contexts_.empty()) {
    graph_.reset();
    return;
  }

  // Build context graph
  BuildContextGraph();
  graph_->Write("contexts.fst");
}

ContextGraph::ContextGraph(
    const std::unordered_map<std::string, int>& unit_table,
    const std::vector<std::string>& contexts, float context_score) {
  unit_table_ = std::make_shared<fst::SymbolTable>();
  for (const auto& unit : unit_table) {
    unit_table_->AddSymbol(unit.first, unit.second);
  }

  for (const auto& context : contexts) {
    contexts_[context] = context_score;
  }

  LOG(INFO) << "Num of contexts: " << contexts_.size();
  if (contexts_.empty()) {
    graph_.reset();
    return;
  }

  // Build context graph
  BuildContextGraph();
  graph_->Write("contexts.fst");
}

// Split the context into unit ids
bool ContextGraph::SplitContextToUnits(const std::string& context,
                                       std::vector<int>* units) {
  std::vector<std::string> chars;
  SplitUTF8StringToChars(context, &chars);
  UTF8CharToUpper(&chars);

  bool no_oov = true;
  for (size_t start = 0; start < chars.size();) {
    for (size_t end = chars.size(); end > start; --end) {
      std::string unit;
      for (size_t i = start; i < end; i++) {
        unit += chars[i];
      }

      int unit_id = unit_table_->Find(unit);
      if (unit_id != -1) {
        units->emplace_back(unit_id);
        start = end;
        continue;
      }

      if (end == start + 1) {
        ++start;
        if (unit == " ") continue;
        no_oov = false;
        LOG(WARNING) << unit << " is oov.";
      }
    }
  }
  return no_oov;
}

int ContextGraph::TraceContext(int cur_state, int unit_id, int* final_state) {
  CHECK_GE(cur_state, 0);
  int next_state = 0;
  Matcher matcher(*graph_, fst::MATCH_INPUT);
  matcher.SetState(cur_state);
  if (matcher.Find(unit_id)) {
    next_state = matcher.Value().nextstate;
    if (graph_->Final(next_state) != Weight::Zero()) {
      *final_state = next_state;
    }
    return next_state;
  }
  LOG(FATAL) << "Trace context failed.";
}

void ContextGraph::BuildContextGraph() {
  // Split context phrase into units according to the `unit_table_`
  std::unordered_map<std::string, std::vector<int>> context_units;
  for (const auto& context : contexts_) {
    std::vector<int> units;
    bool no_oov = SplitContextToUnits(context.first, &units);
    if (!no_oov) {
      LOG(WARNING) << "Ignore unknown unit found during compilation.";
      continue;
    }
    context_units[context.first] = units;
  }

  // Build the context graph
  std::unique_ptr<fst::StdVectorFst> ofst(new fst::StdVectorFst());
  int start_state = ofst->AddState();
  ofst->SetStart(start_state);
  for (const auto& context : contexts_) {
    if (context_units.count(context.first) == 0) continue;
    std::vector<int> units = context_units[context.first];
    int state = start_state;
    int next_state = state;
    for (size_t i = 0; i < units.size(); ++i) {
      next_state = ofst->AddState();
      if (i == units.size() - 1) {
        ofst->SetFinal(next_state, Weight::One());
      }
      ofst->AddArc(state,
                   fst::StdArc(units[i], units[i], context.second, next_state));
      state = next_state;
    }
  }
  graph_ = std::unique_ptr<fst::StdVectorFst>(new fst::StdVectorFst());
  // input/output label are sorted after Determinize
  fst::Determinize(*ofst, graph_.get());

  // Determinize will change the final state id
  for (const auto& context : contexts_) {
    if (context_units.count(context.first) == 0) continue;
    std::vector<int> units = context_units[context.first];
    int final_state = -1;
    int cur_state = 0;
    for (int unit : units) {
      cur_state = TraceContext(cur_state, unit, &final_state);
    }
    CHECK_GT(final_state, 0);
    context_table_[final_state] = context.first;
  }

  // Convert context graph to AC automaton
  ConvertToAC();
}

void ContextGraph::ConvertToAC() {
  CHECK(graph_ != nullptr) << "Context graph should not be nullptr!";
  int num_states = graph_->NumStates();
  std::vector<int> fail_states(num_states, 0);
  std::vector<float> total_weights(num_states, 0);
  Matcher matcher(*graph_, fst::MATCH_INPUT);
  // start state
  fail_states[0] = -1;
  total_weights[0] = 0;

  // Please see:
  // https://web.stanford.edu/group/cslipublications/cslipublications/koskenniemi-festschrift/9-mohri.pdf
  std::queue<int> states_queue;
  states_queue.push(0);
  while (!states_queue.empty()) {
    int state = states_queue.front();
    states_queue.pop();

    for (ArcIterator aiter(*graph_, state); !aiter.Done(); aiter.Next()) {
      const fst::StdArc& arc = aiter.Value();
      int next_state = arc.nextstate;
      total_weights[next_state] = total_weights[state] + arc.weight.Value();
      // Backtracking the failure state for next_state
      for (int fail_state = fail_states[state]; fail_state != -1;
           fail_state = fail_states[fail_state]) {
        matcher.SetState(fail_state);
        if (matcher.Find(arc.ilabel)) {
          fail_states[next_state] = matcher.Value().nextstate;
          break;
        }
      }
      states_queue.push(next_state);
    }
  }

  // Compute fail weight, add fail arc
  for (int state = 0; state < num_states; state++) {
    int fail_state = fail_states[state];
    if (fail_state < 0) continue;
    if (graph_->Final(fail_state) != Weight::Zero()) {
      fallback_finals_[state] = fail_state;
      if (graph_->NumArcs(fail_state) == 0) continue;
    }
    if (graph_->Final(state) != Weight::Zero() && fail_state == 0) continue;

    float fail_weight = total_weights[fail_state] - total_weights[state];
    if (graph_->Final(state) != Weight::Zero()) {
      fail_weight = 0;
    }
    graph_->AddArc(state, fst::StdArc(0, 0, fail_weight, fail_state));
  }
  // Sort arcs by ilabel, means move the fallback arc from last to first for the
  // matcher
  fst::ArcSort(graph_.get(), fst::ILabelCompare<fst::StdArc>());
}

int ContextGraph::GetNextState(int cur_state, int unit_id, float* score,
                               std::unordered_set<std::string>* contexts) {
  CHECK_GE(cur_state, 0);
  // Find(0) matches any epsilons on the underlying FST explicitly
  CHECK_NE(unit_id, 0);
  int next_state = 0;

  Matcher matcher(*graph_, fst::MATCH_INPUT);
  matcher.SetState(cur_state);
  if (matcher.Find(unit_id)) {
    const fst::StdArc& arc = matcher.Value();
    next_state = arc.nextstate;
    *score += arc.weight.Value();
    // Collect all contexts in the decode result
    if (contexts != nullptr) {
      if (graph_->Final(next_state) != Weight::Zero()) {
        contexts->insert(context_table_[next_state]);
      }
      int fallback_final = next_state;
      while (fallback_finals_.count(fallback_final) > 0) {
        fallback_final = fallback_finals_[fallback_final];
        contexts->insert(context_table_[fallback_final]);
      }
    }

    // Leaves go back to the start state
    if (graph_->NumArcs(next_state) == 0) {
      return 0;
    }
    return next_state;
  }

  // Check whether the first arc is fallback arc
  ArcIterator aiter(*graph_, cur_state);
  const fst::StdArc& arc = aiter.Value();
  // The start state has no fallback arc
  if (arc.ilabel == 0) {
    next_state = arc.nextstate;
    *score += arc.weight.Value();
    // fallback
    return GetNextState(next_state, unit_id, score);
  }

  return 0;
}

}  // namespace asr
