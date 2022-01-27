#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <string>
#include <iostream>
#include <optional>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

// Разделение строки на отдельные слова разделённые пробелом
vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    Document() = default;

    Document(int id, double relevance, int rating)
        : id(id), relevance(relevance), rating(rating) { };

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    // Defines an invalid document id    
    inline static constexpr int INVALID_DOCUMENT_ID = -1;

    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            if (IsValidWord(word)) {
                stop_words_.insert(word);
            }                
        }
    }

    //Конструктор от аргумента string - строка со стоп словами
    explicit SearchServer(string stop_words = "") {
        SetStopWords(stop_words);
    }

    // Шаблонный конкструктор для контейнеров set и вектор   
    template<typename Container>
    explicit SearchServer(Container input_stop_words) {
        for (auto& stop_word : input_stop_words) {
            SetStopWords(stop_word);
        }
    }

    // Adding new document to search server
    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        // Check if document with document_id already exist or document_id < 0
        if ( (documents_.count(document_id)) || (document_id < 0) ) {
            throw invalid_argument("document_id already exist or below zero"); // error: this document_id already exist or below zero
        }
        vector<string> words = SplitIntoWordsNoStop(document);        
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id,
            DocumentData{
                ComputeAverageRating(ratings),
                status
            });
        document_ids_.push_back(document_id);        
    }

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

    int GetDocumentCount() const {
        return documents_.size();
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(
            raw_query,
            [status](int document_id, DocumentStatus document_status, int rating) {
                return document_status == status;
            }
        );
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(
            raw_query,
            [](int document_id, DocumentStatus status, int rating) {
                return status == DocumentStatus::ACTUAL;
            }
        );
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const { 
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
       метод должен вернуть значение SearchServer::INVALID_DOCUMENT_ID*/
    int GetDocumentId(int index) const {        
        return document_ids_.at(index);
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector<int> document_ids_;// Идентификаторы документов в порядке добавления

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
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

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
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

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {      
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
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }

    // Проверка валидности отдельного слова
    static bool IsValidWord(const string& word) {
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
};
//------------------Конец класса SearchServer-----------------------
//------------------------------------------------------------------

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
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = search_server.GetDocumentId(index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }
    catch (const exception& e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}

//-----------------------------------------
// Макросы для тестирования
//-----------------------------------------
template <typename Key, typename Value>
ostream& operator<<(ostream& out, const pair<Key, Value>& container) {

    out << container.first << ": " << container.second;

    return out;
}

template <typename Term>
void Print(ostream& out, const Term& container) {
    bool first = true;

    for (const auto& element : container) {
        if (!first) {
            out << ", "s;
        }
        first = false;
        out << element;
    }
}

template <typename Term>
ostream& operator<<(ostream& out, const vector<Term>& container) {
    out << '[';
    Print(out, container);
    out << ']';
    return out;
}

template <typename Term>
ostream& operator<<(ostream& out, const set<Term>& container) {
    out << '{';
    Print(out, container);
    out << '}';
    return out;
}

template <typename Key, typename Value>
ostream& operator<<(ostream& out, const map<Key, Value>& container) {
    out << '{';
    Print(out, container);
    out << '}';
    return out;
}
//-----------------------------------------

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
    const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
    const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <class TestFunc>
void RunTestImpl(TestFunc func, const string& func_name) {
    func();
    cerr << func_name << " OK" << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

//--------------------------------------------------------------
//--------- Начало модульных тестов поисковой системы ----------
// Тест функции SplitIntoWords при множественных пробелах
void TestSplitIntoWordsWithDifferentErrors() {    
    const vector<string> expected_result = { "first"s, "second"s, "third"s };
    vector<string> result = SplitIntoWords("   first       second            third    "s);
    ASSERT_EQUAL(result, expected_result);
}

// Тест проверяет: работоспособность конструкторов с различными параметрами,
// что поисковая система исключает стоп-слова при добавлении документов,
// а также выдаёт ошибки при попытке добавить документ с совпадающим id или если он меньше нуля
void TestAddingDocumentsStopWordsExcludingStopWords() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 0 };

    // Попытаемся добавить документ с отрицательным id или с дублирующимся id
    {
        SearchServer server;        
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);              
        // Повторно пытаемся добавить документ и должны отловить исключение
        try {
            server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        }
        catch (const exception& e) {
            ASSERT_EQUAL(e.what(), "document_id already exist or below zero"s);
        }
        // Пытаемся добавить документ с отрицательным id
        try {
            server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        }
        catch (const exception& e) {
            ASSERT_EQUAL(e.what(), "document_id already exist or below zero"s);
        }
    }
    // Убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        vector<Document> result = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(result.size(), 1u);        
        ASSERT_EQUAL(result[0].id, doc_id);        
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        vector<Document> result = server.FindTopDocuments("in"s);
        ASSERT(result.empty());
    }
}

// Тест проверяет поддержку минус слов, документы содержащие минус слова в поиске не включаются в результат
void TestExludeDocumentsWithMinusWordsFromResults() {
    const vector<int> ratings = { 0 };
    //Проверяем что в случае поиска документа с минус словом он не попадёт в результаты
    {
        SearchServer server;
        

        server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(14, "dog outside the city"s, DocumentStatus::ACTUAL, ratings);
        vector<Document> result = server.FindTopDocuments("city -cat"s);
        //убеждаемся что в результатах есть только один документ в котором нет минус слова
        ASSERT_EQUAL(result.size(), 1u);
    }

    //Проверяем если в запросе будут только стоп слова то результат будет пустой
    {
        SearchServer server;        

        server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(14, "dog outside the city"s, DocumentStatus::ACTUAL, ratings);
        vector<Document> result = server.FindTopDocuments("-cat"s);
        ASSERT(result.empty());
    }
}

// Тест проверяет Матчинг документов.
// При матчинге документа по поисковому запросу должны быть возвращены все слова из поискового запроса,
// присутствующие в документе.Если есть соответствие хотя бы по одному минус - слову,
// должен возвращаться пустой список слов.
void TestMatchDocuments() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    string query = "cat dog outside the";
    //Проверка того что функция MatchDocument находит совпадающие слова
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        vector<string> result_matched_words;
        DocumentStatus status_matched_document_status;
        //Распаковываем кортеж, который изменяется по ссылке в результате выполнения функции MatchDocument
        
        tie(result_matched_words, status_matched_document_status) = server.MatchDocument(query, doc_id);
                
        //Поисковый запрос должен вернуть результат cat и the так как они присутствуют в указанном документе        
        vector<string> expected_result = { "cat", "the" };
        ASSERT_EQUAL((result_matched_words), expected_result);
    }
    //Проверка исключения документа если там присутствует минус слово
    query = "cat dog -city outside the";
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        vector<string> result_matched_words;
        DocumentStatus status_matched_document_status;
        //Распаковываем кортеж, который изменяется по ссылке в результате выполнения функции MatchDocument
        
        tie(result_matched_words, status_matched_document_status) = server.MatchDocument(query, doc_id);

        //Поисковый запрос должен вернуть пустую строку так как в запросе присутвует минус слово city
        ASSERT(result_matched_words.empty());
    }
    //Если совпадающих слов нет должен возвращаться пустой вектор
    query = "dog inside box";
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        vector<string> result_matched_words;
        DocumentStatus status_matched_document_status;
        //Распаковываем кортеж, который изменяется по ссылке в результате выполнения функции MatchDocument        
        tie(result_matched_words, status_matched_document_status) = server.MatchDocument(query, doc_id);

        //Поисковый запрос должен вернуть пустую строку так как в запросе присутвует минус слово city        
        ASSERT(result_matched_words.empty());
    }
}

// Сортировка найденных документов по релевантности.Возвращаемые при поиске документов результаты должны быть
// отсортированы в порядке убывания релевантности.
void TestSortingByRelevance() {
    string query = "two four";
    {
        SearchServer server;        
        // Добавим 4 документа с разной релевантностью в разнобой
        server.AddDocument(41, "one two three four five"s, DocumentStatus::ACTUAL, { 0 });
        server.AddDocument(34, "two four"s, DocumentStatus::ACTUAL, { 0 });
        server.AddDocument(62, "one two four five"s, DocumentStatus::ACTUAL, { 0 });
        server.AddDocument(7, "three four five"s, DocumentStatus::ACTUAL, { 0 });        

        vector<Document> result = server.FindTopDocuments(query);
        // Проверим что каждующая последующая релевантность меньше предыдущей         
        for (int i = 1; i < (result.size()); ++i) {
            double previous_relevance = result[i - 1].relevance;
            ASSERT((result[i].relevance < previous_relevance));
        }
    }

    query = "-two one six";
    // Добавим минус слово в запрос и убедимся что останется два документа
    {
        SearchServer server;
        
        // Добавим 4 документа с разной релевантностью в разнобой
        server.AddDocument(41, "one two three four five"s, DocumentStatus::ACTUAL, { 0 });
        server.AddDocument(34, "one four"s, DocumentStatus::ACTUAL, { 0 });
        server.AddDocument(62, "one two four five"s, DocumentStatus::ACTUAL, { 0 });
        server.AddDocument(7, "three four five six"s, DocumentStatus::ACTUAL, { 0 });
        // Из-за стоп слова в запросе должны уйти из результата два документа        
        vector<Document> result = server.FindTopDocuments(query);
        
        ASSERT_EQUAL(result.size(), 2u);
        // Проверим что каждующая последующая релевантность меньше предыдущей         
        for (int i = 1; i < (result.size()); ++i) {
            double previous_relevance = result[i - 1].relevance;
            ASSERT((result[i].relevance < previous_relevance));
        }
    }
}
//Функция для расчёта среднего арифметического для вектора целых чисел int
int Average_vector_int(vector<int> input_vector) {
    if (input_vector.empty()) return 0;
    int result = 0;

    for (int num : input_vector) {
        result += num;
    }
    return result /= input_vector.size();
}

// Вычисление рейтинга документов.
// Рейтинг добавленного документа равен среднему арифметическому оценок документа.
void TestRatingCalculation() {
    
    string query = "two four";
    {
        SearchServer server;        
        //Добавим 3 документа с нулевым рейтингом, положительным, отрицательным 
        vector<vector<int>> ratings_for_adding = { 
            {50,  -90}, 
            {100, 0, -100}, 
            {-10, 60, 0, -30},
            {1000, 2000, -2700}
        };
        //Рассчитаем получившиеся рейтинги, как среднее арифметическое, например ((-10) + 60 + 0 + (-30))/4
        vector<int> expected_result_rating;
        for (vector<int> each_document_rating : ratings_for_adding) {
            expected_result_rating.push_back(Average_vector_int(each_document_rating));
        }

        server.AddDocument(34, "two four"s, DocumentStatus::ACTUAL, ratings_for_adding[0]);
        server.AddDocument(62, "one two four five"s, DocumentStatus::ACTUAL, ratings_for_adding[1]);
        server.AddDocument(41, "one two three four five"s, DocumentStatus::ACTUAL, ratings_for_adding[2]);
        server.AddDocument(7, "three four five"s, DocumentStatus::ACTUAL, ratings_for_adding[3]);
        
        //Так как последовательность ввиду релевантности мы знаем        
        vector<Document> result = server.FindTopDocuments(query);
        vector<int> expected_result_id = { 34, 62, 41, 7 };

        for (int i = 0; i < result.size(); ++i) {
            ASSERT_EQUAL(result[i].rating, expected_result_rating[i]);
            ASSERT_EQUAL(result[i].id, expected_result_id[i]);
        }
    }
}

// Фильтрация результатов поиска с использованием предиката, задаваемого пользователем.
void TestFilterWithPredicate() {
    string query = "two four";
    SearchServer server;

    server.AddDocument(34, "two four"s, DocumentStatus::ACTUAL, { -20 });
    server.AddDocument(62, "one two four five"s, DocumentStatus::ACTUAL, { 0 });
    server.AddDocument(41, "one two three four five"s, DocumentStatus::ACTUAL, { 5 });
    server.AddDocument(7, "three four five"s, DocumentStatus::ACTUAL, { 100 });
    //Находим документ с ид = 34
    vector<Document> result = server.FindTopDocuments(query,
        [](int document_id, DocumentStatus status, int rating) {
            return document_id == 34;
        }        
    );
    //Такой документ должен быть один
    ASSERT_EQUAL(result.size(), 1u);
    ASSERT_EQUAL(result[0].id, 34u);    

    //Находим документs c рейтингом больше 0
    result = server.FindTopDocuments(query,
        [](int document_id, DocumentStatus status, int rating) {
            return rating > 0;
        }
    );
    //Таких документов должно быть 2
    ASSERT_EQUAL(result.size(), 2u);
    ASSERT_EQUAL(result[0].id, 41u);
    ASSERT_EQUAL(result[1].id, 7u);    

    //Находим документs c четным рейтингом
    result = server.FindTopDocuments(query,
        [](int document_id, DocumentStatus status, int rating) {
            return document_id % 2 == 0;
        }
    );
    //Таких документов должно быть 2
    ASSERT_EQUAL(result.size(), 2u);
    
    //Попробуем найти несуществующий документ
    result = server.FindTopDocuments(query,
        [](int document_id, DocumentStatus status, int rating) {
            return document_id == 4;
        }
    );    
    ASSERT(result.empty());
    
    //Попробуем найти по двум условиям
    result = server.FindTopDocuments(query,
        [](int document_id, DocumentStatus status, int rating) {
            return (document_id == 34) && (rating == -20);
        }
    );
    ASSERT_EQUAL(result[0].id, 34u);
    ASSERT_EQUAL(result.size(), 1u);
}

//Тест поиска документов, имеющих заданный статус
void TestSearchByStatus() {
    SearchServer server;    
    string query = "two four";
    //Добавим 6 документов: 2 в категории ACTUAL, 1 - IRRELEVANT, 0 - REMOVED, 2 - BANNED 
    server.AddDocument(34, "two four"s, DocumentStatus::ACTUAL, { 0 });
    server.AddDocument(62, "one two four five"s, DocumentStatus::ACTUAL, { 0 });
    server.AddDocument(41, "one two three four five"s, DocumentStatus::BANNED, { 0 });
    server.AddDocument(7, "three four five"s, DocumentStatus::BANNED, { 0 });
    server.AddDocument(43, "one two three four five"s, DocumentStatus::IRRELEVANT, { 0 });

    //Найдём кол-во документов и сравним с ожидаемым
    vector<Document> result = server.FindTopDocuments(query, DocumentStatus::ACTUAL);
    ASSERT_EQUAL(result.size(), 2u);    

    result = server.FindTopDocuments(query, DocumentStatus::IRRELEVANT);
    ASSERT_EQUAL(result.size(), 1u);    

    result = server.FindTopDocuments(query, DocumentStatus::REMOVED);
    ASSERT_EQUAL(result.size(), 0u);    

    result = server.FindTopDocuments(query, DocumentStatus::BANNED);
    ASSERT_EQUAL(result.size(), 2u);    

    //Проверим что если нет совпадений со строкой запроса то результата не будет
    query = "three";
    result = server.FindTopDocuments(query, DocumentStatus::ACTUAL);
    ASSERT_EQUAL(result.size(), 0u);
}

// Тест корректного вычисление релевантности найденных документов.
void TestRelevanceCalculating() {
    SearchServer server;    
    //Добавим 3 документа
    string query = "two four";
    vector<string> adding_documents_data = {
        "one two three four five six"s,
        "one two four two"s,
        "one three four"s
    };

    server.AddDocument(41, adding_documents_data[0], DocumentStatus::ACTUAL, { 0 });
    server.AddDocument(34, adding_documents_data[1], DocumentStatus::ACTUAL, { 0 });
    server.AddDocument(15, adding_documents_data[2], DocumentStatus::ACTUAL, { 0 });
    //Рассчитаем релевантность для добавленных документов
    vector<double> expected_relevance; //Релевантность для каждого добавлнного документа
    {
        //Рассчитаем IDF слов запроса
        //Количество всех документов делят на количество тех, где встречается слово
        //к результату применяют лографим
        map<string, double> expected_IDF;
        //Для слова запроса two(оно встречается в 2 из 3 документах):
        expected_IDF["two"] = log(3 / 2.0);
        //Для слова запроса four(встречается в каждом документе):
        expected_IDF["four"] = log(3 / 3.0);

        //TF каждого слова запроса в документе
        //Для конкретного слова и конкретного документа это доля, которую данное слово занимает среди всех.
        //Т.е. если в документе "one two three four five six" слово "four" встречается 1 раз то TF = 1 / 6
        
        //Для первого документа TF-IDF
        expected_relevance.push_back((1 / 6.0) * expected_IDF["two"] + (1 / 6.0) * expected_IDF["four"]);
        //Для второго документа TF-IDF
        expected_relevance.push_back((2 / 4.0) * expected_IDF["two"] + (1 / 4.0) * expected_IDF["four"]);
        //Для третьего документа TF-IDF
        expected_relevance.push_back((0 / 3.0) * expected_IDF["two"] + (1 / 3.0) * expected_IDF["four"]);

        //Отсортируем полученный результат
        sort(expected_relevance.begin(), expected_relevance.end(), greater<double>());
    }
    
    vector<Document> result = server.FindTopDocuments(query, DocumentStatus::ACTUAL);

    //сравним полученные значения с расчтёными с погрешностью в 0.001
    for (int i = 0; i < result.size(); ++i) {
        ASSERT(abs(result[i].relevance - expected_relevance[i]) <= 0.001);
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestSplitIntoWordsWithDifferentErrors);

    RUN_TEST(TestAddingDocumentsStopWordsExcludingStopWords);
    RUN_TEST(TestExludeDocumentsWithMinusWordsFromResults);
    RUN_TEST(TestMatchDocuments);
    RUN_TEST(TestSortingByRelevance);
    RUN_TEST(TestRatingCalculation);
    RUN_TEST(TestFilterWithPredicate);
    RUN_TEST(TestSearchByStatus);
    RUN_TEST(TestRelevanceCalculating);
}

//-----------Окончание модульных тестов поисковой системы------------

int main() {
    TestSearchServer();   
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl << endl;
    
    //----------------Произвольные тесты-----------------------------   
    setlocale(LC_ALL, "Russian");
    SearchServer search_server("и в на"s);    

    AddDocument(search_server, 1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    AddDocument(search_server, 1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
    AddDocument(search_server, -1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
    AddDocument(search_server, 3, "большой пёс скво\x12рец евгений"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
    AddDocument(search_server, 4, "большой пёс скворец евгений"s, DocumentStatus::ACTUAL, { 1, 1, 1 });       

    FindTopDocuments(search_server, "пушистый -пёс"s);
    FindTopDocuments(search_server, "пушистый --кот"s);
    FindTopDocuments(search_server, "пушистый -"s);

    MatchDocuments(search_server, "пушистый пёс"s);
    MatchDocuments(search_server, "модный -кот"s);
    MatchDocuments(search_server, "модный --пёс"s);
    MatchDocuments(search_server, "пушистый - хвост"s);
}
