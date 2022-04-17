#include "search_server.h"

using namespace std;

//---------------------------- Публичные методы ----------------------------

void SearchServer::SetStopWords(const string_view text) {
    for (const string_view stop_word : SplitIntoWords(text)) {
        if (NoSpecSymbols(stop_word)) {
            stop_words_.insert(string(stop_word));
        }
    }
}

//Конструктор от аргумента string - строка со стоп словами
SearchServer::SearchServer(string stop_words) 
    : SearchServer(string_view(stop_words)) {    
}

//Конструктор от аргумента string_view - строка со стоп словами
SearchServer::SearchServer(string_view stop_words) {
    SetStopWords(stop_words);
}
 
// Adding new document to search server
void SearchServer::AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
    // Check if document with document_id already exist or document_id < 0
    if ((documents_.count(document_id)) || (document_id < 0)) {
        throw invalid_argument("document_id already exist or below zero"); // error: this document_id already exist or below zero
    }
    vector<string_view> words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();

    map<string, double> word_freqs_in_doc;
    for (const string_view word : words) {
        //Проверяем слово на наличие спецсимволов
        if (NoSpecSymbols(word)) {
            word_to_document_freqs_[string(word)][document_id] += inv_word_count;
            word_freqs_in_doc[string(word)] += inv_word_count;
        }
    }

    documents_.emplace(document_id,
        DocumentData{
            ComputeAverageRating(ratings),
            status,
            word_freqs_in_doc
        });

    document_ids_.insert(document_id);
}

// Однопоточная версия удаления документа
void SearchServer::RemoveDocument(int document_id) {
    RemoveDocument(std::execution::seq, document_id);
}

// Однопоточная версия удаления документа
void SearchServer::RemoveDocument(std::execution::sequenced_policy, int document_id) {
    // Зная количество слов в документе удаляем из следующих контейнеров данные об этом документе
    // word_to_document_freqs_
    for_each(
        execution::seq,
        documents_[document_id].word_freqs_.begin(),
        documents_[document_id].word_freqs_.end(),
        [this, document_id](const pair<string, double>& word) { // first - word, second - word's freq
            word_to_document_freqs_[word.first].erase(document_id);
        }
    );

    // document_ids_
    document_ids_.erase(document_id);

    // documents_
    documents_.erase(document_id);
}

// Многопоточная версия удаления документа
void SearchServer::RemoveDocument(std::execution::parallel_policy, int document_id) {    

    // Зная количество слов в документе удаляем из следующих контейнеров данные об этом документе
    // word_to_document_freqs_       
    map<string, double>& words_in_doc = documents_.at(document_id).word_freqs_;
    vector<const string*> words_to_erase(words_in_doc.size());

    transform(
        execution::par,
        words_in_doc.begin(),
        words_in_doc.end(),
        words_to_erase.begin(),
        [](const auto& word) {
            return &word.first;
        }
    );

    for_each(
        execution::par,
        words_to_erase.begin(),
        words_to_erase.end(),
        [&, document_id](const auto& word) {
            word_to_document_freqs_[*word].erase(document_id);
        }
    );

    // document_ids_
    document_ids_.erase(document_id);

    // documents_
    documents_.erase(document_id);
}

// Find documents with certain status
vector<Document> SearchServer::FindTopDocuments(const string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(
        raw_query,
        [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        }
    );
}

// Find documents with status = ACTUAL
vector<Document> SearchServer::FindTopDocuments(const string_view raw_query) const {
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

    auto find_freq_result = documents_.find(document_id);
    if (find_freq_result != documents_.end()) {
        return find_freq_result->second.word_freqs_;
    }

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

// Однопоточная версия функции
MatchedWords SearchServer::MatchDocument(const string& raw_query, int document_id) const {
    // Проверяем что данный документ существует по document_id
    return MatchDocument(execution::seq, raw_query, document_id);
}

// Однопоточная версия функции
MatchedWords SearchServer::MatchDocument(execution::sequenced_policy, const string_view raw_query, int document_id) const {
    
    const Query query = ParseQuery(raw_query);
    const DocumentStatus status = documents_.at(document_id).status;
    // Кортеж состоит из vector<string> matched_words, совпадающие слова в документе с docuemnt_id
    // и статуса данного документа
    vector<string> matched_words;
    for (const string_view& word : query.plus_words) {
        if (word_to_document_freqs_.count(string(word)) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(string(word)).count(document_id)) {
            matched_words.push_back(string(word));
        }
    }
    for (const string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(string(word)) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(string(word)).count(document_id)) {
            matched_words.clear();
            break;
        }
    }

    if (matched_words.empty()) {
        return { {}, status };
    }

    vector<string_view> matched_words_view;

    for (auto& word : matched_words) {
        auto it = word_to_document_freqs_.find(string(word));
        matched_words_view.push_back(it->first);
    }

    return { matched_words_view, status }; // Succesfull     
}

// Многопоточная версия функции
MatchedWords SearchServer::MatchDocument(execution::parallel_policy, const string_view raw_query, int document_id) const {
    // Проверяем что данный документ существует по document_id
    if (document_ids_.count(document_id) == 0) {
        throw std::out_of_range("no document with this id");
    }

    const Query query = ParseQuery(raw_query);
    const DocumentStatus status = documents_.at(document_id).status;

    // Кортеж состоит из vector<string> matched_words, совпадающие слова в документе с docuemnt_id и статуса данного документа
    
    // Проверяем вначале если если есть совпадение с минус словами и если есть то возвращаем пустой вектор,
    // так как искать совпадение по плюс словам в таком случае не надо
    if (any_of(
        execution::par,
        query.minus_words.begin(),
        query.minus_words.end(),
        [&, document_id](const string_view minus_word) {
            auto it = word_to_document_freqs_.find(string(minus_word));
            return it != word_to_document_freqs_.end() && it->second.count(document_id);
        }
    )) {
        return { {}, status };
    }
       
    // Если совпадений по минус ловам не было, необходимо пройти по плюс словам и добавить совпадающие значения
    vector<string_view> matched_words(query.plus_words.size());

    auto words_end = copy_if(
        execution::par,
        query.plus_words.begin(),
        query.plus_words.end(),
        matched_words.begin(),
        [&, document_id](const string_view plus_word) {
            auto it = word_to_document_freqs_.find(string(plus_word));
            return it != word_to_document_freqs_.end() && it->second.count(document_id);
        }
    );
    
    sort(execution::par, matched_words.begin(), words_end);
    words_end = unique(matched_words.begin(), words_end);
    matched_words.erase(words_end, matched_words.end());

    vector<string_view> matched_words_view;

    for (auto& word : matched_words) {
        auto it = word_to_document_freqs_.find(string(word));
        matched_words_view.push_back(it->first);
    }

    return { matched_words_view, status }; // Succesfull   
}

//---------------------------- Приватные методы ----------------------------

bool SearchServer::IsStopWord(const string_view word) const {
    return stop_words_.count(string(word)) > 0;
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(const string_view text) const {
    vector<string_view> result;
    for (const string_view word : SplitIntoWords(text)) {        
        if (!IsStopWord(word)) {
            result.push_back(word);
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

// Определяет является ли слово стоп словом(со знаком "-") или находится в вписке стоп слов
SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
    bool is_minus = false;
    // Word shouldn't be empty
    if (text[0] == '-') {
        is_minus = true;
        text.remove_prefix(1);
    }
    return {
        text,
        is_minus,
        IsStopWord(text)
    };
}

// Разбивает входной текст на плюс слова и минус слова(перед которыми есть знак "-")
SearchServer::Query SearchServer::ParseQuery(string_view text) const {
    Query query;    

    for (string_view word : SplitIntoWords(text)) {
        QueryWord query_word = ParseQueryWord(word);
        // Проверяем слово на валидность
        if (NoSpecSymbols(word) && NoWrongMinuses(word)) {
            if (!query_word.is_stop && !query_word.is_minus) {
                query.plus_words.push_back(word);
            }
            else {
                if (query_word.is_minus) {
                    query.minus_words.push_back(query_word.data);
                }
            }
        }
    }

    sort(query.plus_words.begin(), query.plus_words.end());
    auto plus_words_end = unique(query.plus_words.begin(), query.plus_words.end());
    query.plus_words.resize(distance(query.plus_words.begin(), plus_words_end));

    return query;
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

// Проверка отдельного слова на наличие спецсимволов
bool SearchServer::NoSpecSymbols(const string_view word) {    
    if (!none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        })) {
        throw std::invalid_argument("contains invalid characters");
    }    
    return true;
}

// Проверка отдельного слова на :
// - то что слово не является "одиноким" минусом
// - то что перед словом не идут подряд несколько минусов
bool SearchServer::NoWrongMinuses(const string_view word) {
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

void PrintMatchDocumentResult(int document_id, const vector<string_view>& words, DocumentStatus status) {
    cout << "{ "s
        << "document_id = "s << document_id << ", "s
        << "status = "s << static_cast<int>(status) << ", "s
        << "words ="s;
    for (const string_view word : words) {
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