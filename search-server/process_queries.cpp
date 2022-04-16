#include "process_queries.h"

using namespace std;

vector<vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {

    vector<std::vector<Document>> result(queries.size());
    transform(execution::par,
        queries.begin(),
        queries.end(),
        result.begin(),
        [&search_server](const string& query) {
            return move(search_server.FindTopDocuments(execution::par, query));
        }
    );

    return result;
}

std::vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {

    vector<vector<Document>> ProcessQueries_result = ProcessQueries(search_server, queries);

    size_t result_num = 0;
    for (vector<Document> single_query_result : ProcessQueries_result) {
        result_num += single_query_result.size();
    }

    vector<Document> result(result_num);

    auto it_to_paste = result.begin();
    for (vector<Document> single_query_result : ProcessQueries_result) {        
        it_to_paste = copy(execution::par, single_query_result.begin(), single_query_result.end(), it_to_paste);
    }

    // ѕоследовательный перебор всех векторов в векторе ProcessQueries_result и добавление в результирующий вектор
    /*vector<Document> result;
    for (vector<Document> vec : ProcessQueries_result) {
        for (Document doc : vec) {
            result.push_back(doc);
        }
    }*/

    return result;
}