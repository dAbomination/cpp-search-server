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

#include "string_processing.h"
#include "document.h"
#include "log_duration.h"

const int MAX_RESULT_DOCUMENT_COUNT = 5;
//------------------------------------------------------------------
//------------------Начало класса SearchServer----------------------
class SearchServer {
public:    
    void SetStopWords(const std::string& text);

    // Шаблонный конкструктор для контейнеров set и вектор   
    template<typename Container>
    explicit SearchServer(Container input_stop_words);
    //Конструктор от аргумента string - строка со стоп словами
    explicit SearchServer(std::string stop_words = "");

    // Adding new document to search server
    void AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);

    // Удаление документа с заданным id
    void RemoveDocument(int document_id);    
    void RemoveDocument(std::execution::sequenced_policy policy, int document_id);
    void RemoveDocument(std::execution::parallel_policy policy, int document_id);

    // Поиск MAX_RESULT_DOCUMENT_COUNT документов
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::string& raw_query) const;       
        
    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id) const;
    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(std::execution::sequenced_policy policy, const std::string& raw_query, int document_id) const;
    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(std::execution::parallel_policy policy, const std::string& raw_query, int document_id) const;


    // Получение слов и их частоты по Id документа
    const std::map<std::string, double>& GetWordFrequencies(int document_id) const;

    /* Позволяет получить id_document по его порядковому номеру.
       В случае, если порядковый номер документа выходит за пределы от [0; кол - во документов),
       метод возвращает исключение*/
    //int GetDocumentId(int index) const;

    int GetDocumentCount() const;

    std::set<int>::const_iterator begin() const;

    std::set<int>::const_iterator end() const;   

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
        std::map<std::string, double> word_freqs_; // [word, word_freq] in document
    };

    struct Query {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };

    struct QueryWord {
        std::string data;
        bool is_minus;
        bool is_stop;
    };

    std::set<std::string> stop_words_;    
    std::map<int, DocumentData> documents_; // [document_id, DocumentData]
    std::set<int> document_ids_;// Идентификаторы документов в порядке добавления    

    std::map<std::string, std::map<int, double>> word_to_document_freqs_; // [word, [document_id, word_freq]]    

    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

    QueryWord ParseQueryWord(std::string text) const;
    Query ParseQuery(const std::string& text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const SearchServer::Query& query, DocumentPredicate document_predicate) const;

    // Проверка валидности отдельного слова на:
    // отсутствие более чем одного минуса перед словами
    static bool NoWrongMinuses(const std::string& word);
    // наличие недопустимых символов (с кодами от 0 до 31)
    static bool NoSpecSymbols(const std::string& word);

    bool IsStopWord(const std::string& word) const;
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
std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentPredicate document_predicate) const {

    const Query query = ParseQuery(raw_query);

    std::vector<Document> result = FindAllDocuments(query, document_predicate);

    sort(result.begin(), result.end(),
        [](const Document& lhs, const Document& rhs) {
            if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
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
    for (const std::string& word : query.plus_words) {
        if (SearchServer::word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : SearchServer::word_to_document_freqs_.at(word)) {
            const auto& document_data = SearchServer::documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const std::string& word : query.minus_words) {
        if (SearchServer::word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : SearchServer::word_to_document_freqs_.at(word)) {
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