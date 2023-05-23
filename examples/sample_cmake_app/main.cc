#include "hestia.h"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    if (argc == 2) {
        hestia::initialize(argv[1]);
    }
    else {
        hestia::initialize();
    }

    std::string content{"The quick brown fox jumps over the lazy dog"};
    hestia::put({0000, 0001}, false, content.data(), 0, content.length(), 0);

    hestia::move({0000, 0001}, 0, 1);

    std::vector<char> buffer(content.length());
    hestia::get({0000, 0001}, buffer.data(), 0, content.length(), 1, 1);

    std::string returned_content(buffer.begin(), buffer.end());

    if (content == returned_content) {
        std::cout << "Sanity check OK" << std::endl;
        return 0;
    }
    else {
        std::cerr << "Sanity check failed" << std::endl;
        return -1;
    }
}