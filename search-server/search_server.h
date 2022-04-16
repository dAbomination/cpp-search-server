#pragma once

#include <iostream>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <string>
#include <deque>
#include <execution>
#include <string_view>
#include <limits>

#include "string_processing.h"
#include "document.h"
#include "log_duration.h"
#include "concurrent_map.h"

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double RELEVANCE_PRECISION = 1e-6;
//------------------------------------------------------------------
//------------------Начало класса SearchServer----------------------
class SearchServer {
public:    
    void SetStopWords(const std::string_view text);

    // Шаблонный конкструктор для контейнеров set и вектор   
    template<typename Container>
    explicit SearchServer(Container input_stop_words);
    //Конструктор от аргумента string - строка со стоп словами
    explicit SearchServer(std::string stop_words = "");
    explicit SearchServer(std::string_view stop_words);

    // Adding new document to search server
    void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);

    // Удаление документа с заданным id
    void RemoveDocument(int document_id);    
    void RemoveDocument(std::execution::sequenced_policy, int document_id);
    void RemoveDocument(std::execution::parallel_policy, int document_id);

    // Поиск MAX_RESULT_DOCUMENT_COUNT документов
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const;       
        
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy, const std::string_view raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy, const std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(std::execution::sequenced_policy, const std::string_view raw_query) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy, const std::string_view raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy, const std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(std::execution::parallel_policy, const std::string_view raw_query) const;      

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::sequenced_policy, const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::parallel_policy, const std::string_view raw_query, int document_id) const;


    // Получение слов и их частоты по Id документа
    const std::map<std::string, double>& GetWordFrequencies(int document_id) const;

    int GetDocumentCount() const;

    std::set<int>::const_iterator begin() const;

    std::set<int>::const_iterator end() const;   

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
        std::map<std::string, double> word_freqs_; // [word, word_freq] in document
    };

    // Структура для хранения слов запроса
    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    std::set<std::string> stop_words_;    
    std::map<int, DocumentData> documents_; // [document_id, DocumentData]
    std::set<int> document_ids_;// Идентификаторы документов в порядке добавления    

    std::map<std::string, std::map<int, double>> word_to_document_freqs_; // [word, [document_id, word_freq]]    

    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view text) const;

    // Разбивает входной текст на плюс слова и минус слова(перед которыми есть знак "-") 
    Query ParseQuery(std::string_view text) const;
    QueryWord ParseQueryWord(std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const SearchServer::Query& query, DocumentPredicate document_predicate) const;
    
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(std::execution::parallel_policy, const SearchServer::Query& query, DocumentPredicate document_predicate) const;

    // Проверка валидности отдельного слова на:
    // отсутствие более чем одного минуса перед словами
    static bool NoWrongMinuses(const std::string_view word);
    // наличие недопустимых символов (с кодами от 0 до 31)
    static bool NoSpecSymbols(const std::string_view word);

    bool IsStopWord(const std::string_view word) const;
    bool IsStopWordView(std::string_view word) const;

    //bool IsValidWord(std::string_view word) const;
};
//------------------Конец класса SearchServer-----------------------
//------------------------------------------------------------------

// Шаблонный конструктор для контейнеров set и vector   
template<typename Container>
SearchServer::SearchServer(Container input_stop_words) {
    for (auto& stop_word : input_stop_words) {
        if (NoSpecSymbols(stop_word)) {
            stop_words_.insert(stop_word);
        }
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const {
    Query query = ParseQuery(raw_query);
        
    std::vector<Document> result = FindAllDocuments(query, document_predicate);
        
    sort(result.begin(), result.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::fabs(lhs.relevance - rhs.relevance) < RELEVANCE_PRECISION) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        });
    if (result.size() > MAX_RESULT_DOCUMENT_COUNT) {
        result.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return result;// Successful search
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::execution::parallel_policy, const std::string_view raw_query, DocumentPredicate document_predicate) const {
    Query query = ParseQuery(raw_query);

    std::vector<Document> result = FindAllDocuments(std::execution::par, query, document_predicate);

    sort(result.begin(), result.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::fabs(lhs.relevance - rhs.relevance) < RELEVANCE_PRECISION) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        });
    if (result.size() > MAX_RESULT_DOCUMENT_COUNT) {
        result.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return result;// Successful search
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::execution::sequenced_policy, const std::string_view raw_query, DocumentPredicate document_predicate) const {
    Query query = ParseQuery(raw_query);
    
    std::vector<Document> result = FindAllDocuments(query, document_predicate);
    
    sort(result.begin(), result.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::fabs(lhs.relevance - rhs.relevance) < RELEVANCE_PRECISION) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        });
    if (result.size() > MAX_RESULT_DOCUMENT_COUNT) {
        result.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return result;// Successful search
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const SearchServer::Query& query, DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    
    for (const std::string_view word : query.plus_words) {
        if (SearchServer::word_to_document_freqs_.count(std::string(word)) == 0) {
            continue;
        }

        const double inverse_document_freq = ComputeWordInverseDocumentFreq(std::string(word));
        for (const auto& [document_id, term_freq] : SearchServer::word_to_document_freqs_.at(std::string(word))) {
            const auto& document_data = SearchServer::documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }
    
    for (const std::string_view word : query.minus_words) {
        if (SearchServer::word_to_document_freqs_.count(std::string(word)) == 0) {
            continue;
        }
        for (const auto [document_id, _] : SearchServer::word_to_document_freqs_.at(std::string(word))) {
            document_to_relevance.erase(document_id);
        }
    }
    
    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({ document_id, relevance, SearchServer::documents_.at(document_id).rating });
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(std::execution::parallel_policy, const SearchServer::Query& query, DocumentPredicate document_predicate) const {
    const int BUCKET_COUNT = 10;       
    
    ConcurrentMap<int, double> document_to_relevance_concurrent(BUCKET_COUNT);

    // Для каждого из плюс слов 
    for_each(std::execution::par,
        query.plus_words.begin(),
        query.plus_words.end(),
        [this, &document_to_relevance_concurrent, &document_predicate](std::string_view word) {
            if (SearchServer::word_to_document_freqs_.count(std::string(word)) == 0) {
                return;
            }

            const double inverse_document_freq = ComputeWordInverseDocumentFreq(std::string(word));

            for (const auto& [document_id, term_freq] : SearchServer::word_to_document_freqs_.at(std::string(word))) {
                const auto& document_data = SearchServer::documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {                    
                    document_to_relevance_concurrent[document_id].ref_to_value += term_freq * inverse_document_freq;
                }
            }
        }
    );

    std::map<int, double> document_to_relevance = move(document_to_relevance_concurrent.BuildOrdinaryMap());

    for (const std::string_view word : query.minus_words) {
        if (SearchServer::word_to_document_freqs_.count(std::string(word)) == 0) {
            continue;
        }
        for (const auto [document_id, _] : SearchServer::word_to_document_freqs_.at(std::string(word))) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({ document_id, relevance, SearchServer::documents_.at(document_id).rating });
    }

    return matched_documents;
}

//---------------------------------------------------------------------
//--------------Внешние функции для работы с SearchServer--------------
void PrintDocument(const Document& document);

void PrintMatchDocumentResult(int document_id, const std::vector<std::string>& words, DocumentStatus status);

void AddDocument(SearchServer& search_server,
    int document_id,
    const std::string& document,
    DocumentStatus status,
    const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query);

void MatchDocuments(const SearchServer& search_server, const std::string& query);