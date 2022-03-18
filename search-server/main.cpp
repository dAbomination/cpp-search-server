#include "search_server.h"
#include "paginator.h"
#include "request_queue.h"
#include "remove_duplicates.h"

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

        AddDocument(search_server, 1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        AddDocument(search_server, 2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

        // �������� ��������� 2, ����� �����
        AddDocument(search_server, 3, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

        // ������� ������ � ����-������, ������� ����������
        AddDocument(search_server, 4, "funny pet and curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

        // ��������� ���� ����� ��, ������� ���������� ��������� 1
        AddDocument(search_server, 5, "funny funny pet and nasty nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });

        // ���������� ����� �����, ���������� �� ��������
        AddDocument(search_server, 6, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });

        // ��������� ���� ����� ��, ��� � id 6, �������� �� ������ �������, ������� ����������
        AddDocument(search_server, 7, "very nasty rat and not very funny pet"s, DocumentStatus::ACTUAL, { 1, 2 });

        // ���� �� ��� �����, �� �������� ����������
        AddDocument(search_server, 8, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, { 1, 2 });

        // ����� �� ������ ����������, �� �������� ����������
        AddDocument(search_server, 9, "nasty rat with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

        cout << "Before duplicates removed: "s << search_server.GetDocumentCount() << endl;
        RemoveDuplicates(search_server);
        cout << "After duplicates removed: "s << search_server.GetDocumentCount() << endl;
    }
    


    return 0;
}
