#pragma once

#include <vector>
#include <string>

// Разделение строки на отдельные слова разделённые пробелом
std::vector<std::string_view> SplitIntoWords(std::string_view str);