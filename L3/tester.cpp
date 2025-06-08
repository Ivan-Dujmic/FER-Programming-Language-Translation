#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

bool compareFiles(const std::string& file1, const std::string& file2) {
    std::ifstream f1(file1, std::ifstream::binary | std::ifstream::ate);
    std::ifstream f2(file2, std::ifstream::binary | std::ifstream::ate);

    if (f1.fail() || f2.fail()) {
        return false; // file problem
    }

    // seek back to beginning and use std::equal to compare contents
    f1.seekg(0, std::ifstream::beg);
    f2.seekg(0, std::ifstream::beg);
    return std::equal(std::istreambuf_iterator<char>(f1.rdbuf()),
                      std::istreambuf_iterator<char>(),
                      std::istreambuf_iterator<char>(f2.rdbuf()));
}

int main() {
    system("g++ main.cpp -o main.exe");

    std::string mainExe = "main.exe";
    std::string testsDir = "moretests";

    int success = 0;
    int total = 0;

    for (const auto& entry : fs::directory_iterator(testsDir)) {
        if (fs::is_directory(entry)) {
            std::string testIn = entry.path().string() + "\\test.in";
            std::string testOut = entry.path().string() + "\\test.out";
            std::string testTest = entry.path().string() + "\\test.test";

            std::string command = mainExe + " < " + testIn + " > " + testTest;
            system(command.c_str());

            command = "fc " + testOut + " " + testTest + " > nul";

            total++;

            if (system(command.c_str()) == 0) { // 0 means no difference
                success++;
                std::cout << "\033[32m1\033[0m"; // Green for success
            } else {
                std::cout << "\033[31m0\033[0m"; // Red for failure
            }
            std::cout << " " << entry.path().filename() << std::endl;
        }
    }

    std::cout << "Success: " << success << "/" << total << std::endl;

    return 0;
}