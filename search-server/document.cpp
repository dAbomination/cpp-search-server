#include "document.h"

using namespace std;

ostream& operator<<(ostream& out, const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s;

    return out;
}