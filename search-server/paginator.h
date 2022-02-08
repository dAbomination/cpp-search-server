#pragma once

#include <iostream>

using namespace std;

// Один объект типа IteratorRange представляет собой одну страницу
template <typename Iterator>
class IteratorRange {
public:
    IteratorRange(Iterator begin, Iterator end)
        : first_(begin),
        last_(end),
        page_size_(distance(first_, last_)) {
    }

    Iterator begin() const { return first_; }

    Iterator end() const { return last_; }

    size_t size() const { return page_size_; }

private:
    Iterator first_;
    Iterator last_;
    size_t page_size_;
};

template <typename Iterator>
ostream& operator<<(ostream& out, const IteratorRange<Iterator> page) {
    for (auto it = page.begin(); it != page.end(); ++it) {
        cout << *it;
    }
    return out;
}

// Класс для вывода результатов страницами
template <typename Iterator>
class Paginator {
public:
    // делать ли explicit
    // Конструктор разделяет изначальный вектор типа Document на части
    // каждая часть состоит из page_size кол-ва и представляет собой объект типа IteratorRange
    Paginator(Iterator begin, Iterator end, size_t page_size) {
        for (size_t left = distance(begin, end); left > 0;) {
            const size_t current_page_size = min(page_size, left);
            const Iterator current_page_end = next(begin, current_page_size);
            pages_.push_back({ begin, current_page_end });

            left -= current_page_size;
            begin = current_page_end;
        }
    }

    auto begin() const { return pages_.begin(); }

    auto end() const { return pages_.end(); }

    size_t size() const { return pages_.size(); }
private:
    vector<IteratorRange<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}
