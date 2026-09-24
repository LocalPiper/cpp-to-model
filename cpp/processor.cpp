#include <iostream>
#include <string>
#include <algorithm>
#include <sstream>

std::string reverse_string(const std::string& s) {
    std::string result = s;
    std::reverse(result.begin(), result.end());
    return result;
}

std::string to_upper(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

int count_words(const std::string& s) {
    std::istringstream iss(s);
    std::string word;
    int count = 0;
    while (iss >> word) {
        count++;
    }
    return count;
}

std::string escape_json(const std::string& s) {
    std::string result;
    for (char c : s) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}

int main() {
    std::string text;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!text.empty()) text += '\n';
        text += line;
    }

    std::cout << "{" << std::endl;
    std::cout << "  \"reversed\": \"" << escape_json(reverse_string(text)) << "\"," << std::endl;
    std::cout << "  \"length\": " << text.length() << "," << std::endl;
    std::cout << "  \"uppercase\": \"" << escape_json(to_upper(text)) << "\"," << std::endl;
    std::cout << "  \"words\": " << count_words(text) << std::endl;
    std::cout << "}" << std::endl;

    return 0;
}
