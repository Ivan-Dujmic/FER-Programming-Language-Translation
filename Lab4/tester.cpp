#include <iostream>
#include <windows.h>
#include <fstream>
#include <string>
#include <filesystem>
#include <cstdlib>

namespace fs = std::filesystem;

int runWithTimeout(const std::string& command, const std::string& testTest, int timeoutMs) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessA(nullptr, (LPSTR)command.c_str(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        return -1;
    }

    DWORD waitResult = WaitForSingleObject(pi.hProcess, timeoutMs);
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        std::ofstream out(testTest, std::ios::trunc);
        out << "ERROR" << std::endl;
        return 1;
    }

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exitCode;
}

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
    std::string testsDir = "alltests";

    int success = 0;
    int total = 0;

    for (const auto& entry : fs::directory_iterator(testsDir)) {
        if (fs::is_directory(entry)) {
            std::string testIn = entry.path().string() + "\\test.in";
            std::string testOut = entry.path().string() + "\\test.out";
            std::string testFrisc = entry.path().string() + "\\test.frisc";
            std::string testTest = entry.path().string() + "\\test.test";

            std::string command = mainExe + " < " + testIn + " > " + testFrisc;
            system(command.c_str());

            command = "powershell.exe -Command \"node FRISC\\main.js " + testFrisc + " 2>$null | "
                        "Select-String '^-?[0-9]+$' | ForEach-Object { $_.Line } | Out-File -Encoding ASCII " + testTest + "\"";
            runWithTimeout(command, testTest, 1000);

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