#pragma once

#include <vector>
#include <string>

// Разделение строки на отдельные слова разделённые пробелом
std::vector<std::string> SplitIntoWords(const std::string& text);

std::vector<std::string_view> SplitIntoWordsView(std::string_view str);