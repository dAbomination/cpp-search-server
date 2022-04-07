#pragma once

#include <execution>

#include "search_server.h"

// ������� ���������������� ��������� ���������� �������� � ��������� �������
// ���������� vector<Document> ��� ������� �� ������� �������� (��������� FindTopDocuments)
std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries);

// ���������� ������� ProcessQueries, �� ���������� ����� ���������� � ������� ����
std::vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries);