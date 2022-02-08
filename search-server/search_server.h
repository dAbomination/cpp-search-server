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

#include "string_processing.h"
#include "document.h"

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};
//------------------------------------------------------------------
//------------------Начало класса SearchServer----------------------
class SearchServer {
public:
    void SetStopWords(const string& text);

    //Конструктор от аргумента string - строка со стоп словами
    explicit SearchServer(string stop_words = "");
          
    // Шаблонный конкструктор для контейнеров set и вектор   
    template<typename Container>
    explicit SearchServer(Container input_stop_words) {
        for (auto& stop_word : input_stop_words) {
            if (IsValidWord(stop_word)) {
                stop_words_.insert(stop_word);
            }
        }
    }

    // Adding new document to search server
    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings);

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        const Query query = ParseQuery(raw_query);

        vector<Document> result = FindAllDocuments(query, document_predicate);

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

    int GetDocumentCount() const;

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const;

    vector<Document> FindTopDocuments(const string& raw_query) const;

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const;

    /* Позволяет получить id_document по его порядковому номеру.
       В случае, если порядковый номер документа выходит за пределы от [0; кол - во документов),
       метод возвращает исключение*/
    int GetDocumentId(int index) const;

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector<int> document_ids_;// Идентификаторы документов в порядке добавления

    bool IsStopWord(const string& word) const;

    vector<string> SplitIntoWordsNoStop(const string& text) const;

    static int ComputeAverageRating(const vector<int>& ratings);

    QueryWord ParseQueryWord(string text) const;

    Query ParseQuery(const string& text) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const;

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const SearchServer::Query& query, DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
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

        for (const string& word : query.minus_words) {
            if (SearchServer::word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : SearchServer::word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({ document_id, relevance, SearchServer::documents_.at(document_id).rating });
        }
        return matched_documents;
    }

    // Проверка валидности отдельного слова
    static bool IsValidWord(const string& word);
};
//------------------Конец класса SearchServer-----------------------
//------------------------------------------------------------------

//---------------------------------------------------------------------
//--------------Внешние функции для работы с SearchServer--------------
void PrintDocument(const Document& document);

void PrintMatchDocumentResult(int document_id, const vector<string>& words, DocumentStatus status);

void AddDocument(SearchServer& search_server,
    int document_id,
    const string& document,
    DocumentStatus status,
    const vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, const string& raw_query);

void MatchDocuments(const SearchServer& search_server, const string& query);