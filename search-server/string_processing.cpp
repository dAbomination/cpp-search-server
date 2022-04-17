#include "string_processing.h"

using namespace std;

vector<string_view> SplitIntoWords(string_view str) {
    if (str.empty()) return { basic_string_view("") };

    vector<string_view> result = {};
    const int64_t pos_end = str.npos;

    while (!str.empty()) {
        //Находим близжайший пробел
        int64_t space = str.find(' ', 0);

        result.push_back(str.substr(0, space));

        if (space == pos_end) {
            break;
        }
        str.remove_prefix(++space);
    }

    return result;
}