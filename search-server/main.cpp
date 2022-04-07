#include "search_server.h"
#include "paginator.h"
#include "request_queue.h"
#include "remove_duplicates.h"
#include "process_queries.h"


#include <random>

#define TESTS

#ifdef TESTS
#include "tests.h"
#endif // DEBUG

int main() {  
    using namespace std;
    setlocale(LC_ALL, "Russian");

    #ifdef TESTS
    TestSearchServer();
    // ���� �� ������ ��� ������, ������ ��� ����� ������ �������
    cout << "Search server testing finished"s << endl << endl;
    #endif // DEBUG    
    
    //----------------������������ �����-----------------------------     
    {
        SearchServer search_server("and with"s);

        int id = 0;
        for (
            const string& text : {
                "funny pet and nasty rat"s,
                "funny pet with curly hair"s,
                "funny pet and not very nasty rat"s,
                "pet with rat and rat and rat"s,
                "nasty rat with curly hair"s,
            }
            ) {
            search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, { 1, 2 });
        }

        const vector<string> queries = {
            "nasty rat -not"s,
            "not very funny nasty pet"s,
            "curly hair"s
        };
        for (const Document& document : ProcessQueriesJoined(search_server, queries)) {
            cout << "Document "s << document.id << " matched with relevance "s << document.relevance << endl;
        }
    }

    return 0;
}
