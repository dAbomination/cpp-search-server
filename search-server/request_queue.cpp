#include "request_queue.h"

vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) {
    vector<Document> result = search_server_.FindTopDocuments(raw_query, status);
    adding_result(result.size());

    return result;
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query) {
    vector<Document> result = search_server_.FindTopDocuments(raw_query);
    adding_result(result.size());

    return result;
}

// ¬озвращает кол-во пустых запросов за сутки
int RequestQueue::GetNoResultRequests() const {
    return num_of_empty_requests_;
}

// ‘ункци€ добавл€ет в очередь результат запроса и врем€ добавлени€
// и производит необходимые операции со временем
void RequestQueue::adding_result(int results_num) {
    // один запрос в одну минуту
    ++time_;
    // ≈сли результат поиска пустой увеличиваем значение пустых запросов
    if (results_num == 0) {
        ++num_of_empty_requests_;
    }
    requests_.push_back({ time_, results_num });

    // ”дал€ем запросы которым больше суток
    while (min_in_day_ <= (time_ - requests_.front().adding_time_)) {
        if (0 == requests_.front().results_num_) {
            --num_of_empty_requests_;
        }
        requests_.pop_front();
    }
}