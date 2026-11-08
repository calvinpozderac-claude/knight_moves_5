#pragma once
#include "../../src/eng.h"
#include <regex>
#include <sstream>
static inline int sqi(const string& s){ return (s[0]-'a') + 8*(s[1]-'1'); }
static vector<pair<int,int>> read_tour(const char* fn){
  ifstream f(fn); string l1,l2; getline(f,l1); getline(f,l2);
  vector<pair<int,int>> t; regex re("\\((\\d+), ([a-h][1-8])\\)");
  for (sregex_iterator it(l2.begin(), l2.end(), re), e; it!=e; ++it) t.push_back({stoi((*it)[1]), sqi((*it)[2])});
  return t;
}
// convert (wait, q) to (delta,q) by replay; returns false if illegal
static bool replay_moves(S& s, const vector<pair<int,int>>& t, int k, vector<pair<int,int>>* dm=nullptr){
  vector<pair<int,int>> mv;
  for (int i=0;i<k;i++){
    pinned_moves(s,3,true,mv); bool ok=false;
    for (auto& m: mv) if (m.second==t[i].second){ int n=group_n(s,s.pos); if (n*m.first==t[i].first){ if(dm) dm->push_back(m); apply_move(s,m.first,m.second); ok=true; break;} }
    if(!ok){ int p=s.pos,q=t[i].second,n=group_n(s,p);  // lns84: free sink (target in the knight's group) in the prefix
      if (s.alt[q]==s.alt[p] && n>0 && t[i].first%n==0){ if(dm) dm->push_back({t[i].first/n,q}); apply_move(s,t[i].first/n,q); ok=true; } }
    if(!ok){ fprintf(stderr,"replay fail at %d\n",i); return false; }
  }
  return true;
}
static string tour_str(const vector<pair<int,int>>& t){ string o; for(size_t i=0;i<t.size();i++){ if(i) o+=", "; o+="("+to_string(t[i].first)+", "+nm(t[i].second)+")"; } return o; }
