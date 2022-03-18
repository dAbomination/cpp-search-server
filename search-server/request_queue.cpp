#include "request_queue.h"

using namespace std;

vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) {    
    return AddFindRequest(
        raw_query,
        [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        }
    );    
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query) {
    return AddFindRequest(raw_query, DocumentStatus::ACTUAL);    
}

// ���������� ���-�� ������ �������� �� �����
int RequestQueue::GetNoResultRequests() const {
    return num_of_empty_requests_;
}

// ������� ��������� � ������� ��������� ������� � ����� ����������
// � ���������� ����������� �������� �� ��������
void RequestQueue::AddinResult(int results_num) {
    // ���� ������ � ���� ������
    ++time_;
    // ���� ��������� ������ ������ ����������� �������� ������ ��������
    if (results_num == 0) {
        ++num_of_empty_requests_;
    }
    requests_.push_back({ time_, results_num });

    // ������� ������� ������� ������ �����
    while (min_in_day_ <= (time_ - requests_.front().adding_time_)) {
        if (0 == requests_.front().results_num_) {
            --num_of_empty_requests_;
        }
        requests_.pop_front();
    }
}
