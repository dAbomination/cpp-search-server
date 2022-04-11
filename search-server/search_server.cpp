#include "search_server.h"

using namespace std;

//---------------------------- Публичные методы ----------------------------

void SearchServer::SetStopWords(const string& text) {
    for (const string& stop_word : SplitIntoWords(text)) {
        if (NoSpecSymbols(stop_word)) {
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

    map<string, double> word_freqs_in_doc;
    for (const string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;        
        word_freqs_in_doc[word] += inv_word_count;
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
    RemoveDocument(execution::seq, document_id);
}

// Однопоточная версия удаления документа
void SearchServer::RemoveDocument(std::execution::sequenced_policy policy, int document_id) {
    // Зная количество слов в документе удаляем из следующих контейнеров данные об этом документе
    // word_to_document_freqs_
    for_each(
        policy,
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
void SearchServer::RemoveDocument(std::execution::parallel_policy policy, int document_id) {    

// Зная количество слов в документе удаляем из следующих контейнеров данные об этом документе
// word_to_document_freqs_       
map<string, double>& words_in_doc = documents_.at(document_id).word_freqs_;
vector<const string*> words_to_erase(words_in_doc.size());

transform(
    policy,
    words_in_doc.begin(),
    words_in_doc.end(),
    words_to_erase.begin(),
    [](const auto& word) {
        return &word.first;
    }
);

for_each(
    policy,
    words_to_erase.begin(),
    words_to_erase.end(),
    [&, document_id](const auto& word) {
        word_to_document_freqs_[*word].erase(document_id);
    }
);

// Недостаточно скорости выполнения
//vector<pair<string, double>> words_to_remove(documents_[document_id].word_freqs_.begin(), documents_[document_id].word_freqs_.end());

//for_each(
//    policy,
//    words_to_remove.begin(),
//    words_to_remove.end(),
//    [&, document_id](const pair<string, double>& word) { // first - word, second - word's freq
//        word_to_document_freqs_[word.first].erase(document_id);
//    }
//);

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
tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, int document_id) const {
    // Проверяем что данный документ существует по document_id
    if (document_ids_.count(document_id) == 0) {
        throw std::out_of_range("No document with this id");
    }

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

    return { matched_words, documents_.at(document_id).status }; // Succesfull
}

// Однопоточная версия функции
tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(execution::sequenced_policy policy, const string& raw_query, int document_id) const {
    // Проверяем что данный документ существует по document_id
    if (document_ids_.count(document_id) == 0) {
        throw std::out_of_range("No document with this id");
    }

    const Query query = ParseQuery(raw_query);
    const DocumentStatus status = documents_.at(document_id).status;

    // Кортеж состоит из vector<string> matched_words, совпадающие слова в документе с docuemnt_id и статуса данного документа

    // Проверяем вначале если если есть совпадение с минус словами и если есть то возвращаем пустой вектор,
    // так как искать совпадение по плюс словам в таком случае не надо
    if (any_of(
        policy,
        query.minus_words.begin(),
        query.minus_words.end(),
        [&, document_id](const string& minus_word) {
            auto it = word_to_document_freqs_.find(minus_word);
            return it != word_to_document_freqs_.end() && it->second.count(document_id);
        }
    )) {
        return { {}, status };
    }

    // Если совпадений по минус ловам не было, необходимо
    vector<string> matched_words(query.plus_words.size());

    auto words_end = copy_if(
        policy,
        query.plus_words.begin(),
        query.plus_words.end(),
        matched_words.begin(),
        [&, document_id](const string_view minus_word) {
            auto it = word_to_document_freqs_.find(string(minus_word));
            return it != word_to_document_freqs_.end() && it->second.count(document_id);
        }
    );

    sort(matched_words.begin(), words_end);
    words_end = unique(matched_words.begin(), words_end);
    matched_words.erase(words_end, matched_words.end());

    return { matched_words, status }; // Succesfull    
}

// Многопоточная версия функции
tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(execution::parallel_policy policy, const string& raw_query, int document_id) const {
    // Проверяем что данный документ существует по document_id
    if (document_ids_.count(document_id) == 0) {
        throw std::out_of_range("No document with this id");
    }

    const QueryView query = ParseQueryView(raw_query);
    const DocumentStatus status = documents_.at(document_id).status;

    // Кортеж состоит из vector<string> matched_words, совпадающие слова в документе с docuemnt_id и статуса данного документа
    
    // Проверяем вначале если если есть совпадение с минус словами и если есть то возвращаем пустой вектор,
    // так как искать совпадение по плюс словам в таком случае не надо
    if (any_of(
        policy,
        query.minus_words.begin(),
        query.minus_words.end(),
        [&, document_id](const string_view minus_word) {
            auto it = word_to_document_freqs_.find(string(minus_word));
            return it != word_to_document_freqs_.end() && it->second.count(document_id);
        }
    )) {
        return { {}, status };
    }
       
    // Если совпадений по минус ловам не было, необходимо
    vector<string> matched_words(query.plus_words.size());

    auto words_end = copy_if(
        policy,
        query.plus_words.begin(),
        query.plus_words.end(),
        matched_words.begin(),
        [&, document_id](const string_view plus_word) {
            auto it = word_to_document_freqs_.find(string(plus_word));
            return it != word_to_document_freqs_.end() && it->second.count(document_id);
        }
    );
    
    sort(matched_words.begin(), words_end);
    words_end = unique(matched_words.begin(), words_end);
    matched_words.erase(words_end, matched_words.end());

    return { matched_words, status }; // Succesfull
}

//---------------------------- Приватные методы ----------------------------

bool SearchServer::IsStopWord(const string& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsStopWordView(string_view word) const {
    return stop_words_.count(string(word)) > 0;
}

vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
    vector<string> result;
    for (const string& word : SplitIntoWords(text)) {
        if (NoSpecSymbols(word)) {
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

// Разбивает входной текст на плюс слова и минус слова(перед которыми есть знак "-")
SearchServer::Query SearchServer::ParseQuery(const string& text) const {
    Query query;
    for (const string& word : SplitIntoWords(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (NoSpecSymbols(word) && NoWrongMinuses(word)) {// Проверка все ли слова подходящие
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

// Определяет является ли слово стоп словом(со знаком "-") или находится в вписке стоп слов
SearchServer::QueryWordView SearchServer::ParseQueryWordView(string_view text) const {
    bool is_minus = false;
    // Word shouldn't be empty
    if (text[0] == '-') {
        is_minus = true;
        text.remove_prefix(1);
    }
    return {
        text,
        is_minus,
        IsStopWordView(text)
    };
}

// Разбивает входной текст на плюс слова и минус слова(перед которыми есть знак "-")
SearchServer::QueryView SearchServer::ParseQueryView(string_view text) const {
    QueryView query;
    
    for (string_view word : SplitIntoWordsView(text)) {
        QueryWordView query_word = ParseQueryWordView(word);

        if (!query_word.is_stop && !query_word.is_minus) {
            query.plus_words.push_back(word);
        }
        else {
            if (query_word.is_minus) {
                query.minus_words.push_back(query_word.data);
            }
        }
    }

    return query;
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

// Проверка отдельного слова на наличие спецсимволов
bool SearchServer::NoSpecSymbols(const string& word) {    
    if (!none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        })) {
        throw invalid_argument("contains invalid characters");
    }    
    return true;
}

// Проверка отдельного слова на :
// - то что слово не является "одиноким" минусом
// - то что перед словом не идут подряд несколько минусов
bool SearchServer::NoWrongMinuses(const string& word) {
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