#include "remove_duplicates.h"

using namespace std;

void RemoveDuplicates(SearchServer& search_server) {
	vector<int> ids_to_delete;
	set<set<string>> words_to_document;

	// ������� id ���������� ������� ��������� �������
	for (const int document_id : search_server) {
		map<string, double> words_freq = search_server.GetWordFrequencies(document_id);
		set<string> words_in_document;
		for (auto [words, freq] : words_freq) {
			words_in_document.insert(words);
		}

		// ���� ��� �������� � ����� �������� ���� ����, �� ��������� ��� id � ��������
		if (words_to_document.count(words_in_document)) {
			ids_to_delete.push_back(document_id);
		}
		else { // ���� ��������� �� ����, �� ��������� ���� ����� ���� � ��������� words_to_document
			words_to_document.insert(words_in_document);
		}
	}

	// ������� ����� ��������� ��������� ����������
	for (const int id : ids_to_delete) {
		search_server.RemoveDocument(id);
		cout << "Found duplicate document id " << id << endl;
	}	
}