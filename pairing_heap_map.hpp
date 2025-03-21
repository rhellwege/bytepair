#include <functional>
#include <stdexcept>
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

        // To maintain the max heap condition compare
        // the nodes and node with minimum value become
        // parent of the other node
        if(keyFunc_(a->key) > keyFunc_(b->key)) {
            a->addChild(b);
            return a;
        }
        else {
            b->addChild(a);
            return b;
        }

        return nullptr; // Unreachable
    }
    // This method is used when we want to delete root node
    PairingHeapNode<K,V> *twopassmerge_(PairingHeapNode<K,V> *node) {
        if(node == nullptr || node->nextSibling == nullptr)
            return node;
        else {
            PairingHeapNode<K,V> *a, *b, *newNode;
            a = node;
            b = node->nextSibling;
            newNode = node->nextSibling->nextSibling;

            a->nextSibling = nullptr;
            b->nextSibling = nullptr;

            return merge_(merge_(a, b), twopassmerge_(newNode));
        }

        return nullptr; // Unreachable
    }

    // Function to delete the root node in heap
    PairingHeapNode<K,V> *remove_(PairingHeapNode<K,V> *node) {
        return twopassmerge_(node->leftChild);
    }
public:
    PairingHeapMap(HeapKeyFunc keyFunc) : keyFunc_(keyFunc) {};
    ~PairingHeapMap() = default;

    void push(const K& key, const V& value) {
        merge_(root, new PairingHeapNode<K, V>(make_pair(key, value), nullptr, nullptr));
    }

    void update(const K& key, const function<void(V&)>& updateFunc) {
    }

    const V& view(const K& key) {
    }

    pair<K, V> pop() {
        pair<K,V> top = max();
        remove_(root);
        return top;
    }

    const pair<K, V>& max() const {
        if (root == nullptr) throw runtime_error("Cannot view the top of empty");
        return root->key;
    }

    bool contains(K key) const {
    }

    size_t size() const {
    }

    pair<K, V> erase(K map_key) {
    }
};
