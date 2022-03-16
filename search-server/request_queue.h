#pragma once

#include "search_server.h"

// ������� ��������
class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server)
        : search_server_(search_server) {
    }
    // "������" ��� ���� ������� ������, ����� ��������� ���������� ��� ����������
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);

    // ���������� ���-�� ������ �������� �� �����
    int GetNoResultRequests() const;
private:
    struct QueryResult {
        int adding_time_;
        int results_num_;
    };
    std::deque<QueryResult> requests_;
    const SearchServer& search_server_;

    const static int min_in_day_ = 1440;
    int time_ = 0;
    int num_of_empty_requests_ = 0;

    // ������� ��������� � ������� ��������� ������� � ����� ����������
    // � ���������� ����������� �������� �� ��������
    void AddinResult(int results_num);
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    // ����� ����������� �������, ��������� � ������� ��������� � ����� �����������
    std::vector<Document> result = search_server_.FindTopDocuments(raw_query, document_predicate);
    AddinResult(result.size());

    return result;
}