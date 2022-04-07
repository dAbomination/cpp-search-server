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
        
    }

    return 0;
}
