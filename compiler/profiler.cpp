#include "clay.hpp"
#include "profiler.hpp"
#include "printer.hpp"

namespace clay {

static llvm::StringMap<int> countsMap;

void incrementCount(ObjectPtr obj) {
    string buf;
    llvm::raw_string_ostream sout(buf);
    sout << obj;
    string s = sout.str();
    llvm::StringMap<int>::iterator i = countsMap.find(s);
    if (i == countsMap.end()) {
        countsMap[s] = 1;
    }
    else {
        i->second += 1;
    }
}

void displayCounts() {
    vector<pair<int, string> > counts;
    llvm::StringMap<int>::iterator cmi = countsMap.begin();
    while (cmi != countsMap.end()) {
        counts.push_back(make_pair(cmi->getValue(), cmi->getKey()));
        ++cmi;
    }
    sort(counts.begin(), counts.end());
    for (size_t i = 0; i < counts.size(); ++i) {
        llvm::outs() << counts[i].second << " - " << counts[i].first << '\n';
    }
}

}
