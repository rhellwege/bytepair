#include <functional>
#include <unordered_map>

using namespace std;

template <typename K, typename V, typename Hasher, typename HeapKeyFunc>
class FibHeapMap {
private:
    unordered_map<K, V> map_;
    HeapKeyFunc keyFunc_;

public:
    FibHeapMap(HeapKeyFunc keyFunc) : keyFunc_(keyFunc) {};
    ~FibHeapMap() = default;

    void push(const K& key, const V& value) {
    }


    void update(const K& key, const function<void(V&)>& updateFunc) {
    }

    const V& view(const K& key) const {
    }

    pair<K, V> pop() {
    }

    const pair<K, V>& max() const {
    }

    bool contains(K key) const {
    }

    size_t size() const {
    }

    pair<K, V> erase(K map_key) {
    }
};
