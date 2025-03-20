#include <functional>
#include <unordered_map>

using namespace std;

template<typename K, typename V>
struct PairingHeapNode {
    pair<K, V> data;
    PairingHeapNode* leftChild;
    PairingHeapNode* nextSibling;

    PairingHeapNode():
            leftChild(nullptr), nextSibling(nullptr) {}

    // creates a new node
    PairingHeapNode(pair<K, V> data_, PairingHeapNode *leftChild_, PairingHeapNode *nextSibling_):
        data(data_), leftChild(leftChild_), nextSibling(nextSibling_) {}

    // Adds a child and sibling to the node
    void addChild(PairingHeapNode *node) {
        if(leftChild == nullptr)
            leftChild = node;
        else {
            node->nextSibling = leftChild;
            leftChild = node;
        }
    }
};

template <typename K, typename V, typename Hasher, typename HeapKeyFunc>
class PairingHeapMap {
private:
    unordered_map<K, V> map_;
    HeapKeyFunc keyFunc_;
    PairingHeapNode<K, V>* root;

    PairingHeapNode<K, V>* merge_(PairingHeapNode<K,V>* a, PairingHeapNode<K,V>* b) {
        // If any of the two-nodes is null
        // the return the not null node
        if(a == nullptr) return b;
        if(b == nullptr) return a;

        // To maintain the min heap condition compare
        // the nodes and node with minimum value become
        // parent of the other node
        if(a->key < b->key) {
            a->addChild(b);
            return a;
        }
        else {
            b->addChild(a);
            return b;
        }

        return nullptr; // Unreachable
    }
public:
    PairingHeapMap(HeapKeyFunc keyFunc) : keyFunc_(keyFunc) {};
    ~PairingHeapMap() = default;

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
