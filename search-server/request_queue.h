#pragma once

#include "search_server.h"

// Очередь запросов
class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server)
        : search_server_(search_server) {
    }
    // сделаем "обёртки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
    template <typename DocumentPredicate>
    vector<Document> AddFindRequest(const string& raw_query, DocumentPredicate document_predicate) {
        // После поступления запроса, добавляем в очередь результат и время поступления
        vector<Document> result = search_server_.FindTopDocuments(raw_query, document_predicate);
        adding_result(result.size());

        return result;
    }

    vector<Document> AddFindRequest(const string& raw_query, DocumentStatus status);

    vector<Document> AddFindRequest(const string& raw_query);

    // ВОзвращает кол-во пустых звпросов за сутки
    int GetNoResultRequests() const;
private:
    struct QueryResult {
        int adding_time_;
        int results_num_;
    };
    deque<QueryResult> requests_;
    const SearchServer& search_server_;

    const static int min_in_day_ = 1440;
    int time_ = 0;
    int num_of_empty_requests_ = 0;

    // Функция добавляет в очередь результат запроса и время добавления
    // и производит необходимые операции со временем
    void adding_result(int results_num);
};