#include "profiler.hpp"

static map<string, int> countsMap;

void incrementCount(ObjectPtr obj) {
    ostringstream sout;
    sout << obj;
    string s = sout.str();
    map<string, int>::iterator i = countsMap.find(s);
    if (i == countsMap.end()) {
        countsMap[s] = 1;
    }
    else {
        i->second += 1;
    }
}

void displayCounts() {
    vector<pair<int, string> > counts;
    map<string,int>::iterator cmi = countsMap.begin();
    while (cmi != countsMap.end()) {
        counts.push_back(make_pair(cmi->second, cmi->first));
        ++cmi;
    }
    sort(counts.begin(), counts.end());
    for (unsigned i = 0; i < counts.size(); ++i) {
        std::cout << counts[i].second << " - " << counts[i].first << '\n';
    }
}
