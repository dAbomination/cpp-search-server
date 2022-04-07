#pragma once

#include <execution>

#include "search_server.h"

// Функция распараллеливает обработку нескольких запросов к поисковой системе
// возвращает vector<Document> для каждого из входных запросов (результат FindTopDocuments)
std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries);

// Аналогична функции ProcessQueries, но возвращает набор документов в плоском виде
std::vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries);