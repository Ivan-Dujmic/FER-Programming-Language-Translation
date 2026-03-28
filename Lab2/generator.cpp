#include <iostream>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <stack>

using namespace std;

struct enfaState {
    string left;
    vector<string> right;
    int dot;
    unordered_set<string> terminals;

    bool operator==(const enfaState& other) const {
    return 
        left == other.left && 
        right == other.right && 
        dot == other.dot && 
        terminals == other.terminals;
    }
};

ostream& operator<<(ostream& os, const enfaState& state) {
    os << state.left << ": ";
    os << "{";
    for (size_t i = 0; i < state.right.size(); ++i) {
        os << state.right[i];
        if (i < state.right.size() - 1)
            os << ", ";
    }
    os << "}, " << state.dot << ", ";
    os << "{";
    bool first = true;
    for (const auto& terminal : state.terminals) {
        if (!first) os << ", ";
        os << terminal;
        first = false;
    }
    os << "}";
    return os;
}

struct enfaStateHash {
    size_t operator()(const enfaState& state) const {
        size_t h1 = hash<string>()(state.left);
        size_t h2 = 0;
        for (const auto& str : state.right)
            h2 ^= hash<string>()(str) + 0x9e3779b9 + (h2 << 6) + (h2 >> 2);
        size_t h3 = hash<int>()(state.dot);
        
        size_t h4 = 0;
        vector<string> sortedTerminals(state.terminals.begin(), state.terminals.end());
        sort(sortedTerminals.begin(), sortedTerminals.end());
        for (const auto& str : sortedTerminals) {
            h4 ^= hash<string>()(str) + 0x9e3779b9 + (h4 << 6) + (h4 >> 2);
        }

        return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
    }
};

struct enfaStateSetHash {
    size_t operator()(const std::unordered_set<enfaState, enfaStateHash>& set) const {
        size_t hashValue = 0;
        
        // Iterate through all elements in the unordered_set
        for (const auto& state : set) {
            // Combine each state's hash into the overall hash value
            hashValue ^= enfaStateHash{}(state) << 1;
        }
        
        return hashValue;
    }
};

//Split string by delimiter into vector
vector<string> split(const string& str, char delimiter) {
    vector<string> parts;
    stringstream ss(str);
    string token;

    while (getline(ss, token, delimiter)) parts.push_back(token);

    return parts;
}

//Data structures
vector<string> symbols;
vector<string> terminalSymbols;
vector<string> nonTerminalSymbols;
unordered_map<string, vector<vector<string>>> grammar;
vector<pair<string, vector<string>>> grammarOrder;
string sync;
unordered_set<string> emptyNonTerminal;
unordered_map<string, unordered_map<string, bool>> startsWith;
unordered_map<string, unordered_set<string>> starts;
unordered_map<enfaState, unordered_map<string, vector<enfaState>>, enfaStateHash> enfa;
unordered_map<enfaState, unordered_set<enfaState, enfaStateHash>, enfaStateHash> epsilonClosures;
unordered_map<unordered_set<enfaState, enfaStateHash>, unordered_map<string, unordered_set<enfaState, enfaStateHash>>, enfaStateSetHash> dfa;
unordered_map<unordered_set<enfaState, enfaStateHash>, unordered_map<string, tuple<char, string, vector<string>>>, enfaStateSetHash> actionTable;   //tuple<action, left, right> (right=optional)
unordered_map<unordered_set<enfaState, enfaStateHash>, unordered_map<string, string>, enfaStateSetHash> newStateTable;

void recursiveDoTransition(enfaState state) {
    if (enfa.find(state) != enfa.end()) return;  //if we already processed this state
    if (state.dot == state.right.size()) {enfa[state]; return;}  //if the dot is at the end then just add this state to the enfa (but it won't have any transitions)
    enfaState newState = state;
    //if we read the symbol on the right of the dot then we move the dot to the right of it
    newState.dot++;
    string symbolOnRight = state.right[state.dot];
    enfa[state][symbolOnRight].push_back(newState);
    recursiveDoTransition(newState);
    //analyse non-terminal transitions after reading an epsilon
    if (grammar.find(symbolOnRight) != grammar.end()) {
        for (auto transition : grammar[symbolOnRight]) {
            unordered_set<string> terminals;
            //symbols that start the right sub-sequence
            size_t i = state.dot + 1;
            for (i ; i < state.right.size() ; i++) {
                if (grammar.find(state.right[i]) != grammar.end()) {
                    for (auto terminalToAdd : starts[state.right[i]]) terminals.insert(terminalToAdd);   //if non-terminal then add it's starts
                }
                else terminals.insert(state.right[i]);  //if terminal then add it directly (terminal only has itself in starts)

                if (emptyNonTerminal.find(state.right[i]) == emptyNonTerminal.end()) break; //add the first non-empty
            }

            if (i == state.right.size() && (state.dot+1 == state.right.size() || emptyNonTerminal.find(state.right[i-1]) != emptyNonTerminal.end())) {
                for (auto terminaltoAdd : state.terminals) terminals.insert(terminaltoAdd); //if the right sub-sequence can generate epsilon then keep the previous terminals as well
            }

            newState.left = symbolOnRight;
            newState.right = transition;
            newState.dot = 0;
            newState.terminals = terminals;

            enfa[state]["$"].push_back(newState);
            recursiveDoTransition(newState);
        }
    }
}

unordered_set<enfaState, enfaStateHash> recursiveEpsilonClosure(enfaState state, unordered_set<enfaState, enfaStateHash> &visited) {
    if (epsilonClosures.find(state) != epsilonClosures.end()) return epsilonClosures[state];
    if (visited.find(state) != visited.end()) return unordered_set<enfaState, enfaStateHash>();
   
    visited.insert(state);
    unordered_set<enfaState, enfaStateHash> newClosure = {state};

    for (auto newState : enfa[state]["$"]) {
        for (auto toAdd : recursiveEpsilonClosure(newState, visited)) newClosure.insert(toAdd);
    }
    visited.erase(visited.find(state));
    epsilonClosures[state] = newClosure;
    return newClosure;
}


int main() {
//Help variables
    string input;
    vector<string> list;
    vector<string> parts;
    bool found;
    bool change;
    string currentNonTerminal;

//Reading inputs
    getline(cin, input);    //Non-terminal symbols
    parts = split(input, ' ');
    list = {parts[1]};
    grammar["q0"].push_back(list);
    startsWith["q0"] = unordered_map<string, bool>();
    for (size_t i = 0 ; i < parts.size() ; i++) nonTerminalSymbols.push_back(parts[i]);
    for (size_t i = 0 ; i < parts.size() ; i++) symbols.push_back(parts[i]);
    for (size_t i = 1 ; i < parts.size() ; i++) grammar[parts[i]] = vector<vector<string>>();  //Prepare the map
    for (size_t i = 1 ; i < parts.size() ; i++) startsWith[parts[i]] = unordered_map<string, bool>();
    getline(cin, input);
    parts = split(input, ' ');
    for (size_t i = 0 ; i < parts.size() ; i++) terminalSymbols.push_back(parts[i]);
    for (size_t i = 0 ; i < parts.size() ; i++) symbols.push_back(parts[i]);
    getline(cin, sync);    //Sync terminal symbols
    sync = sync.substr(5);
    
    while (getline(cin, input)) {   //Productions
        if (input == "!") break;    //ERR
        if (input[0] != ' ') currentNonTerminal = input;
        else {
            parts = split(input.substr(1), ' ');
            if (parts[0] == "$") parts.erase(parts.begin());
            grammarOrder.push_back(make_pair(currentNonTerminal, parts));
            grammar[currentNonTerminal].push_back(parts);

        }
    }

//Empty non-terminal symbols
//unordered_set<string> emptyNonTerminal;
    for (auto nonTerminal : grammar) {
        for (auto production : nonTerminal.second) {
            if (production.size() == 0) {
                emptyNonTerminal.insert(nonTerminal.first);
                break;
            }
        }
    }

    change = true;
    while (change) {
        change = false;
        for (auto nonTerminal : grammar) {
            if (emptyNonTerminal.find(nonTerminal.first) != emptyNonTerminal.end()) continue; //if we know it's empty already, skip
            for (auto production : nonTerminal.second) {
                found = false;
                for (auto symbol : production) {
                    if (emptyNonTerminal.find(symbol) == emptyNonTerminal.end()) { //if a symbol on the right isn't in an empty then left isn't in an empty either
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    emptyNonTerminal.insert(nonTerminal.first);
                    change = true;
                }
            }
        }
    }

//Starts directly with
//unordered_map<string, unordered_map<string, bool>> startsWith;
    for (auto nonTerminal : grammar) {
        for (auto production : nonTerminal.second) {
            for (auto symbol : production) { //everything until and including first non empty is a direct start for nonTerminal
                startsWith[nonTerminal.first][symbol] = 1;
                if (grammar.find(symbol) == grammar.end()) starts[nonTerminal.first].insert(symbol);
                if (emptyNonTerminal.find(symbol) == emptyNonTerminal.end()) break;
            }
        }
    }

//Starts with
    for (auto symbol : startsWith) startsWith[symbol.first][symbol.first] = true;

    change = true;
    while (change) {
        change = false;
        for (auto left : startsWith) {    //leftSymbol = leftSymbol.first
            for (auto middle : left.second) {   //middleSymbol = middle.first
                for (auto right : startsWith[middle.first]) {   //rightSymbol = right.first
                    if (!startsWith[left.first][right.first]) {
                        startsWith[left.first][right.first] = true;
                        if (grammar.find(right.first) == grammar.end()) starts[left.first].insert(right.first);
                        change = true;
                    }
                }
            }
        }
    }

//ENFA
//unordered_map<enfaState, unordered_map<string, vector<enfaState>>> enfa;
    enfaState firstState;
    firstState.left = "q0";
    firstState.right = grammar["q0"][0];
    firstState.dot = 0;
    firstState.terminals.insert("%");   // % is the simbol for the end of sequence

    recursiveDoTransition(firstState);

    cout << "ENFA size: " << enfa.size() << '\n';   //ERR

//All ENFA epsilon closures
//unordered_map<enfaState, unordered_set<enfaState>> epsilonClosures;
    for (auto stateTransitionPair : enfa) {
        unordered_set<enfaState, enfaStateHash> visited;
        if (epsilonClosures.find(stateTransitionPair.first) == epsilonClosures.end()) recursiveEpsilonClosure(stateTransitionPair.first, visited);
    }

//ENFA -> DFA
//unordered_map<unordered_set<enfaState, enfaStateHash>, unordered_map<string, unordered_set<enfaState, enfaStateHash>>> dfa;
    stack<unordered_set<enfaState, enfaStateHash>> dfaStack;
    dfaStack.push(epsilonClosures[firstState]);
    unordered_set<enfaState, enfaStateHash> currentStateSet;

    while (!dfaStack.empty()) {
        currentStateSet = dfaStack.top();
        dfaStack.pop();

        dfa[currentStateSet];

        for (auto symbol : symbols) {
            unordered_set<enfaState, enfaStateHash> newStateSet;
            unordered_set<enfaState, enfaStateHash> newStateSetWithEpsilon;
            for (auto state : currentStateSet) {
                for (auto toAdd : enfa[state][symbol]) newStateSet.insert(toAdd);
                for (auto addedState : newStateSet) for (auto toAdd : epsilonClosures[addedState]) newStateSetWithEpsilon.insert(toAdd);
            }
            if (dfa.find(newStateSetWithEpsilon) == dfa.end()) {
                dfa[currentStateSet][symbol] = newStateSetWithEpsilon;
                dfaStack.push(newStateSetWithEpsilon);
            }
        }
    }

    cout << "DFA size: " << dfa.size() << '\n'; //ERR

//LR(1) parser table
//unordered_map<unordered_set<enfaState, enfaStateHash>, unordered_map<string, unordered_set<enfaState, enfaStateHash>>> dfa;
//unordered_map<unordered_set<enfaState, enfaStateHash>, unordered_map<string, tuple<char, string, vector<string>>>, enfaStateSetHash> actionTable;
//unordered_map<unordered_set<enfaState, enfaStateHash>, unordered_map<string, string>, enfaStateSetHash> newStateTable;
    //action table
    for (auto dfaStatePair : dfa) {
        for (auto state : dfaStatePair.first) {
            if (state.dot != state.right.size()) {  //move
                
            }
            else {  //reduce

            }
        }
    }

    return 0;
}