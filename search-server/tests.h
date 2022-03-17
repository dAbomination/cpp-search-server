#pragma once

#include <iostream>
#include <vector>
#include <set>
#include <map>

#include "search_server.h"

// Потом убрать это и переделать на 2 файла
using namespace std;
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

// Проверяем корректность удаления документа
void TestDeletetingDocument() {
    SearchServer search_server("and with"s);

    AddDocument(search_server, 1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    AddDocument(search_server, 2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });    
    AddDocument(search_server, 3, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });
    AddDocument(search_server, 4, "funny pet and curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });
    AddDocument(search_server, 5, "funny funny pet and nasty nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });
    AddDocument(search_server, 6, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });
    AddDocument(search_server, 7, "very nasty rat and not very funny pet"s, DocumentStatus::ACTUAL, { 1, 2 });
    AddDocument(search_server, 8, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, { 1, 2 });
    AddDocument(search_server, 9, "nasty rat with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    int doc_count = search_server.GetDocumentCount();
    ASSERT_EQUAL(doc_count, 9); // Кол-во документов должно быть равно 9

    //удаляем документ с id 5
    search_server.RemoveDocument(5);

    doc_count = search_server.GetDocumentCount();
    ASSERT_EQUAL(doc_count, 8); // Кол-во документов должно быть равно 8

    //Проверям что по зпросу с id=5 не находится ничего
    auto result = search_server.GetWordFrequencies(5);
    ASSERT(result.empty());
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
    RUN_TEST(TestDeletetingDocument);

}
//-----------Окончание модульных тестов поисковой системы------------