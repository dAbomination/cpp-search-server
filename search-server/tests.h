#pragma once

#include <iostream>
#include <vector>
#include <set>
#include <map>

#include "search-server.h"

using namespace std;

//-----------------------------------------
// ������� ��� ������������
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
//--------- ������ ��������� ������ ��������� ������� ----------
// ���� ������� SplitIntoWords ��� ������������� ��������
void TestSplitIntoWordsWithDifferentErrors() {
    const vector<string> expected_result = { "first"s, "second"s, "third"s };
    vector<string> result = SplitIntoWords("   first       second            third    "s);
    ASSERT_EQUAL(result, expected_result);
}

// ���� ���������: ����������������� ������������� � ���������� �����������,
// ��� ��������� ������� ��������� ����-����� ��� ���������� ����������,
// � ����� ����� ������ ��� ������� �������� �������� � ����������� id ��� ���� �� ������ ����
void TestAddingDocumentsStopWordsExcludingStopWords() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 0 };

    // ���������� �������� �������� � ������������� id ��� � ������������� id
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        // �������� �������� �������� �������� � ������ �������� ����������
        try {
            server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        }
        catch (const exception& e) {
            ASSERT_EQUAL(e.what(), "document_id already exist or below zero"s);
        }
        // �������� �������� �������� � ������������� id
        try {
            server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        }
        catch (const exception& e) {
            ASSERT_EQUAL(e.what(), "document_id already exist or below zero"s);
        }
    }
    // ����������, ��� ����� �����, �� ��������� � ������ ����-����,
    // ������� ������ ��������
    {
        SearchServer server;

        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        vector<Document> result = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(result.size(), 1u);
        ASSERT_EQUAL(result[0].id, doc_id);
    }

    // ����� ����������, ��� ����� ����� �� �����, ��������� � ������ ����-����,
    // ���������� ������ ���������
    {
        SearchServer server;

        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        vector<Document> result = server.FindTopDocuments("in"s);
        ASSERT(result.empty());
    }
}

// ���� ��������� ��������� ����� ����, ��������� ���������� ����� ����� � ������ �� ���������� � ���������
void TestExludeDocumentsWithMinusWordsFromResults() {
    const vector<int> ratings = { 0 };
    //��������� ��� � ������ ������ ��������� � ����� ������ �� �� ������ � ����������
    {
        SearchServer server;


        server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(14, "dog outside the city"s, DocumentStatus::ACTUAL, ratings);
        vector<Document> result = server.FindTopDocuments("city -cat"s);
        //���������� ��� � ����������� ���� ������ ���� �������� � ������� ��� ����� �����
        ASSERT_EQUAL(result.size(), 1u);
    }

    //��������� ���� � ������� ����� ������ ���� ����� �� ��������� ����� ������
    {
        SearchServer server;

        server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(14, "dog outside the city"s, DocumentStatus::ACTUAL, ratings);
        vector<Document> result = server.FindTopDocuments("-cat"s);
        ASSERT(result.empty());
    }
}

// ���� ��������� ������� ����������.
// ��� �������� ��������� �� ���������� ������� ������ ���� ���������� ��� ����� �� ���������� �������,
// �������������� � ���������.���� ���� ������������ ���� �� �� ������ ����� - �����,
// ������ ������������ ������ ������ ����.
void TestMatchDocuments() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    string query = "cat dog outside the";
    //�������� ���� ��� ������� MatchDocument ������� ����������� �����
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        vector<string> result_matched_words;
        DocumentStatus status_matched_document_status;
        //������������� ������, ������� ���������� �� ������ � ���������� ���������� ������� MatchDocument

        tie(result_matched_words, status_matched_document_status) = server.MatchDocument(query, doc_id);

        //��������� ������ ������ ������� ��������� cat � the ��� ��� ��� ������������ � ��������� ���������        
        vector<string> expected_result = { "cat", "the" };
        ASSERT_EQUAL((result_matched_words), expected_result);
    }
    //�������� ���������� ��������� ���� ��� ������������ ����� �����
    query = "cat dog -city outside the";
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        vector<string> result_matched_words;
        DocumentStatus status_matched_document_status;
        //������������� ������, ������� ���������� �� ������ � ���������� ���������� ������� MatchDocument

        tie(result_matched_words, status_matched_document_status) = server.MatchDocument(query, doc_id);

        //��������� ������ ������ ������� ������ ������ ��� ��� � ������� ���������� ����� ����� city
        ASSERT(result_matched_words.empty());
    }
    //���� ����������� ���� ��� ������ ������������ ������ ������
    query = "dog inside box";
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        vector<string> result_matched_words;
        DocumentStatus status_matched_document_status;
        //������������� ������, ������� ���������� �� ������ � ���������� ���������� ������� MatchDocument        
        tie(result_matched_words, status_matched_document_status) = server.MatchDocument(query, doc_id);

        //��������� ������ ������ ������� ������ ������ ��� ��� � ������� ���������� ����� ����� city        
        ASSERT(result_matched_words.empty());
    }
}

// ���������� ��������� ���������� �� �������������.������������ ��� ������ ���������� ���������� ������ ����
// ������������� � ������� �������� �������������.
void TestSortingByRelevance() {
    string query = "two four";
    {
        SearchServer server;
        // ������� 4 ��������� � ������ �������������� � ��������
        server.AddDocument(41, "one two three four five"s, DocumentStatus::ACTUAL, { 0 });
        server.AddDocument(34, "two four"s, DocumentStatus::ACTUAL, { 0 });
        server.AddDocument(62, "one two four five"s, DocumentStatus::ACTUAL, { 0 });
        server.AddDocument(7, "three four five"s, DocumentStatus::ACTUAL, { 0 });

        vector<Document> result = server.FindTopDocuments(query);
        // �������� ��� ��������� ����������� ������������� ������ ����������         
        for (int i = 1; i < (result.size()); ++i) {
            double previous_relevance = result[i - 1].relevance;
            ASSERT((result[i].relevance < previous_relevance));
        }
    }

    query = "-two one six";
    // ������� ����� ����� � ������ � �������� ��� ��������� ��� ���������
    {
        SearchServer server;

        // ������� 4 ��������� � ������ �������������� � ��������
        server.AddDocument(41, "one two three four five"s, DocumentStatus::ACTUAL, { 0 });
        server.AddDocument(34, "one four"s, DocumentStatus::ACTUAL, { 0 });
        server.AddDocument(62, "one two four five"s, DocumentStatus::ACTUAL, { 0 });
        server.AddDocument(7, "three four five six"s, DocumentStatus::ACTUAL, { 0 });
        // ��-�� ���� ����� � ������� ������ ���� �� ���������� ��� ���������        
        vector<Document> result = server.FindTopDocuments(query);

        ASSERT_EQUAL(result.size(), 2u);
        // �������� ��� ��������� ����������� ������������� ������ ����������         
        for (int i = 1; i < (result.size()); ++i) {
            double previous_relevance = result[i - 1].relevance;
            ASSERT((result[i].relevance < previous_relevance));
        }
    }
}
//������� ��� ������� �������� ��������������� ��� ������� ����� ����� int
int Average_vector_int(vector<int> input_vector) {
    if (input_vector.empty()) return 0;
    int result = 0;

    for (int num : input_vector) {
        result += num;
    }
    return result /= input_vector.size();
}

// ���������� �������� ����������.
// ������� ������������ ��������� ����� �������� ��������������� ������ ���������.
void TestRatingCalculation() {

    string query = "two four";
    {
        SearchServer server;
        //������� 3 ��������� � ������� ���������, �������������, ������������� 
        vector<vector<int>> ratings_for_adding = {
            {50,  -90},
            {100, 0, -100},
            {-10, 60, 0, -30},
            {1000, 2000, -2700}
        };
        //���������� ������������ ��������, ��� ������� ��������������, �������� ((-10) + 60 + 0 + (-30))/4
        vector<int> expected_result_rating;
        for (vector<int> each_document_rating : ratings_for_adding) {
            expected_result_rating.push_back(Average_vector_int(each_document_rating));
        }

        server.AddDocument(34, "two four"s, DocumentStatus::ACTUAL, ratings_for_adding[0]);
        server.AddDocument(62, "one two four five"s, DocumentStatus::ACTUAL, ratings_for_adding[1]);
        server.AddDocument(41, "one two three four five"s, DocumentStatus::ACTUAL, ratings_for_adding[2]);
        server.AddDocument(7, "three four five"s, DocumentStatus::ACTUAL, ratings_for_adding[3]);

        //��� ��� ������������������ ����� ������������� �� �����        
        vector<Document> result = server.FindTopDocuments(query);
        vector<int> expected_result_id = { 34, 62, 41, 7 };

        for (int i = 0; i < result.size(); ++i) {
            ASSERT_EQUAL(result[i].rating, expected_result_rating[i]);
            ASSERT_EQUAL(result[i].id, expected_result_id[i]);
        }
    }
}

// ���������� ����������� ������ � �������������� ���������, ����������� �������������.
void TestFilterWithPredicate() {
    string query = "two four";
    SearchServer server;

    server.AddDocument(34, "two four"s, DocumentStatus::ACTUAL, { -20 });
    server.AddDocument(62, "one two four five"s, DocumentStatus::ACTUAL, { 0 });
    server.AddDocument(41, "one two three four five"s, DocumentStatus::ACTUAL, { 5 });
    server.AddDocument(7, "three four five"s, DocumentStatus::ACTUAL, { 100 });
    //������� �������� � �� = 34
    vector<Document> result = server.FindTopDocuments(query,
        [](int document_id, DocumentStatus status, int rating) {
            return document_id == 34;
        }
    );
    //����� �������� ������ ���� ����
    ASSERT_EQUAL(result.size(), 1u);
    ASSERT_EQUAL(result[0].id, 34u);

    //������� ��������s c ��������� ������ 0
    result = server.FindTopDocuments(query,
        [](int document_id, DocumentStatus status, int rating) {
            return rating > 0;
        }
    );
    //����� ���������� ������ ���� 2
    ASSERT_EQUAL(result.size(), 2u);
    ASSERT_EQUAL(result[0].id, 41u);
    ASSERT_EQUAL(result[1].id, 7u);

    //������� ��������s c ������ ���������
    result = server.FindTopDocuments(query,
        [](int document_id, DocumentStatus status, int rating) {
            return document_id % 2 == 0;
        }
    );
    //����� ���������� ������ ���� 2
    ASSERT_EQUAL(result.size(), 2u);

    //��������� ����� �������������� ��������
    result = server.FindTopDocuments(query,
        [](int document_id, DocumentStatus status, int rating) {
            return document_id == 4;
        }
    );
    ASSERT(result.empty());

    //��������� ����� �� ���� ��������
    result = server.FindTopDocuments(query,
        [](int document_id, DocumentStatus status, int rating) {
            return (document_id == 34) && (rating == -20);
        }
    );
    ASSERT_EQUAL(result[0].id, 34u);
    ASSERT_EQUAL(result.size(), 1u);
}

//���� ������ ����������, ������� �������� ������
void TestSearchByStatus() {
    SearchServer server;
    string query = "two four";
    //������� 6 ����������: 2 � ��������� ACTUAL, 1 - IRRELEVANT, 0 - REMOVED, 2 - BANNED 
    server.AddDocument(34, "two four"s, DocumentStatus::ACTUAL, { 0 });
    server.AddDocument(62, "one two four five"s, DocumentStatus::ACTUAL, { 0 });
    server.AddDocument(41, "one two three four five"s, DocumentStatus::BANNED, { 0 });
    server.AddDocument(7, "three four five"s, DocumentStatus::BANNED, { 0 });
    server.AddDocument(43, "one two three four five"s, DocumentStatus::IRRELEVANT, { 0 });

    //����� ���-�� ���������� � ������� � ���������
    vector<Document> result = server.FindTopDocuments(query, DocumentStatus::ACTUAL);
    ASSERT_EQUAL(result.size(), 2u);

    result = server.FindTopDocuments(query, DocumentStatus::IRRELEVANT);
    ASSERT_EQUAL(result.size(), 1u);

    result = server.FindTopDocuments(query, DocumentStatus::REMOVED);
    ASSERT_EQUAL(result.size(), 0u);

    result = server.FindTopDocuments(query, DocumentStatus::BANNED);
    ASSERT_EQUAL(result.size(), 2u);

    //�������� ��� ���� ��� ���������� �� ������� ������� �� ���������� �� �����
    query = "three";
    result = server.FindTopDocuments(query, DocumentStatus::ACTUAL);
    ASSERT_EQUAL(result.size(), 0u);
}

// ���� ����������� ���������� ������������� ��������� ����������.
void TestRelevanceCalculating() {
    SearchServer server;
    //������� 3 ���������
    string query = "two four";
    vector<string> adding_documents_data = {
        "one two three four five six"s,
        "one two four two"s,
        "one three four"s
    };

    server.AddDocument(41, adding_documents_data[0], DocumentStatus::ACTUAL, { 0 });
    server.AddDocument(34, adding_documents_data[1], DocumentStatus::ACTUAL, { 0 });
    server.AddDocument(15, adding_documents_data[2], DocumentStatus::ACTUAL, { 0 });
    //���������� ������������� ��� ����������� ����������
    vector<double> expected_relevance; //������������� ��� ������� ����������� ���������
    {
        //���������� IDF ���� �������
        //���������� ���� ���������� ����� �� ���������� ���, ��� ����������� �����
        //� ���������� ��������� ��������
        map<string, double> expected_IDF;
        //��� ����� ������� two(��� ����������� � 2 �� 3 ����������):
        expected_IDF["two"] = log(3 / 2.0);
        //��� ����� ������� four(����������� � ������ ���������):
        expected_IDF["four"] = log(3 / 3.0);

        //TF ������� ����� ������� � ���������
        //��� ����������� ����� � ����������� ��������� ��� ����, ������� ������ ����� �������� ����� ����.
        //�.�. ���� � ��������� "one two three four five six" ����� "four" ����������� 1 ��� �� TF = 1 / 6

        //��� ������� ��������� TF-IDF
        expected_relevance.push_back((1 / 6.0) * expected_IDF["two"] + (1 / 6.0) * expected_IDF["four"]);
        //��� ������� ��������� TF-IDF
        expected_relevance.push_back((2 / 4.0) * expected_IDF["two"] + (1 / 4.0) * expected_IDF["four"]);
        //��� �������� ��������� TF-IDF
        expected_relevance.push_back((0 / 3.0) * expected_IDF["two"] + (1 / 3.0) * expected_IDF["four"]);

        //����������� ���������� ���������
        sort(expected_relevance.begin(), expected_relevance.end(), greater<double>());
    }

    vector<Document> result = server.FindTopDocuments(query, DocumentStatus::ACTUAL);

    //������� ���������� �������� � ��������� � ������������ � 0.001
    for (int i = 0; i < result.size(); ++i) {
        ASSERT(abs(result[i].relevance - expected_relevance[i]) <= 0.001);
    }
}

// ������� TestSearchServer �������� ������ ����� ��� ������� ������
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

//-----------��������� ��������� ������ ��������� �������------------