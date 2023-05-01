export GLOG_logtostderr=1
export GLOG_v=2

cmake -B build -S src
cmake --build build

./build/hotword_main \
  --unit_path units.txt \
  --context_path contexts.txt \
  --context_score 3

fstdraw --portrait --osymbols=units.txt contexts.fst |\
  dot -Gdpi=300 -Tpng > contexts.png
