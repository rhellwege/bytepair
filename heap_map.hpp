#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <functional>

using namespace std;

template <typename K, typename V, typename Hasher, typename HeapKeyFunc>
class HeapMap {
public:
    HeapMap(HeapKeyFunc keyFunc) : keyFunc_(keyFunc) {};
    ~HeapMap() = default;

    void push(const K& key, const V& value) {
        heap_.push_back({key, value});
        if (map_.find(key) != map_.end()) throw runtime_error("Key already exists");
        map_[key] = heap_.size() - 1;
        _heapify_up(heap_.size() - 1);
    }

    void update(const K& key, const function<void(V&)>& updateFunc) {
        if (map_.find(key) == map_.end()) throw runtime_error("Key does not exist");
        size_t heap_i = map_[key];
        updateFunc(heap_[heap_i].second);
        _heapify_up(heap_i);
        _heapify_down(heap_i);
    }

    const V& view(const K& key) {
        if (map_.find(key) == map_.end()) throw runtime_error("Key does not exist");
        return heap_[map_[key]].second;
    }

    pair<K, V> pop() {
        if (heap_.size() == 0) throw runtime_error("The heap is empty.");
        _swap(0, heap_.size() - 1);
        pair<K, V> item = heap_.back();
        heap_.pop_back();
        map_.erase(item.first);
        _heapify_down(0);
        return item;
    }

    const pair<K, V>& max() const {
        if (heap_.empty()) throw runtime_error("The heap is empty.");
        return heap_[0];
    }

    bool contains(K key) const {
        return map_.find(key) != map_.end();
    }

    size_t size() const {
        if (map_.size() != heap_.size()) throw runtime_error("Heap and map sizes mismatch");
        return heap_.size();
    }

    pair<K, V> erase(K map_key) {
        if (map_.find(map_key) == map_.end()) throw runtime_error("Invalid map key");
        size_t heap_i = map_[map_key];
        pair<K, V> item = heap_[heap_i];
        _swap(heap_i, heap_.size() - 1);
        heap_.pop_back();
        map_.erase(map_key);

        if (heap_i < heap_.size()) {
            _heapify_down(heap_i);
            _heapify_up(heap_i);
        }

        return item;
    }
private:
    HeapKeyFunc keyFunc_;
    unordered_map<K, size_t, Hasher> map_; // keeps track of indices in heap
    vector<pair<K, V>> heap_;

    void _swap(size_t i, size_t j) {
        map_[heap_[i].first] = j;
        map_[heap_[j].first] = i;
        pair<K, V> temp = heap_[i];
        heap_[i] = heap_[j];
        heap_[j] = temp;
    }

    inline size_t _right_child(size_t i) {
        return 2 * i + 2;
    }

    inline size_t _left_child(size_t i) {
        return 2 * i + 1;
    }

    inline size_t _parent(size_t i) {
        return (i - 1) / 2;
    }

    void _heapify_down(size_t i) {
        size_t largest = i;
        size_t left = _left_child(i);
        size_t right = _right_child(i);
        if (left < heap_.size() && keyFunc_(heap_[left]) > keyFunc_(heap_[largest])) {
            largest = left;
        }
        if (right < heap_.size() && keyFunc_(heap_[right]) > keyFunc_(heap_[largest])) {
            largest = right;
        }
        if (largest != i) {
            _swap(i, largest);
            _heapify_down(largest);
        }
    }

    void _heapify_up(size_t i) {
        while (i > 0 && keyFunc_(heap_[i]) > keyFunc_(heap_[_parent(i)])) {
            _swap(i, _parent(i));
            i = _parent(i);
        }
    }

public:
    class ConstIterator {
    public:
        ConstIterator(const HeapMap& heapMap, size_t index) : heapMap_(heapMap), index_(index) {}

        const pair<K, V>& operator*() const {
            return heapMap_.heap_[index_];
        }

        const pair<K, V>* operator->() const {
            return &heapMap_.heap_[index_];
        }

        ConstIterator& operator++() {
            ++index_;
            return *this;
        }

        ConstIterator operator++(int) {
            ConstIterator temp = *this;
            ++index_;
            return temp;
        }

        bool operator==(const ConstIterator& other) const {
            return index_ == other.index_;
        }

        bool operator!=(const ConstIterator& other) const {
            return index_ != other.index_;
        }

    private:
        const HeapMap& heapMap_;
        size_t index_;
    };

    ConstIterator begin() const {
        return ConstIterator(*this, 0);
    }

    ConstIterator end() const {
        return ConstIterator(*this, heap_.size());
    }
};
