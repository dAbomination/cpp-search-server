#include "search_server.h"

using namespace std;

void SearchServer::SetStopWords(const string& text) {
    for (const string& stop_word : SplitIntoWords(text)) {
        if (IsValidWord(stop_word)) {
            stop_words_.insert(stop_word);
        }
    }
}

//Конструктор от аргумента string - строка со стоп словами
SearchServer::SearchServer(string stop_words) {
    SetStopWords(stop_words);
}

// Adding new document to search server
void SearchServer::AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
    // Check if document with document_id already exist or document_id < 0
    if ((documents_.count(document_id)) || (document_id < 0)) {
        throw invalid_argument("document_id already exist or below zero"); // error: this document_id already exist or below zero
    }
    vector<string> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][word] += inv_word_count;
    }

    documents_.emplace(document_id,
        DocumentData{
            ComputeAverageRating(ratings),
            status
        });

    document_ids_.insert(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    // Зная количество слов в документе удаляем из следующих контейнеров данные об этом документе
    // word_to_document_freqs_
    for (auto [word, freq] : document_to_word_freqs_[document_id]) {        
        word_to_document_freqs_[word].erase(document_id);
    }
    // document_to_word_freqs_
    document_to_word_freqs_.erase(document_id);

    // document_ids_
    document_ids_.erase(document_id);

    // documents_
    documents_.erase(document_id);
}

// Find documents with certain status
vector<Document> SearchServer::FindTopDocuments(const string& raw_query, DocumentStatus status) const {
    return FindTopDocuments(
        raw_query,
        [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        }
    );
}

// Find documents with status = ACTUAL
vector<Document> SearchServer::FindTopDocuments(const string& raw_query) const {
    return FindTopDocuments(
        raw_query,
        [](int document_id, DocumentStatus status, int rating) {
            return status == DocumentStatus::ACTUAL;
        }
    );
}

// Получение слов и их частоты по Id документа
const map<string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static map<string, double> result;

    auto find_result = document_to_word_freqs_.find(document_id);
    if (find_result != document_to_word_freqs_.end()) result = find_result->second;

    return result;
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

std::set<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, int document_id) const {
    const Query query = ParseQuery(raw_query);
    // Кортеж состоит из vector<string> matched_words, совпадающие слова в документе с docuemnt_id
    // и статуса данного документа
    vector<string> matched_words;
    for (const string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }

    return tuple{ matched_words, documents_.at(document_id).status };// Succesfull
}

/* Позволяет получить id_document по его порядковому номеру.
   В случае, если порядковый номер документа выходит за пределы от [0; кол - во документов),
   метод возвращает исключение*/
/*int SearchServer::GetDocumentId(int index) const {
    return document_ids_.at(index);
}*/

// Приватные методы

bool SearchServer::IsStopWord(const string& word) const {
    return stop_words_.count(word) > 0;
}

vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
    vector<string> result;
    for (const string& word : SplitIntoWords(text)) {
        if (IsValidWord(word)) {
            if (!IsStopWord(word)) {
                result.push_back(word);
            }
        }
    }
    return result;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string text) const {
    bool is_minus = false;
    // Word shouldn't be empty
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    return {
        text,
        is_minus,
        IsStopWord(text)
    };
}

SearchServer::Query SearchServer::ParseQuery(const string& text) const {
    Query query;
    for (const string& word : SplitIntoWords(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (IsValidWord(word)) {// Проверка все ли слова подходящие
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
    }
    return query;
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

// Проверка валидности отдельного слова
bool SearchServer::IsValidWord(const string& word) {
    // К слову предъявляются следующие условия:
    // 1. не является "одиноким" минусом;
    // 2. не является спецсимолом;
    // 3. вначале слова нет больше 1 минуса (2 и больше);
    if (!none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        })) {
        throw invalid_argument("contains invalid characters");
    }
    if (word == "-") throw invalid_argument("lonely minus");
    if ((word.size() >= 2) && (word[0] == '-') && (word[1] == '-')) throw invalid_argument("too many minuses");
    return true;
}

//---------------------------------------------------------------------
//--------------Внешние функции для работы с SearchServer--------------
void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << endl;
}

void PrintMatchDocumentResult(int document_id, const vector<string>& words, DocumentStatus status) {
    cout << "{ "s
        << "document_id = "s << document_id << ", "s
        << "status = "s << static_cast<int>(status) << ", "s
        << "words ="s;
    for (const string& word : words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status,
    const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    }
    catch (const exception& e) {
        cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    LOG_DURATION_STREAM("Operation time", cout);
    cout << "Результаты поиска по запросу: "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    }
    catch (const exception& e) {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
    LOG_DURATION_STREAM("Operation time", cout);
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;        
        
        for (const int document_id : search_server) {
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }

    }
    catch (const exception& e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}