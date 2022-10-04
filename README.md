# Поисковая система
Представляет собой хранилище документов и различные методы взаимодействя с ними. Документ состоят из id, релевентности (TF-IDF) и рейтинга. id и рейтинг задаются пользователем при добавлении документа. В отдельном контейнере хранятся данные слов из которых состоят документы и в каких документах они присутствуют.
Поисковая система поддерживает следующие операции:
- добавление (AddDocument), удаление(RemoveDocument) документов и стоп слов(слова, которые игнорируются в документах, могут быть заданы при помощи конструктора или методом SetStopWords);
- поиск наиболее релевантных документов (FindTopDocuments) по заданным словам или по другим параметрам;
- поиск любых документов в которых встречаются искомые слова (MatchDocument);

Дополнительный функционал:
- удаление дубликатов (RemoveDuplicates);
- очередь запросов (RequestQueque);
- постраничная выдача результатов поиска (Paginator);
- обработка большого кол-ва запросов (ProcessQueries).

Ускорение выполнения большого кол-ва запросов (более чем в 2 раза) достигается, использованием параллельных алгоритмов для поиска и удаления документов (возможно и последовательное).
concurent_map.h предоставляет многопоточную работу со словарями (map).
Используется стандарт C++17.
