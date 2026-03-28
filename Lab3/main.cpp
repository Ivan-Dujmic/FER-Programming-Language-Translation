#include <iostream>
#include <memory>
#include <vector>
#include <map>

using namespace std;

enum class Type {
    NONE,
    CHAR,
    INT,
    VOID
};

string typeToString(Type type) {
    switch (type) {
        case Type::CHAR: return "CHAR";
        case Type::INT: return "INT";
        case Type::VOID: return "VOID";
        default: return "NONE";
    }
}

Type stringToType(string str) {
    if (str == "CHAR") return Type::CHAR;
    else if (str == "INT") return Type::INT;
    else if (str == "VOID") return Type::VOID;
    else return Type::NONE;
}

struct Object {
    Type type = Type::NONE;

    bool con = false;           
    bool array = false;

    bool isFunction = false;
    bool isDefined = false;
    Type returnType = Type::NONE;
    string postfixName = "";
    vector<Object> parameters;
};

ostream& operator<<(ostream& os, const Object& obj) {
    os << "Type: " << typeToString(obj.type) << ", "
        << "Const: " << obj.con << ", "
        << "Array: " << obj.array << ", "
        << "IsFunction: " << obj.isFunction << ", "
        << "IsDefined: " << obj.isDefined << ", "
        << "ReturnType: " << typeToString(obj.returnType) << ", "
        << "PostfixName: " << obj.postfixName << ", "
        << "Parameters: [";
    for (const auto& param : obj.parameters) {
        os << param << ", ";
    }
    os << "]\n";
    return os;
}

struct Block : enable_shared_from_this<Block> {
    map<string, Object> table;
    Type function = Type::NONE;
    weak_ptr<Block> parent;
    vector<shared_ptr<Block>> children;
};


struct Node {
    string symbol = "";
    virtual ~Node() = default;
};

struct Branch : Node {
    Object type;
    Object nType;
    bool lValue = false;
    int amount = 0;
    vector<Object> arguments;
    vector<string> argumentNames;
    vector<unique_ptr<Node>> children;
};

struct Leaf : Node {
    string line = "";
    string data = "";
};

bool canImplicit(const Object& from, const Object& to) {
    if (!to.isFunction) {
        if (!from.array && !to.array) {
            if ((from.type == Type::CHAR && to.type == Type::CHAR) ||
                (from.type == Type::INT && to.type == Type::INT) ||
                (from.type == Type::CHAR && to.type == Type::INT)) return true;
        }
        if (from.array && !from.con && to.array) {
            if ((from.type == Type::CHAR && to.type == Type::CHAR) ||
                (from.type == Type::INT && to.type == Type::INT) ||
                (from.type == Type::CHAR && to.type == Type::INT)) return true;
        }
    }
    return false;
}

bool canExplicit(const Object& from, const Object& to) {
    if (from.type == Type::INT && to.type == Type::CHAR) return true;
    else return canImplicit(from, to);
}

bool isValidInt(string str, bool minus) {
    try {
        if (minus) str = "-" + str;
        stoi(str, nullptr, 0);
    } catch (const exception& e) { return false; }
    return true;
}

bool isValidSpecial(char c) {
    switch (c) {
        case 't': // newline
        case 'n': // tab
        case '0': // backslash
        case '\'': // single quote
        case '\"': // double quote
        case '\\': // null character
            return true;
        default:
            return false;
    }
}

bool isValidChar(string str) {
    if (str.length() == 3) {
        if (str[0] == '\'' && str[1] != '\'' && str[2] == '\'') return true;
        else return false;
    }
    else if (str.length() == 4) {
        if (str[0] == '\'' && str[1] != '\'' && isValidSpecial(str[2]) && str[3] == '\'') return true;
        else return false;
    }
    else return false;
}

int isValidCharArray(string str) {  // Returns array size if valid, -1 otherwise
    str = str.substr(1, str.length() - 2);
    int count = 0;

    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '\\') {
            if (str.length() == i + 1) return -1;
            else if (isValidSpecial(str[i + 1])) {
                i++;
                count++;
            }
            else return -1;
        }
        else count++;
    }
    return count;   // No +1 for the null character because we do that in the main program
}

bool isLValue(const Object& obj) {
    if (!obj.isFunction) {
        return !obj.con && !obj.array;
    }
    else return false;
}

bool isValidArraySize(string str) {
    try {
        int num = stoi(str);
        if (num <= 0 || num > 1024) return false;
    } catch (const exception& e) { return false; }
    return true;
}

string input;
bool stop = false;
void loadGenTree(Branch &branch, size_t depth) {
    while (!stop && (!input.empty() || getline(cin, input))) { // If input is empty, try to get a new line
        if (input.empty()) { stop = true; return; }
        size_t nextDepth = input.find_first_not_of(' ');
        if (nextDepth > depth) {
            input = input.substr(nextDepth);
            if (input[0] == '<') {
                auto newBranch = make_unique<Branch>();
                newBranch->symbol = input;
                input.clear();
                Branch& branchRef = *newBranch;
                branch.children.push_back(move(newBranch));
                loadGenTree(branchRef, nextDepth);
            } else {
                auto newLeaf = make_unique<Leaf>();
                size_t firstSpace = input.find(' ');
                size_t secondSpace = input.find(' ', firstSpace + 1);
                newLeaf->symbol = input.substr(0, firstSpace);
                newLeaf->line = input.substr(firstSpace + 1, secondSpace - firstSpace - 1);
                newLeaf->data = input.substr(secondSpace + 1);
                input.clear();
                branch.children.push_back(move(newLeaf));
            }
        }
        else return;
    }
}

void printError(Node &node) {
    Branch* branch = dynamic_cast<Branch*>(&node);
    cout << branch->symbol << " ::=";
    for (auto& child : branch->children) {
        if (Branch* childBranch = dynamic_cast<Branch*>(child.get())) {
            cout << " " << childBranch->symbol;
        } else {
            Leaf* childLeaf = dynamic_cast<Leaf*>(child.get());
            cout << " " << childLeaf->symbol << '(' << childLeaf->line << ',' << childLeaf->data << ')';
        }
    }
    cout << '\n';
    exit(0);
}

shared_ptr<Block> scopeTree = make_shared<Block>();
vector<string> paramNamesBuffer;
vector<Object> paramsBuffer;
Type functionTypeBuffer;
bool minusBuffer = false;
void resolveTree(Node &node, Block* currentScope, bool inLoop) {
    Branch* branch = dynamic_cast<Branch*>(&node);
    if (branch->symbol == "<primarni_izraz>") {
        switch (branch->children.size()) {
            case 1: {
                Leaf* child0 = dynamic_cast<Leaf*>(branch->children[0].get());
                if (child0->symbol == "IDN") {
                    Block* scopeCheck = currentScope;
                    bool exists = false;
                    map<std::string, Object>::iterator it;
                    do {
                        it = scopeCheck->table.find(child0->data);
                        if (it != scopeCheck->table.end()) {
                            exists = true;
                            break;
                        }
                    } while ((scopeCheck = scopeCheck->parent.lock().get()));

                    if (exists) {
                        if (!it->second.isFunction) branch->type = scopeCheck->table[child0->data];
                        else {
                            Object obj;
                            obj.isFunction = true;
                            obj.type = Type::NONE;
                            obj.postfixName = child0->data;
                            obj.returnType = scopeCheck->table[child0->data].type;
                            obj.parameters = scopeCheck->table[child0->data].parameters;
                            branch->type = obj;
                        }
                        branch->lValue = isLValue(scopeCheck->table[child0->data]);
                    } else printError(*branch);
                } else if (child0->symbol == "BROJ") {
                    if (isValidInt(child0->data, minusBuffer)) {
                        minusBuffer = false;
                        Object newNum;
                        newNum.type = Type::INT;
                        branch->type = newNum;
                        branch->lValue = false;
                    } else printError(*branch);
                } else if (child0->symbol == "ZNAK") {
                    if (isValidChar(child0->data)) {
                        Object newNum;
                        newNum.type = Type::CHAR;
                        branch->type = newNum;
                        branch->lValue = false;
                    } else printError(*branch);
                } else if (child0->symbol == "NIZ_ZNAKOVA") {
                    branch->amount = isValidCharArray(child0->data);
                    if (branch->amount != -1) {
                        Object newNum;
                        newNum.type = Type::CHAR;
                        newNum.con = true;
                        newNum.array = true;
                        branch->type = newNum;
                        branch->lValue = false;
                    } else printError(*branch);
                }
                break;
            }
            case 3: {
                Branch* child1 = dynamic_cast<Branch*>(branch->children[1].get());
                resolveTree(*child1, currentScope, inLoop);
                branch->type = child1->type;
                branch->lValue = child1->lValue;
                break;
            }
        }
    }
    else if (branch->symbol == "<postfiks_izraz>") {
        Object retNum;
        retNum.type = Type::INT;
        switch (branch->children.size()) {
            case 1: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                resolveTree(*child0, currentScope, inLoop);
                branch->type = child0->type;
                branch->lValue = child0->lValue;
                break;
            }
            case 2: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                resolveTree(*child0, currentScope, inLoop);
                if (!child0->lValue || !canImplicit(child0->type, retNum)) printError(*branch);
                branch->type = retNum;
                branch->type.postfixName = child0->type.postfixName;
                branch->lValue = false;
                break;
            }
            case 3: {   // This is for function calls
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                resolveTree(*child0, currentScope, inLoop);
                if (!child0->type.isFunction) printError(*branch);
                if (child0->type.parameters.size() != 0) printError(*branch);

                if (child0->type.postfixName != "") {
                    auto scopeCheck = currentScope;
                    do {
                        if (scopeCheck->table.find(child0->type.postfixName) != scopeCheck->table.end()) {
                            Object &func = scopeCheck->table[child0->type.postfixName];
                            if (!func.isFunction || func.parameters.size() != 0) {
                                printError(*branch);
                                break;
                            }
                        }
                    } while ((scopeCheck = scopeCheck->parent.lock().get()));
                } else printError(*branch);
                retNum.type = child0->type.returnType;
                branch->type = retNum;
                break;
            }
            case 4: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                Branch* child2 = dynamic_cast<Branch*>(branch->children[2].get());
                if (child2->symbol == "<izraz>") {
                    resolveTree(*child0, currentScope, inLoop);
                    if (child0->type.isFunction) printError(*branch);
                    if (!child0->type.array) printError(*branch);
                    resolveTree(*child2, currentScope, inLoop);
                    Object castTo;
                    castTo.type = Type::INT;
                    if (!canImplicit(child2->type, castTo)) printError(*branch);
                    Object ret = child0->type;
                    ret.array = false;
                    branch->type = ret;
                    branch->type.postfixName = child0->type.postfixName;
                    branch->lValue = !ret.con;
                } else {
                    resolveTree(*child0, currentScope, inLoop);
                    resolveTree(*child2, currentScope, inLoop);
                    if (!child0->type.isFunction) printError(*branch);
                    vector<Object> args = child2->arguments;
                    vector<Object> params = child0->type.parameters;
                    if (args.size() != params.size()) printError(*branch);
                    for (size_t i = 0; i < args.size(); i++) {
                        if (!canImplicit(args[i], params[i])) printError(*branch);
                    }
                    retNum.type = child0->type.returnType;
                    branch->type = retNum;
                    branch->type.postfixName = child0->type.postfixName;
                    branch->lValue = false;
                }
                break;
            }
        }
    }
    else if (branch->symbol == "<lista_argumenata>") {
        Object newNum;
        switch (branch->children.size()) {
            case 1: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                resolveTree(*child0, currentScope, inLoop);
                newNum = child0->type;
                branch->arguments.push_back(newNum);
                break;
            }
            case 3: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                Branch* child2 = dynamic_cast<Branch*>(branch->children[2].get());
                resolveTree(*child0, currentScope, inLoop);
                resolveTree(*child2, currentScope, inLoop);
                newNum = child2->type;
                branch->arguments = child0->arguments;
                branch->arguments.push_back(newNum);
                break;
            }
        }
    }
    else if (branch->symbol == "<unarni_izraz>") {
        Object castTo;
        castTo.type = Type::INT;
        switch (branch->children.size()) {
            case 1: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                resolveTree(*child0, currentScope, inLoop);
                branch->type = child0->type;
                branch->lValue = child0->lValue;
                break;
            }
            case 2: {
                Branch* child1 = dynamic_cast<Branch*>(branch->children[1].get());
                if (child1->symbol == "<unarni_izraz>") {
                    resolveTree(*child1, currentScope, inLoop);
                    if (!isLValue(child1->type) || !canImplicit(child1->type, castTo)) printError(*branch);
                    branch->type = castTo;
                    branch->lValue = false;
                } else {
                    Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                    resolveTree(*child0, currentScope, inLoop);
                    resolveTree(*child1, currentScope, inLoop);
                    if (!canImplicit(child1->type, castTo)) printError(*branch);
                    branch->type = castTo;
                    branch->lValue = false;
                }
                break;
            }
        }
    }
    else if (branch->symbol == "<unarni_operator>") {
        // No checks, just set minus to true for upcoming number
        Leaf* child0 = dynamic_cast<Leaf*>(branch->children[0].get());
        if (child0->symbol == "MINUS") minusBuffer = true;
    }
    else if (branch->symbol == "<cast_izraz>") {
        Object newNum;
        switch (branch->children.size()) {
            case 1: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                resolveTree(*child0, currentScope, inLoop);
                branch->type = child0->type;
                branch->lValue = child0->lValue;
                break;
            }
            case 4: {
                Branch* child1 = dynamic_cast<Branch*>(branch->children[1].get());
                Branch* child3 = dynamic_cast<Branch*>(branch->children[3].get());
                resolveTree(*child1, currentScope, inLoop);
                resolveTree(*child3, currentScope, inLoop);
                if (!canExplicit(child3->type, child1->type)) printError(*branch);
                branch->type = child1->type;
                branch->lValue = false;
                break;
            }
        }
    }
    else if (branch->symbol == "<ime_tipa>") {
        switch (branch->children.size()) {
            case 1: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                resolveTree(*child0, currentScope, inLoop);
                branch->type = child0->type;
                break;
            }
            case 2: {
                Branch* child1 = dynamic_cast<Branch*>(branch->children[1].get());
                resolveTree(*child1, currentScope, inLoop);
                if (child1->type.type == Type::VOID) printError(*branch);
                Object retType = child1->type;
                retType.con = true;
                branch->type = retType;
                break;
            }
        }
    }
    else if (branch->symbol == "<specifikator_tipa>") {
        Object obj;
        Leaf* child0 = dynamic_cast<Leaf*>(branch->children[0].get());
        if (child0->symbol == "KR_VOID") obj.type = Type::VOID;
        else if (child0->symbol == "KR_CHAR") obj.type = Type::CHAR;
        else obj.type = Type::INT;
        branch->type = obj;
    }
    else if (branch->symbol == "<multiplikativni_izraz>" || 
            branch->symbol == "<aditivni_izraz>" || 
            branch->symbol == "<odnosni_izraz>" || 
            branch->symbol == "<jednakosni_izraz>" || 
            branch->symbol == "<bin_i_izraz>" || 
            branch->symbol == "<bin_xili_izraz>" || 
            branch->symbol == "<bin_ili_izraz>" || 
            branch->symbol == "<log_i_izraz>" || 
            branch->symbol == "<log_ili_izraz>") {
        Object castTo;
        castTo.type = Type::INT;
        switch (branch->children.size()) {
            case 1: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                resolveTree(*child0, currentScope, inLoop);
                branch->type = child0->type;
                branch->lValue = child0->lValue;
                break;
            }
            case 3: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                Branch* child2 = dynamic_cast<Branch*>(branch->children[2].get());
                resolveTree(*child0, currentScope, inLoop);
                if (!canImplicit(child0->type, castTo)) printError(*branch);
                resolveTree(*child2, currentScope, inLoop);
                if (!canImplicit(child2->type, castTo)) printError(*branch);
                branch->type = castTo;
                branch->lValue = false;
                break;
            }
        }
    }
    else if (branch->symbol == "<izraz_pridruzivanja>") {
        Object castTo;
        castTo.type = Type::INT;
        switch (branch->children.size()) {
            case 1: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                resolveTree(*child0, currentScope, inLoop);
                branch->type = child0->type;
                branch->lValue = child0->lValue;
                break;
            }
            case 3: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                Branch* child2 = dynamic_cast<Branch*>(branch->children[2].get());
                resolveTree(*child0, currentScope, inLoop);
                if (!child0->lValue) printError(*branch);
                resolveTree(*child2, currentScope, inLoop);
                if (!canImplicit(child2->type, child0->type)) printError(*branch);
                branch->type = child0->type;
                branch->lValue = false;
                break;
            }
        }
    }
    else if (branch->symbol == "<izraz>") {
        switch (branch->children.size()) {
            case 1: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                resolveTree(*child0, currentScope, inLoop);
                branch->type = child0->type;
                branch->lValue = child0->lValue;
                break;
            }
            case 3: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                Branch* child2 = dynamic_cast<Branch*>(branch->children[2].get());
                resolveTree(*child0, currentScope, inLoop);
                resolveTree(*child2, currentScope, inLoop);
                branch->type = child2->type;
                branch->lValue = false;
                break;
            }
        }
    }
    else if (branch->symbol == "<slozena_naredba>") {
        switch (branch->children.size()) {
            case 3: {
                Branch* child1 = dynamic_cast<Branch*>(branch->children[1].get());
                currentScope->children.push_back(make_shared<Block>());
                currentScope->children.back()->parent = currentScope->shared_from_this();
                currentScope = currentScope->children.back().get();
                if (functionTypeBuffer != Type::NONE) {
                    currentScope->function = functionTypeBuffer;
                    functionTypeBuffer = Type::NONE;
                }
                if (!paramsBuffer.empty()) {
                    for (size_t i = 0; i < paramsBuffer.size(); i++) {
                        if (currentScope->table.find(paramNamesBuffer[i]) != currentScope->table.end()) printError(*branch);
                        currentScope->table[paramNamesBuffer[i]] = paramsBuffer[i];
                    }
                    paramsBuffer.clear();
                    paramNamesBuffer.clear();
                }
                resolveTree(*child1, currentScope, inLoop);
                currentScope = currentScope->parent.lock().get();
                break;
            }
            case 4: {
                Branch* child1 = dynamic_cast<Branch*>(branch->children[1].get());
                Branch* child2 = dynamic_cast<Branch*>(branch->children[2].get());
                currentScope->children.push_back(make_shared<Block>());
                currentScope->children.back()->parent = currentScope->shared_from_this();
                currentScope = currentScope->children.back().get();
                if (functionTypeBuffer != Type::NONE) {
                    currentScope->function = functionTypeBuffer;
                    functionTypeBuffer = Type::NONE;
                }
                if (!paramsBuffer.empty()) {
                    for (size_t i = 0; i < paramsBuffer.size(); i++) {
                        if (currentScope->table.find(paramNamesBuffer[i]) != currentScope->table.end()) printError(*branch);
                        currentScope->table[paramNamesBuffer[i]] = paramsBuffer[i];
                    }
                    paramsBuffer.clear();
                    paramNamesBuffer.clear();
                }
                resolveTree(*child1, currentScope, inLoop);
                resolveTree(*child2, currentScope, inLoop);
                currentScope = currentScope->parent.lock().get();
                break;
            }
        }
    }
    else if (branch->symbol == "<lista_naredbi>") {
        switch (branch->children.size()) {
            case 1: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                resolveTree(*child0, currentScope, inLoop);
                break;
            }
            case 2: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                Branch* child1 = dynamic_cast<Branch*>(branch->children[1].get());
                resolveTree(*child0, currentScope, inLoop);
                resolveTree(*child1, currentScope, inLoop);
                break;
            }
        }
    }
    else if (branch->symbol == "<naredba>") {
        Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
        resolveTree(*child0, currentScope, inLoop);
    }
    else if (branch->symbol == "<izraz_naredba>") {
        Object num;
        num.type = Type::INT;
        switch(branch->children.size()) {
            case 1: {
                branch->type = num;
                break;
            }
            case 2: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                resolveTree(*child0, currentScope, inLoop);
                branch->type = child0->type;
                break;
            }
        }
    }
    else if (branch->symbol == "<naredba_grananja>") {
        Object num;
        num.type = Type::INT;
        switch (branch->children.size()) {
            case 5: {
                Branch* child2 = dynamic_cast<Branch*>(branch->children[2].get());
                Branch* child4 = dynamic_cast<Branch*>(branch->children[4].get());
                resolveTree(*child2, currentScope, inLoop);
                if (!canImplicit(child2->type, num)) printError(*branch);
                resolveTree(*child4, currentScope, inLoop);
                break;
            }
            case 7: {
                Branch* child2 = dynamic_cast<Branch*>(branch->children[2].get());
                Branch* child4 = dynamic_cast<Branch*>(branch->children[4].get());
                Branch* child6 = dynamic_cast<Branch*>(branch->children[6].get());
                resolveTree(*child2, currentScope, inLoop);
                if (!canImplicit(child2->type, num)) printError(*branch);
                resolveTree(*child4, currentScope, inLoop);
                resolveTree(*child6, currentScope, inLoop);
                break;
            }
        }
    }
    else if (branch->symbol == "<naredba_petlje>") {
        Object num;
        num.type = Type::INT;
        switch (branch->children.size()) {
            case 5: {
                Branch* child2 = dynamic_cast<Branch*>(branch->children[2].get());
                Branch* child4 = dynamic_cast<Branch*>(branch->children[4].get());
                resolveTree(*child2, currentScope, inLoop);
                if (!canImplicit(child2->type, num)) printError(*branch);
                resolveTree(*child4, currentScope, true);
                break;
            }
            case 6: {
                Branch* child2 = dynamic_cast<Branch*>(branch->children[2].get());
                Branch* child4 = dynamic_cast<Branch*>(branch->children[4].get());
                Branch* child5 = dynamic_cast<Branch*>(branch->children[5].get());
                resolveTree(*child2, currentScope, inLoop);
                resolveTree(*child4, currentScope, inLoop);
                if (!canImplicit(child4->type, num)) printError(*branch);
                resolveTree(*child5, currentScope, true);
                break;
            }
            case 7: {
                Branch* child2 = dynamic_cast<Branch*>(branch->children[2].get());
                Branch* child3 = dynamic_cast<Branch*>(branch->children[3].get());
                Branch* child4 = dynamic_cast<Branch*>(branch->children[4].get());
                Branch* child6 = dynamic_cast<Branch*>(branch->children[6].get());
                resolveTree(*child2, currentScope, inLoop);
                resolveTree(*child3, currentScope, inLoop);
                if (!canImplicit(child3->type, num)) printError(*branch);
                resolveTree(*child4, currentScope, inLoop);
                resolveTree(*child6, currentScope, true);
                break;
            }
        }
    }
    else if (branch->symbol == "<naredba_skoka>") {
        Block* scopeCheck = currentScope;
        bool exists = false;
        switch (branch->children.size()) {
            case 2: {
                if (branch->children[0]->symbol != "KR_RETURN")
                    { if (!inLoop) printError(*branch); }
                else {
                    do {
                        if (scopeCheck->function == Type::VOID) {
                            exists = true;
                            break;
                        }
                    } while ((scopeCheck = scopeCheck->parent.lock().get()));

                    if (!exists) printError(*branch);
                }
                break;
            }
            case 3: {
                Branch* child1 = dynamic_cast<Branch*>(branch->children[1].get());
                resolveTree(*child1, currentScope, inLoop);
                do {
                    if (scopeCheck->function == Type::CHAR || scopeCheck->function == Type::INT) {
                        Object funcType;
                        funcType.type = scopeCheck->function;
                        if (!canImplicit(child1->type, funcType)) printError(*branch);
                        exists = true;
                        break;
                    }
                } while ((scopeCheck = scopeCheck->parent.lock().get()));
                if (!exists) printError(*branch);
                break;
            }
        }
    }
    else if (branch->symbol == "<prijevodna_jedinica>") {
        switch (branch->children.size()) {
            case 1: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                resolveTree(*child0, currentScope, inLoop);
                break;
            }
            case 2: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                Branch* child1 = dynamic_cast<Branch*>(branch->children[1].get());
                resolveTree(*child0, currentScope, inLoop);
                resolveTree(*child1, currentScope, inLoop);
                break;
            }
        }
    }
    else if (branch->symbol == "<vanjska_deklaracija>") {
        Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
        resolveTree(*child0, currentScope, inLoop);
    }
    else if (branch->symbol == "<definicija_funkcije>") {
        Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
        Leaf* child1 = dynamic_cast<Leaf*>(branch->children[1].get());
        Branch* child5 = dynamic_cast<Branch*>(branch->children[5].get());
        if (branch->children[3]->symbol == "KR_VOID") {
            resolveTree(*child0, currentScope, inLoop);
            if (child0->type.type == Type::INT || child0->type.type == Type::CHAR)
                if (!child0->type.array && child0->type.con) printError(*branch);
            auto it = scopeTree->table.find(child1->data);
            if (it != scopeTree->table.end()) {
                Object &func = it->second;
                if (func.isFunction && func.isDefined) printError(*branch);
                if (!func.isFunction || func.type != child0->type.type || func.parameters.size() != 0) printError(*branch);
                func.isDefined = true;
                functionTypeBuffer = child0->type.type;
            } else {
                Object func;
                func.isFunction = true;
                func.type = child0->type.type;
                func.isDefined = true;
                functionTypeBuffer = child0->type.type;
                scopeTree->table[child1->data] = func;
            }
            resolveTree(*child5, currentScope, inLoop);
        } else {
            Branch* child3 = dynamic_cast<Branch*>(branch->children[3].get());
            resolveTree(*child0, currentScope, inLoop);
            if (child0->type.con) printError(*branch);
            auto it = scopeTree->table.find(child1->data);
            if (it != scopeTree->table.end()) {
                Object &func = it->second;
                if (func.isFunction && func.isDefined) printError(*branch);
                resolveTree(*child3, currentScope, inLoop);
                if (!func.isFunction || func.type != child0->type.type || func.parameters.size() != child3->arguments.size()) printError(*branch);
                for (size_t i = 0 ; i < func.parameters.size(); i++) {
                    if (func.parameters[i].type != child3->arguments[i].type) printError(*branch);
                }
                func.isDefined = true;
                
            } else {
                resolveTree(*child3, currentScope, inLoop);
                Object func;
                func.isFunction = true;
                func.type = child0->type.type;
                func.isDefined = true;
                func.parameters = child3->arguments;
                scopeTree->table[child1->data] = func;
            }
            functionTypeBuffer = child0->type.type;
            paramsBuffer = child3->arguments;
            paramNamesBuffer = child3->argumentNames;
            resolveTree(*child5, currentScope, inLoop);
        }
    }
    else if (branch->symbol == "<lista_parametara>") {
        switch (branch->children.size()) {
            case 1: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                resolveTree(*child0, currentScope, inLoop);
                branch->arguments.push_back(child0->type);
                branch->argumentNames = child0->argumentNames;
                break;
            }
            case 3: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                Branch* child2 = dynamic_cast<Branch*>(branch->children[2].get());
                resolveTree(*child0, currentScope, inLoop);
                resolveTree(*child2, currentScope, inLoop);
                for (string name : branch->argumentNames) {
                    if (name == child2->argumentNames[0]) printError(*branch);
                }
                branch->arguments = child0->arguments;
                branch->arguments.push_back(child2->type);
                branch->argumentNames = child0->argumentNames;
                branch->argumentNames.push_back(child2->argumentNames[0]);
                break;
            }
        }
    }
    else if (branch->symbol == "<deklaracija_parametra>") {
        switch(branch->children.size()) {
            case 2: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                Leaf* child1 = dynamic_cast<Leaf*>(branch->children[1].get());
                resolveTree(*child0, currentScope, inLoop);
                if (child0->type.type == Type::VOID) printError(*branch);
                branch->type = child0->type;
                branch->argumentNames.push_back(child1->data);
                break;
            }
            case 4: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                Leaf* child1 = dynamic_cast<Leaf*>(branch->children[1].get());
                resolveTree(*child0, currentScope, inLoop);
                if (child0->type.type == Type::VOID) printError(*branch);
                Object obj = child0->type;
                obj.array = true;
                branch->type = obj;
                branch->argumentNames.push_back(child1->data);
                break;
            }
        }
    }
    else if (branch->symbol == "<lista_deklaracija>") {
        switch (branch->children.size()) {
            case 1: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                resolveTree(*child0, currentScope, inLoop);
                break;
            }
            case 2: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                Branch* child1 = dynamic_cast<Branch*>(branch->children[1].get());
                resolveTree(*child0, currentScope, inLoop);
                resolveTree(*child1, currentScope, inLoop);
                break;
            }
        }
    }
    else if (branch->symbol == "<deklaracija>") {
        Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
        Branch* child1 = dynamic_cast<Branch*>(branch->children[1].get());
        resolveTree(*child0, currentScope, inLoop);
        child1->nType = child0->type;
        resolveTree(*child1, currentScope, inLoop);
    }
    else if (branch->symbol == "<lista_init_deklaratora>") {
        switch (branch->children.size()) {
            case 1: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                child0->nType = branch->nType;
                resolveTree(*child0, currentScope, inLoop);
                break;
            }
            case 3: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                Branch* child2 = dynamic_cast<Branch*>(branch->children[2].get());
                child0->nType = branch->nType;
                resolveTree(*child0, currentScope, inLoop);
                child2->nType = branch->nType;
                resolveTree(*child2, currentScope, inLoop);
                break;
            }
        }
    }
    else if (branch->symbol == "<init_deklarator>") {
        switch (branch->children.size()) {
            case 1: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                child0->nType = branch->nType;
                resolveTree(*child0, currentScope, inLoop);
                if (child0->type.con) printError(*branch);
                break;
            }
            case 3: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                Branch* child2 = dynamic_cast<Branch*>(branch->children[2].get());
                child0->nType = branch->nType;
                resolveTree(*child0, currentScope, inLoop);
                resolveTree(*child2, currentScope, inLoop);
                if ((child0->type.type == Type::INT || child0->type.type == Type::CHAR) && !child0->type.array) {
                    Object child0NoConst = child0->type;
                    child0NoConst.con = false;
                    if (!canImplicit(child2->type, child0NoConst)) printError(*branch);
                } else if ((child0->type.type == Type::INT || child0->type.type == Type::CHAR) && child0->type.array) {
                    if (child2->amount > child0->amount) printError(*branch);
                    Object child0NoConstNoArray = child0->type;
                    child0NoConstNoArray.con = false;
                    child0NoConstNoArray.array = false;
                    for (auto &elem : child2->arguments) {
                        if (!canImplicit(elem, child0NoConstNoArray)) printError(*branch);
                    }
                }
                break;
            }
        }
    }
    else if (branch->symbol == "<izravni_deklarator>") {
        switch (branch->children.size()) {
            case 1: {
                Leaf* child0 = dynamic_cast<Leaf*>(branch->children[0].get());
                if (branch->nType.type == Type::VOID) printError(*branch);
                if (currentScope->table.find(child0->data) != currentScope->table.end()) printError(*branch);
                currentScope->table[child0->data] = branch->nType;
                branch->type = branch->nType;
                break;
            }
            case 4: {
                Leaf* child0 = dynamic_cast<Leaf*>(branch->children[0].get());
                Leaf* child2 = dynamic_cast<Leaf*>(branch->children[2].get());
                if (branch->children[2].get()->symbol == "BROJ") {
                    if (branch->nType.type == Type::VOID) printError(*branch);
                    if (currentScope->table.find(child0->data) != currentScope->table.end()) printError(*branch);
                    if (!isValidArraySize(child2->data)) printError(*branch);
                    Object obj = branch->nType;
                    obj.array = true;
                    currentScope->table[child0->data] = obj;
                    branch->type = obj;
                    branch->amount = stoi(dynamic_cast<Leaf*>(branch->children[2].get())->data);
                } else if (branch->children[2].get()->symbol == "KR_VOID") {
                    auto it = currentScope->table.find(child0->data);
                    if (it != currentScope->table.end()) {
                        Object func = it->second;
                        if (!func.isFunction || func.parameters.size() != 0 || func.type != branch->nType.type) printError(*branch);
                    } else {
                        Object func;
                        func.isFunction = true;
                        func.type = branch->nType.type;
                        currentScope->table[child0->data] = func;
                    }
                    Object func;
                    func.isFunction = true;
                    func.type = branch->nType.type;
                    branch->type = func;
                } else {
                    Branch* child2 = dynamic_cast<Branch*>(branch->children[2].get());
                    resolveTree(*child2, currentScope, inLoop);
                    auto it = currentScope->table.find(child0->data);
                    if (it != currentScope->table.end()) {
                        Object func = it->second;
                        if (!func.isFunction || func.parameters.size() != child2->arguments.size() || func.type != branch->nType.type) printError(*branch);
                        for (size_t i = 0 ; i < func.parameters.size(); i++) {
                            if (func.parameters[i].type != child2->arguments[i].type) printError(*branch);
                        }
                    } else {
                        Object func;
                        func.type = branch->nType.type;
                        func.parameters = child2->arguments;
                        func.isFunction = true;
                        currentScope->table[child0->data] = func;
                    }
                    Object func;
                    func.type = branch->nType.type;
                    func.parameters = child2->arguments;
                    func.isFunction = true;
                    branch->type = func;
                }
                break;
            }
        }
    }
    else if (branch->symbol == "<inicijalizator>") {
        switch (branch->children.size()) {
            case 1: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                resolveTree(*child0, currentScope, inLoop);
                Branch* prev = child0;
                for (Branch* p = child0 ; p != nullptr && p->children.size() == 1 ; p = dynamic_cast<Branch*>(p->children[0].get())) {
                    prev = p;
                }
                Leaf* last = dynamic_cast<Leaf*>(prev->children[0].get());
                if (last != nullptr && last->symbol == "NIZ_ZNAKOVA") {
                    branch->amount = prev->amount + 1;
                    vector<Object> charArray;
                    for (int i = 0 ; i < branch->amount ; i++) {
                        Object num;
                        num.type = Type::CHAR;
                        charArray.push_back(num);
                    }
                    branch->arguments = charArray;
                } else {
                    branch->type = child0->type;
                }
                break;
            }
            case 3: {
                Branch* child1 = dynamic_cast<Branch*>(branch->children[1].get());
                resolveTree(*child1, currentScope, inLoop);
                branch->amount = child1->amount;
                branch->arguments = child1->arguments;
                break;
            }
        }
    }
    else if (branch->symbol == "<lista_izraza_pridruzivanja>") {
        switch (branch->children.size()) {
            case 1: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                resolveTree(*child0, currentScope, inLoop);
                branch->arguments.push_back(child0->type);
                branch->amount = 1;
                break;
            }
            case 3: {
                Branch* child0 = dynamic_cast<Branch*>(branch->children[0].get());
                Branch* child2 = dynamic_cast<Branch*>(branch->children[2].get());
                resolveTree(*child0, currentScope, inLoop);
                resolveTree(*child2, currentScope, inLoop);
                branch->arguments.push_back(child2->type);
                branch->amount = child0->amount + 1;
                break;
            }
        }
    }
}

void checkMain(Block& block) {
    auto it = block.table.find("main");
    if (it == block.table.end()) {
        cout << "main\n";
        exit(0);
    }
    Object mainFunc = it->second;
    if (!mainFunc.isFunction || mainFunc.type != Type::INT || !mainFunc.parameters.empty()) {
        cout << "main\n";
        exit(0);
    }
}

void checkFunctionDefinitions(Block& block, Block& global) {
    for (auto &pair : block.table) {
        Object func = pair.second;
        if (func.isFunction) {
            bool found = false;
            for (auto &otherPair : global.table) {
                Object globalFunc = otherPair.second;
                if (globalFunc.isFunction && globalFunc.isDefined && otherPair.first == pair.first) {
                    found = true;
                    for (int i=0 ; i < func.parameters.size() ; i++) {
                        if (func.parameters[0].type != globalFunc.parameters[i].type) {
                            cout << "funkcija\n";
                            exit(0);
                        }
                    }
                }
            }
            if (!found) {
                cout << "funkcija\n";
                exit(0);
            }
        }
    }
    for (auto &child : block.children) {
        checkFunctionDefinitions(*child, global);
    }
}

int main() {
    ios_base::sync_with_stdio(false);
    cout.setf(std::ios::unitbuf);

    Branch genTree;
    getline(cin, input);
    genTree.symbol = input;
    input.clear();
    loadGenTree(genTree, 0);

    Block* currentScope = scopeTree.get();

    resolveTree(genTree, currentScope, false);
    checkMain(*scopeTree);
    checkFunctionDefinitions(*scopeTree, *scopeTree);

    return 0;
}