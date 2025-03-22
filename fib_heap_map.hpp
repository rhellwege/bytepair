#include <functional>
#include <stdexcept>
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
        // take care of previous siblings
        node1->right->left = node1->left;
        node1->left->right = node1->right;
        node2->add_child(node1); // this takes care of the degree
        if (node2->degree > max_degree_) {
            max_degree_ = node2->degree;
            aux_arr_.push_back(nullptr);
        }
    }

    // called at the end of pop (extract min)
    void _consolidate() {
        FibHeapNode<pair<K,V>>* curr = max_;
        if (!curr) return;
        do {
            FibHeapNode<pair<K,V>>* next = curr->right;
            if (aux_arr_[curr->degree]) {
                FibHeapNode<pair<K,V>>* aux = aux_arr_[curr->degree];
                _merge(aux, curr);
                aux_arr_[curr->degree] = nullptr;
                curr = aux;
            } else {
                aux_arr_[curr->degree] = curr;
                curr = next;
            }
        } while (curr != max_);
    }

public:
    FibHeapMap(HeapKeyFunc keyFunc) : keyFunc_(keyFunc), max_degree_(0), max_(nullptr), aux_arr_({nullptr}) {};
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
        auto it = map_.find(key);
        if (it == map_.end()) throw std::runtime_error("Key not found");

        FibHeapNode<pair<K,V>>* node = it->second;
        uint32_t old_key_value = keyFunc_(node->data);

        // Call the user-provided function to modify the value
        updateFunc(node->data.second);

        // Get the new key value after the update
        uint32_t new_key_value = keyFunc_(node->data);

        // Check if the key has increased or decreased
        if (new_key_value < old_key_value) { // ?? here can we use the max of the children?
            // Remove from parent
            if (!node->parent->marked) {
                FibHeapNode<pair<K,V>>* parent = node->parent;
                if (parent->child == node) {
                    parent->child = (node->right == node) ? nullptr : node->right;
                }
                parent->degree--;
                parent->marked = true; // the parent lost a child

                // get rid of old sibling relation
                node->left->right = node->right;
                node->right->left = node->left;

                // Add to the root list
                max_->add_sibling(node);
                return;
            }
            auto curr = node->parent;
            do {
                if (curr->parent->child == node) {
                    curr->parent->child = (node->right == node) ? nullptr : node->right;
                }
                curr->parent->degree--;
                // get rid of old sibling relation
                curr->left->right = curr->right;
                curr->right->left = curr->left;

                // Add to the root list
                max_->add_sibling(curr);
                curr->marked = false;

                // Move to the next parent
                curr = curr->parent;
            } while(curr->parent && curr->parent->marked);
        } else if (new_key_value > old_key_value) {
            // Key has increased
            // We need to cut the node from its parent and add it to the root list
            if (!node->parent || node->parent && new_key_value < keyFunc_(node->parent->data)) return; // nothing needs to be done
            // Remove from parent
            if (!node->parent->marked) {
                FibHeapNode<pair<K,V>>* parent = node->parent;
                if (parent->child == node) {
                    parent->child = (node->right == node) ? nullptr : node->right;
                }
                parent->degree--;
                parent->marked = true; // the parent lost a child

                // get rid of old sibling relation
                node->left->right = node->right;
                node->right->left = node->left;

                // Add to the root list
                max_->add_sibling(node);
                return;
            }
            auto curr = node->parent;
            do {
                if (curr->parent->child == node) {
                    curr->parent->child = (node->right == node) ? nullptr : node->right;
                }
                curr->parent->degree--;
                // get rid of old sibling relation
                curr->left->right = curr->right;
                curr->right->left = curr->left;

                // Add to the root list
                max_->add_sibling(curr);
                curr->marked = false;

                // Move to the next parent
                curr = curr->parent;
            } while(curr->parent && curr->parent->marked);
        }
    }

    const V& view(const K& key) const {
        if (map_.find(key) == map_.end()) throw std::runtime_error("Key not found");
        return map_.at(key)->data.second;
    }

    pair<K, V> pop() {
        if (!max_) throw runtime_error("Heap is empty");
        auto ret = max_->data;
        erase(max_->data.first); // this is kind of lazy
        return ret;
    }

    const pair<K, V>& max() const {
        return max_->data;
    }

    bool contains(K key) const {
        return map_.find(key) != map_.end();
    }

    size_t size() const {
        return map_.size();
    }

    pair<K, V> erase(K map_key) {
        auto it = map_.find(map_key);
        if (it == map_.end()) throw std::runtime_error("Key not found");

        FibHeapNode<pair<K,V>>* node = it->second;

        // If the node is the max node, we need to update max_
        // Remove the node from its siblings
        node->left->right = node->right;
        node->right->left = node->left;

        // If the node has a parent, we need to remove it from the parent's child list
        if (node->parent) {
            node->parent->degree--;
            node->parent->marked = true; // the parent lost a child
            if (node->parent->child == node) {
                node->parent->child = (node->right == node) ? nullptr : node->right;
            }
        }
        // Add the children of the deleted node to the root list
        if (node->child) {
            // Connect the children to the root list
            FibHeapNode<pair<K,V>>* child = node->child;

            // Connect the last child to the root list
            FibHeapNode<pair<K,V>>* last_child = child->left; // last child in the child list

            // Connect the last child's right to the current max's left
            last_child->right = max_->left;
            max_->left->right = child; // Connect the first child to the current max's left
            child->left = last_child; // Connect the first child to the last child
            max_->left = last_child; // Update the max's left to the last child

            // Set parent pointers of children to nullptr
            child->parent = nullptr;
            for (FibHeapNode<pair<K,V>>* curr = child; curr != last_child; curr = curr->right) {
                curr->parent = nullptr; // Remove parent reference
            }
        }
        // Clean up the node
        map_.erase(it);
        delete node;

        _consolidate();
        // Return the erased data
        return make_pair(it); // Return the key and the value that was set to zero
    }
};
