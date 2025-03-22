#include <functional>
#include <sys/types.h>
#include <unordered_map>
#include <cstdint>
#include <vector>

using namespace std;

template <typename Data>
struct FibHeapNode {
    Data data;
    FibHeapNode<Data>* parent;
    FibHeapNode<Data>* child;
    FibHeapNode<Data>* left;
    FibHeapNode<Data>* right;
    uint32_t degree;
    bool marked;

    FibHeapNode(const Data& data) : data(data), parent(nullptr), child(nullptr), left(this), right(this), degree(0), marked(false) {}
    void add_sibling(FibHeapNode<Data>* node) {
        if (!node) return;
        // Update the pointers to add the sibling
        node->parent = this->parent; // inherit parent
        node->left = this->left;
        node->right = this;
        this->left->right = node;
        this->left = node;
    }

    void add_child(FibHeapNode<Data>* node) {
        if (!node) return;
        // Update the pointers to add the child
        if (this->child) {
            this->child->add_sibling(node);
        }
        else {
            this->child = node;
            node->parent = this;
        }
        this->degree++;
    }

};

template <typename K, typename V, typename Hasher, typename HeapKeyFunc>
class FibHeapMap {
private:
    unordered_map<K, V> map_;
    HeapKeyFunc keyFunc_;
    uint32_t max_degree_;
    FibHeapNode<pair<K,V>>* max_;
    vector<FibHeapNode<pair<K,V>>*> aux_arr_;

    FibHeapNode<pair<K,V>>* _max_sibling(FibHeapNode<pair<K,V>>* node) {
        FibHeapNode<pair<K,V>>* curr = node;
        uint32_t max_val = keyFunc_(curr->data);
        auto max = curr;
        do  {
            if (keyFunc_(curr->right->data) > max_val) {
                max_val = keyFunc_(curr->right->data);
                max = curr->right;
            }
            curr = curr->right;
        } while (curr != node);
        return max;
    }

    void _merge(FibHeapNode<pair<K,V>>* node1, FibHeapNode<pair<K,V>>* node2) {
        if (!node1 || !node2) return;
        if (keyFunc_(node1->data) < keyFunc_(node2->data)) swap(node1, node2);
        node2->add_child(node1); // this takes care of the degree
        max_degree_ = max(max_degree_, node2->degree);
    }

    // called at the end of pop (extract min)
    void _consolidate() {
        FibHeapNode<pair<K,V>>* curr = max_;
        if (!curr) return;
        do  {
            FibHeapNode<pair<K,V>>* next = curr->right;
            if (aux_arr_[curr->degree]) {
                FibHeapNode<pair<K,V>>* aux = aux_arr_[curr->degree];
                aux_arr_[curr->degree] = nullptr;
                _merge(aux, curr);
                curr = aux;
            } else {
                aux_arr_[curr->degree] = curr;
                curr = next;
            }
        } while (curr != max_);
    }

public:
    FibHeapMap(HeapKeyFunc keyFunc) : keyFunc_(keyFunc), max_degree_(0), max_(nullptr) {};
    ~FibHeapMap() = default;

    void push(const K& key, const V& value) {
        if (map_.find(key) != map_.end()) return; // do not add duplicates.
        FibHeapNode<pair<K,V>>* node = new FibHeapNode<pair<K,V>>(make_pair(key, value));
        map_[key] = node;
        if (max_) {
            max_->add_sibling(node);
            if (keyFunc_(node->data) > keyFunc_(max_->data)) max_ = node;
        }
        else {
            max_ = node;
        }
    }


    void update(const K& key, const function<void(V&)>& updateFunc) {
    }

    const V& view(const K& key) const {
    }

    pair<K, V> pop() {
    }

    const pair<K, V>& max() const {
        return max_->data;
    }

    bool contains(K key) const {
    }

    size_t size() const {
    }

    pair<K, V> erase(K map_key) {
    }
};
