#include <iostream>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <stack>
#include <algorithm>

using namespace std;

struct ruleOperation {
    string UNIT_TO_ADD;
    bool NEW_LINE = false;
    string ENTER_STATE;
    int GO_BACK = 0;
};

unordered_map<string, vector<pair<unordered_map<int, unordered_map<char, vector<int>>>, ruleOperation>>> automatas;
// automatas<lexStateName, listOf<pair<transitions<state, transition<input, listOf<newStates>>>, ruleOperation>>>
string allInput;

void doTransition(unordered_map<int, unordered_map<char, vector<int>>> &automata, stack<int> &stateStack, vector<bool> &X, vector<bool> &Y, char input) {
    for (size_t i = 0 ; i < X.size() ; i++) {
        if (X[i]) {
            for (int newState : automata[i][input]) {
                stateStack.push(newState);
                Y[newState] = true;
            }
        }
    }

    //Get epsilon closure
    while (!stateStack.empty()) {
        int top = stateStack.top();
        stateStack.pop();
        for (int newState : automata[top][-8]) {
            if (!(Y[newState])) {
                stateStack.push(newState);
                Y[newState] = true;
            }
        }
    }

    //zero everything in X and then swap X and Y so that X once again contains the current states and Y is zeroed
    fill(X.begin(), X.end(), false);
    swap(X, Y);
}

//returns the number of characters that were recognized by the automata
int simulateENFA(unordered_map<int, unordered_map<char, vector<int>>> &automata, int startP) {
    //Initialize a stack and 2 bit vectors
    stack<int> stateStack;
    vector<bool> X(automata.size(), false); //contains the current states
    vector<bool> Y(automata.size(), false); //is always empty here, but is used in doTransition
    X[0] = true;

    //Get epsilon closure
    doTransition(automata, stateStack, X, Y, -8);

    //Take inputs
    int inputsRecognized = 0;
    size_t currentP = startP;
    
    while(currentP < allInput.length()) {
        if (any_of(X.begin(), X.end(), [](bool val) {return val;})) {   //if we are currently in at least one state
            doTransition(automata, stateStack, X, Y, allInput[currentP]);
            currentP++;
            if (X[1]) inputsRecognized = currentP - startP; //if we are in the acceptable state (always and only state 1)
        }
        else {  //no longer in any state, stop
            break;
        }
    }
    
    return inputsRecognized;
}

int main() {
////////////////
//Read from .txt
/*
    STARTING STATE
    LexState
    {
    automata state
    input(doesn't exist if no transitions)
    nextState1 nextState2 ... (doesn't exist if no transitions - only state 1)
    (empty line)
    ...
    }
    string UNIT_TO_ADD
    bool NEW_LINE(0/1)
    string ENTER_STATE
    int GO_BACK
    ... ( more automatas)
    - (dash before next LexState)
*/
    ifstream inputFile("enfa.txt");

    string startingState;
    getline(inputFile, startingState);

    string read;
    while (getline(inputFile, read)) {  //goes through all lex states
        string lexState = read;
        while(getline(inputFile, read) && read != "-") {    //goes through all automata in lex state
            unordered_map<int, unordered_map<char, vector<int>>> transitions;
            while (getline(inputFile, read)) {  //goes through all transitions in automata
                int state;
                char input;
                vector<int> newStates;
                state = stoi(read);
                getline(inputFile, read);
                if (read == "") {   //if no transition
                    transitions[state];
                }
                else {
                    if (read[0] == -9) input = '\n';
                    else input = read[0];
                    getline(inputFile, read);
                    size_t startPos = 0;
                    size_t endPos;
                    while (true) {  //goes through all new states in transition
                        endPos = read.find(' ', startPos);
                        bool willBreak = false;
                        if (endPos == string::npos) {
                            willBreak = true;
                            endPos = read.length();
                        }
                        newStates.push_back(stoi(read.substr(startPos, endPos-startPos)));
                        startPos = endPos+1;
                        if (willBreak) break;
                    }
                    transitions[state][input] = newStates;
                    getline(inputFile, read);   //eat empty line or recognize end of automata }
                    if (read == "}") break;
                }
            }
            ruleOperation ro;
            getline(inputFile, ro.UNIT_TO_ADD);
            getline(inputFile, read);
            ro.NEW_LINE = stoi(read);
            getline(inputFile, ro.ENTER_STATE);
            getline(inputFile, read);
            ro.GO_BACK = stoi(read);
            
            automatas[lexState].push_back(make_pair(transitions, ro));
        }
    }

    inputFile.close();

///////////////////////////////////
//Reading source code into a string
    getline(cin, allInput);
    while(getline(cin, read)) {
        allInput += '\n' + read;
    }

/////////
//Analyze
    int currentLine = 1;
    string currentState = startingState;
    size_t startP = 0;  //start of the non-analyzed part in allInput

    while (startP < allInput.length()) {
        int longestPrefix = 0;  //holds the length of the longest recognized leftover input prefix
        int longestPrefixENFA = -1;  //holds the index of the automataOperator pair which has the longestPrefix
        for (size_t i = 0 ; i < automatas[currentState].size() ; i++) {    //simulate all ENFAs for this lexic state
            int prefixLength = simulateENFA(automatas[currentState][i].first, startP);
            if (prefixLength > longestPrefix) {
                longestPrefix = prefixLength;
                longestPrefixENFA = i;
            }
        }

        if (longestPrefixENFA != -1) {  //not error
            ruleOperation ro = automatas[currentState][longestPrefixENFA].second;

            if (ro.UNIT_TO_ADD != "-") {
                if (ro.GO_BACK) cout << ro.UNIT_TO_ADD << ' ' << currentLine << ' ' << allInput.substr(startP, ro.GO_BACK) << '\n';
                else cout << ro.UNIT_TO_ADD << ' ' << currentLine << ' ' << allInput.substr(startP, longestPrefix) << '\n';   
            }

            if (ro.GO_BACK) startP += ro.GO_BACK;
            else startP += longestPrefix;
            
            if (ro.NEW_LINE) currentLine++;
            if (ro.ENTER_STATE != "") currentState = ro.ENTER_STATE;
        }
        else {
            cerr << allInput[startP];  
            startP++;
        }
    }
    
    return 0;
}