#include <iostream>
#include <unordered_map>
#include <vector>
#include <fstream>

using namespace std;

struct ruleOperation {
    string UNIT_TO_ADD;
    bool NEW_LINE = false;
    string ENTER_STATE;
    int GO_BACK = 0;
};

//Variable definitions
unordered_map<string, string> regex;    // <regexName, regex>
// key = state ; value = vector of <regex, ruleOperation> pairs
unordered_map<string, vector<pair<string, ruleOperation>>> rules;
vector<string> units;
unordered_map<string, vector<pair<unordered_map<int, unordered_map<char, vector<int>>>, ruleOperation>>> automatas;
// automatas<lexStateName, listOf<pair<map<state, transition<input, listOf<newStates>>>, ruleOperation>>>

/*
Converting operators to single characters and removing escapes to simplify the rest of the process
    ( -2
    ) -3
    { -4
    } -5
    | -6
    * -7
    $ -8
    \n -9
*/
void convertOperators(string& s) {
    //Replace \n \t \_ with -9 tab space
    bool oddBackslashes = false;
    string newlineReplacement = " ";
    newlineReplacement[0] = -9;
    for (size_t i = 0 ; i < s.length() ; i++) {
        if (s[i] == '\\') oddBackslashes = !oddBackslashes;
        else if (oddBackslashes) {
            oddBackslashes = false;
            if (s[i] == 'n') s.replace(i-1, 2, newlineReplacement);
            else if (s[i] == 't') s.replace(i-1, 2, "\t");
            else if (s[i] == '_') s.replace(i-1, 2, " ");
            i--;
        }
        else oddBackslashes = false;
    }

    //Convert operators to single characters
    char operators[] = {'(', ')', '{', '}', '|', '*', '$'};
    for (size_t i = 0 ; i < s.length() ; i++) {
        for (size_t j = 0 ; j < sizeof(operators) ; j++) {
            if (s[i] == operators[j] && (i == 0 || s[i-1] != '\\')) {
                s[i] = -j-2;
            }
        }
    }

    //Remove escapes
    for (size_t i = 0 ; i < s.length() ; i++) {
        if (s[i] == '\\') s.erase(i, 1);
    }
}

void extractNestedRegex(string& s) {
    size_t startPos = 0;
    size_t endPos;
    while ((startPos = s.find(-4, startPos)) != string::npos) {
            endPos = s.find(-5, startPos+1);
            string replacement = regex[s.substr(startPos+1, endPos-startPos-1)];
            replacement = ((char)-2) + replacement + ((char)-3);
            s.replace(startPos, endPos-startPos+1, replacement);
            startPos = 0;
        }
}

//Add new state (int) and return it
int newState(unordered_map<int, unordered_map<char, vector<int>>> &automata) {
    int state = automata.size();
    automata[state] = unordered_map<char, vector<int>>();
    return state;
}

//Transform regex into Epsilon-NFA
pair<int, int> transform(string reg, unordered_map<int, unordered_map<char, vector<int>>> &automata) {
    //Split regex by |
    vector<string> parts;
    int bracketLevel = 0;
    size_t startPos = 0;
    for (size_t i = 0 ; i < reg.length() ; i++) {
        if (reg[i] == -2) bracketLevel++;
        else if (reg[i] == -3) bracketLevel--;
        else if (bracketLevel == 0 && reg[i] == -6) {
            parts.push_back(reg.substr(startPos, i-startPos));
            startPos = i + 1;
        }
    }

    if (parts.size() > 0) parts.push_back(reg.substr(startPos));
    int leftState = newState(automata);
    int rightState = newState(automata);
    
    if (parts.size() > 0) {
        for (size_t i = 0 ; i < parts.size() ; i++) {
            pair<int, int> temp = transform(parts[i], automata);
            automata[leftState][-8].push_back(temp.first);
            automata[temp.second][-8].push_back(rightState);
        }
    }
    else {
        int lastState = leftState;
        for (size_t i = 0 ; i < reg.length() ; i++) {
            int a, b;
            
            //Concatenation
            if (reg[i] != -2) {
                a = newState(automata);
                b = newState(automata);
                if (reg[i] == -8) automata[a][-8].push_back(b);
                else automata[a][reg[i]].push_back(b);
            }
            //Brackets
            else {
                int j = i + 1;
                bracketLevel = 1;
                while (true) {
                    if (reg[j] == -2) bracketLevel++;
                    else if (reg[j] == -3) {
                        bracketLevel--;
                        if (bracketLevel == 0) break;
                    }
                    j++;
                }
                pair<int, int> temp = transform(reg.substr(i+1, j-i-1), automata);
                a = temp.first;
                b = temp.second;
                i = j;
            }

            //Kleen
            if (i+1 < reg.length() && reg[i+1] == -7) {
                int x = a;
                int y = b;
                a = newState(automata);
                b = newState(automata);
                automata[a][-8].push_back(x);
                automata[y][-8].push_back(b);
                automata[a][-8].push_back(b);
                automata[y][-8].push_back(x);
                i++;
            }
            //Connect with previous part
            automata[lastState][-8].push_back(a);
            lastState = b;
        }
        automata[lastState][-8].push_back(rightState);
    } 
    
    return pair<int, int>(leftState, rightState);
}

int main() {
    string input;   //used for processing inputs
    size_t startPos, endPos;

/////////////////////////////
//Reading regular definitions

    while (getline(cin, input) && input[0] != '%') {    //read lines until line starting with %
        startPos = input.find('{');
        endPos = input.find('}');

        string regexName = input.substr(startPos+1, endPos-1);  //the part between {} is the name
        string regexValue = input.substr(endPos+2);             //the name part is followed by a space and then a value (regular expression)

        convertOperators(regexValue);
        startPos = 0;

        extractNestedRegex(regexValue);

        regex[regexName] = regexValue;
    }

///////////////////////////////
//Reading lexic analyzer states

    startPos = input.find(' ');
    string firstState;

    while (true) {
        endPos = input.find(' ', startPos+1);
        if (endPos == string::npos) {
            endPos = input.length();
            rules[input.substr(startPos+1, endPos-startPos-1)] = vector<pair<string, ruleOperation>>();
            if (firstState == "") firstState = input.substr(startPos+1, endPos-startPos-1);
            break;
        }

        rules[input.substr(startPos+1, endPos-startPos-1)] = vector<pair<string, ruleOperation>>();
        if (firstState == "") firstState = input.substr(startPos+1, endPos-startPos-1);
        startPos = endPos;
    }

//////////////////////////
//Reading lexic unit names

    getline(cin, input);
    startPos = input.find(' ');

    while (true) {
        endPos = input.find(' ', startPos+1);
        if (endPos == string::npos) {
            endPos = input.length();
            units.push_back(input.substr(startPos+1, endPos-startPos-1));
            break;
        }

        units.push_back(input.substr(startPos+1, endPos-startPos-1));
        startPos = endPos;
    }

//////////////////////////////
//Reading lexic analyzer rules

    while (getline(cin, input)) {
        //Extract state name
        endPos = input.find('>');
        string stateName = input.substr(1, endPos-1);

        //Extract regex
        input.erase(0, endPos+1);
        convertOperators(input);
        string regexRule = input;
        extractNestedRegex(regexRule);

        //Extract rule operations
        getline(cin, input);    //eat {
        ruleOperation ro;
        getline(cin, input);
        ro.UNIT_TO_ADD = input;
        while (getline(cin, input)) {
            if (input == "}") break;
            else if (input == "NOVI_REDAK") ro.NEW_LINE = true;
            else if (input.rfind("UDJI_U_STANJE", 0) == 0) ro.ENTER_STATE = input.substr(14);
            else if (input.rfind("VRATI_SE", 0) == 0) ro.GO_BACK = stoi(input.substr(9));
        }
        rules[stateName].push_back(make_pair(regexRule, ro));
    }

////////////////////////////////////////
//Generate an Epsilon-NFA for each regex
    for (auto &state : rules) {
        for (auto &rule : state.second) {
            string reg = rule.first;
            unordered_map<int, unordered_map<char, vector<int>>> automata;
            transform(reg, automata);
            automatas[state.first].push_back(make_pair(automata, rule.second));
        }
    }

/////////////////
//Convert to .txt
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
    ofstream output("./analizator/enfa.txt");

    output << firstState;

    for (auto lexState : automatas) {
        output << '\n' << lexState.first;
        for (auto p : lexState.second) {
            output << "\n{";
            bool isFirstState = true;
            for (auto state : p.first) {
                if (!isFirstState) output << '\n';
                output << '\n' << state.first;
                isFirstState = false;
                for (auto transition : state.second) {
                    output << '\n' << transition.first << '\n';
                    bool isFirstNewState = true;
                    for (auto nextState : transition.second) {
                        if (!isFirstNewState) output << ' ';
                        output << nextState;
                        isFirstNewState = false;
                    }
                }
            }
            output << "\n}";
            auto ro = p.second;
            output << '\n' << ro.UNIT_TO_ADD << '\n' << ro.NEW_LINE << '\n' << ro.ENTER_STATE << '\n' << ro.GO_BACK;
        }
        output << "\n-";
    }

    output.close();
    return 0;
}