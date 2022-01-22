//#include "search_server.h"
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
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id,
            DocumentData{
                ComputeAverageRating(ratings),
                status
            });
    }

    template <typename KeyMapper>
    vector<Document> FindTopDocuments(const string& raw_query, KeyMapper key_mapper) const {
        const Query query = ParseQuery(raw_query);

        auto matched_documents = FindAllDocuments(query, key_mapper);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                    return lhs.rating > rhs.rating;
                }
                else {
                    return lhs.relevance > rhs.relevance;
                }
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query,
            [](int document_id, DocumentStatus status, int rating) {
                return status == DocumentStatus::ACTUAL;
            }
        );
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
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
        return { matched_words, documents_.at(document_id).status };
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
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
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename KeyMapper>
    vector<Document> FindAllDocuments(const Query& query, KeyMapper key_mapper) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);

            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                //depending on key_mapper type
                if constexpr (is_same_v<KeyMapper, DocumentStatus>) {//KeyMapper == DocumentStatus
                    if (documents_.at(document_id).status == key_mapper)
                    {
                        document_to_relevance[document_id] += term_freq * inverse_document_freq;
                    }
                }
                else {//KeyMapper == lambda func
                    if (key_mapper(document_id, documents_.at(document_id).status, documents_.at(document_id).rating))
                    {
                        document_to_relevance[document_id] += term_freq * inverse_document_freq;
                    }
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
            matched_documents.push_back({
                            document_id,
                            relevance,
                            documents_.at(document_id).rating
                });

        }
        return matched_documents;
    }
};

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating
        << " }"s << endl;
}

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
// -------- ������ ��������� ������ ��������� ������� ----------

// ���� ���������, ��� ��������� ������� ��������� ����-����� ��� ���������� ����������
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    // ������� ����������, ��� ����� �����, �� ��������� � ������ ����-����,
    // ������� ������ ��������
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    // ����� ����������, ��� ����� ����� �� �����, ��������� � ������ ����-����,
    // ���������� ������ ���������
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}

// ���� ��������� ��������� ����� ����, ��������� ���������� ����� ����� � ������ �� ���������� � ���������
void TestExludeDocumentsWithMinusWordsFromResults() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };

    //��������� ��� � ������ ������ ��������� � ����� ������ �� �� ������ � ����������
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(14, "dog outside the city"s, DocumentStatus::ACTUAL, { 2, 3, 4 });
        const auto found_docs = server.FindTopDocuments("city -cat"s);
        //���������� ��� � ����������� ���� ������ ���� �������� � ������� ��� ����� �����
        ASSERT_EQUAL(found_docs.size(), 1);
    }

    //��������� ���� � ������� ����� ������ ���� ����� �� ��������� ����� ������
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(14, "dog outside the city"s, DocumentStatus::ACTUAL, { 2, 3, 4 });
        const auto found_docs = server.FindTopDocuments("-cat"s);
        ASSERT(found_docs.empty());
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
        const auto result = server.MatchDocument(query, doc_id);
        //��������� ������ ������ ������� ��������� cat � the ��� ��� ��� ������������ � ��������� ���������
        //� ��� ������ ACTUCAL
        vector<string> expected_result = { "cat", "the" };
        ASSERT_EQUAL((get<0>(result)), expected_result);
    }
    //�������� ���������� ��������� ���� ��� ������������ ����� �����
    query = "cat dog -city outside the";
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto result = server.MatchDocument(query, doc_id);
        //��������� ������ ������ ������� ������ ������ ��� ��� � ������� ���������� ����� ����� city
        vector<string> expected_result = { };
        ASSERT_EQUAL((get<0>(result)), expected_result);
    }
    //���� ����������� ���� ��� ������ ������������ ������ ������
    query = "dog inside box";
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto result = server.MatchDocument(query, doc_id);
        //��������� ������ ������ ������� ������ ������ ��� ��� � ������� ���������� ����� ����� city        
        ASSERT(get<0>(result).empty());
    }
}

// ���������� ��������� ���������� �� �������������.������������ ��� ������ ���������� ���������� ������ ����
// ������������� � ������� �������� �������������.
void TestSortingByRelevance() {
    string query = "two four";
    {
        SearchServer server;
        //������� 4 ��������� � ������ �������������� � ��������
        server.AddDocument(41, "one two three four five"s, DocumentStatus::ACTUAL, { 2, 3, 4 });
        server.AddDocument(34, "two four"s, DocumentStatus::ACTUAL, { 2, 3, 4 });
        server.AddDocument(62, "one two four five"s, DocumentStatus::ACTUAL, { 2, 3, 4 });
        server.AddDocument(7, "three four five"s, DocumentStatus::ACTUAL, { 2, 3, 4 });

        vector<Document> result = server.FindTopDocuments(query);
        //�������� ��� ��������� ����������� ������������� ������ ����������         
        for (int i = 1; i < (result.size()); ++i) {
            double tmp = result[i - 1].relevance;
            ASSERT((result[i].relevance < tmp));
        }
    }

    query = "-two one six";
    //������� ����� ����� � ����� � �������� ��� ��������� ��� ���������
    {
        SearchServer server;
        //������� 4 ��������� � ������ �������������� � ��������
        server.AddDocument(41, "one two three four five"s, DocumentStatus::ACTUAL, { 2, 3, 4 });
        server.AddDocument(34, "two four"s, DocumentStatus::ACTUAL, { 2, 3, 4 });
        server.AddDocument(62, "one four five"s, DocumentStatus::ACTUAL, { 2, 3, 4 });
        server.AddDocument(7, "three four five six"s, DocumentStatus::ACTUAL, { 2, 3, 4 });
        //� ����������� � ���������� ������ ���� �������� ���������� � ��������� ������������������:
        //7 62 � �� ������ ���� 2
        vector<Document> result = server.FindTopDocuments(query);
        vector<int> expected_result = { 7, 62 };
        ASSERT_EQUAL(result.size(), 2);
        for (int i = 0; i < result.size(); ++i) {
            ASSERT_EQUAL(result[i].id, expected_result[i]);
        }
    }
}

// ���������� �������� ����������.
// ������� ������������ ��������� ����� �������� ��������������� ������ ���������.
void TestRatingCalculation() {
    //������� 3 ��������� �� �������� ����������: 5, -20, 0, 100
    string query = "two four";
    {
        SearchServer server;
        //������� 4 ���������        
        server.AddDocument(34, "two four"s, DocumentStatus::ACTUAL, { 50,  -90 });
        server.AddDocument(62, "one two four five"s, DocumentStatus::ACTUAL, { 100, 0, -100 });
        server.AddDocument(41, "one two three four five"s, DocumentStatus::ACTUAL, { -10, 60, 0, -30 });
        server.AddDocument(7, "three four five"s, DocumentStatus::ACTUAL, { 1000, 2000, -2700 });
        //��� ��� ������������������ ����� ������������� �� �����, ����� ���������� ��������� ���������        
        vector<Document> result = server.FindTopDocuments(query);
        vector<int> expected_result_rating = { -20, 0, 5, 100 };
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
    server.AddDocument(34, "two four"s, DocumentStatus::ACTUAL, { 50,  -90 });
    server.AddDocument(62, "one two four five"s, DocumentStatus::ACTUAL, { 100, 0, -100 });
    server.AddDocument(41, "one two three four five"s, DocumentStatus::ACTUAL, { -10, 60, 0, -30 });
    server.AddDocument(7, "three four five"s, DocumentStatus::ACTUAL, { 1000, 2000, -2700 });
    //������� �������� � �� = 34
    vector<Document> result = server.FindTopDocuments(query,
        [](int document_id, DocumentStatus status, int rating) {
            return document_id == 34;
        }
    );
    //����� �������� ������ ���� ����
    ASSERT_EQUAL(result.size(), 1);
    ASSERT_EQUAL(result[0].id, 34);

    //������� ��������s c ��������� ������ 0
    result = server.FindTopDocuments(query,
        [](int document_id, DocumentStatus status, int rating) {
            return rating > 0;
        }
    );
    //����� ���������� ������ ���� 2
    ASSERT_EQUAL(result.size(), 2);
    ASSERT_EQUAL(result[0].id, 41);
    ASSERT_EQUAL(result[1].id, 7);

    //������� ��������s c ������ ���������
    result = server.FindTopDocuments(query,
        [](int document_id, DocumentStatus status, int rating) {
            return document_id % 2 == 0;
        }
    );
    //����� ���������� ������ ���� 2
    ASSERT_EQUAL(result.size(), 2);

    //��������� ����� �������������� ��������
    result = server.FindTopDocuments(query,
        [](int document_id, DocumentStatus status, int rating) {
            return document_id == 4;
        }
    );    
    ASSERT_EQUAL(result.size(), 0);

    //��������� ����� �� ���� ��������
    result = server.FindTopDocuments(query,
        [](int document_id, DocumentStatus status, int rating) {
            return (document_id == 34) && (rating == -20);
        }
    );
    ASSERT_EQUAL(result[0].id, 34);
    ASSERT_EQUAL(result.size(), 1);
}

//���� ������ ����������, ������� �������� ������
void TestSearchByStatus() {
    SearchServer server;
    string query = "two four";
    //������� 6 ����������: 2 � ��������� ACTUAL, 1 - IRRELEVANT, 0 - REMOVED, 2 - BANNED 
    server.AddDocument(34, "two four"s, DocumentStatus::ACTUAL, { 50,  -90 });
    server.AddDocument(62, "one two four five"s, DocumentStatus::ACTUAL, { 100, 0, -100 });
    server.AddDocument(41, "one two three four five"s, DocumentStatus::BANNED, { -10, 60, 0, -30 });
    server.AddDocument(7, "three four five"s, DocumentStatus::BANNED, { 1000, 2000, -2700 });
    server.AddDocument(43, "one two three four five"s, DocumentStatus::IRRELEVANT, { -10, 60, 0, -30 });

    //����� ���-�� ���������� � ������� � ���������
    vector<Document> result = server.FindTopDocuments(query, DocumentStatus::ACTUAL);
    ASSERT_EQUAL(result.size(), 2);

    result = server.FindTopDocuments(query, DocumentStatus::IRRELEVANT);
    ASSERT_EQUAL(result.size(), 1);

    result = server.FindTopDocuments(query, DocumentStatus::REMOVED);
    ASSERT_EQUAL(result.size(), 0);

    result = server.FindTopDocuments(query, DocumentStatus::BANNED);
    ASSERT_EQUAL(result.size(), 2);

    //�������� ��� ���� ��� ���������� �� ������� ������� �� ���������� �� �����
    query = "three";
    result = server.FindTopDocuments(query, DocumentStatus::ACTUAL);
    ASSERT_EQUAL(result.size(), 0);
}

// ���� ����������� ���������� ������������� ��������� ����������.
void TestRelevanceCalculating() {
    SearchServer server;
    //������� 3 ��������� � ����������� ������� ��������������
    string query = "two four";

    server.AddDocument(41, "one two three four five"s, DocumentStatus::ACTUAL, { 2, 3, 4 });
    server.AddDocument(34, "one two three"s, DocumentStatus::ACTUAL, { 2, 3, 4 });
    server.AddDocument(15, "one three"s, DocumentStatus::ACTUAL, { 2, 3, 4 });
    //� ���������� ������ ���� ����. �������������: 0.30082, 0.13503 � 0
    //�� ��� ��� � 3 ��������� ��� ����������� ���� � ����������� ������ ��� �� �����

    vector<Document> result = server.FindTopDocuments(query, DocumentStatus::ACTUAL);
    //������� ���������� �������� � ��������� � ������������ � 0.001
    ASSERT(abs(result[0].relevance - 0.30082) <= 0.001);
    ASSERT_EQUAL(result[0].id, 41);
    ASSERT(abs(result[1].relevance - 0.13503) <= 0.001);
    ASSERT_EQUAL(result[1].id, 34);
}

// ������� TestSearchServer �������� ������ ����� ��� ������� ������
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExludeDocumentsWithMinusWordsFromResults);
    RUN_TEST(TestMatchDocuments);
    RUN_TEST(TestSortingByRelevance);
    RUN_TEST(TestRatingCalculation);
    RUN_TEST(TestFilterWithPredicate);
    RUN_TEST(TestSearchByStatus);
    RUN_TEST(TestRelevanceCalculating);
}

// --------- ��������� ��������� ������ ��������� ������� -----------

int main() {
    TestSearchServer();
    // ���� �� ������ ��� ������, ������ ��� ����� ������ �������
    cout << "Search server testing finished"s << endl;
}