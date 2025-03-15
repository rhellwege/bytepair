#include <iostream>
#include <vector>

using namespace std;

template <typename T>
struct Node {
    Node* next;
    Node* prev;
    size_t index;
    T data;
};

template <typename T>
class LinkedArray {
private:
    vector<Node<T>*> nodes;

public:
    size_t length;
    void fill(const vector<T>& data) {
        Node<T>* prev = nullptr;
        for (size_t i = 0; i < data.size(); ++i) {
            T newdata = data[i];
            Node<T>* newNode = new Node<T>{nullptr, prev, i, newdata};
            if (prev) prev->next = newNode;
            nodes.push_back(newNode);
            prev = newNode;
        }
        length = data.size();
    }
    LinkedArray() {}
    ~LinkedArray() {
        cout << "LinkedArray destructor called " << nodes.size()<< endl;
        for (auto node : nodes) {
            if (node != nullptr) delete node;
        }
    }

    T get_by_index(size_t index) const {
        if (index >= nodes.size()) throw out_of_range("Index out of range");
        if (nodes[index] == nullptr) {
            cout << "Node at index " << index << " is null" << endl;
            throw out_of_range("Node is null");
        }
        return nodes[index]->data;
    }

    Node<T>* get_raw(size_t index) {
        if (index >= nodes.size()) throw out_of_range("Index out of range");
        return nodes[index];
    }

    T operator[](size_t index) const {
        return get_by_index(index);
    }

    size_t get_prev_index(size_t index) const {
        if (index == 0) throw out_of_range("No previous index");
        return nodes[index]->prev->index;
    }

    size_t get_next_index(size_t index) const {
        if (index == nodes.size() - 1) throw out_of_range("No next index");
        return nodes[index]->next->index;
    }

    size_t get_second_next_index(size_t index) const {
        if (index == nodes.size() - 2) throw out_of_range("No next next index");
        return nodes[index]->next->next->index;
    }

    size_t size() const {
        return length;
    }

    size_t capacity() const {
        return nodes.size();
    }

    void replace_pair(size_t index, T new_item) {
        if (index > nodes.size() - 2 || nodes[index] == nullptr || nodes[index]->next == nullptr) {
            throw out_of_range("Index out of range");
        }
        nodes[index]->data = new_item;
        nodes[nodes[index]->next->index] = nullptr;
        nodes[index]->next = nodes[index]->next->next;
        if (nodes[index]->next != nullptr) {
            nodes[index]->next->prev = nodes[index];
        }
        length--;
    }

    class Iterator {
    public:
        Iterator(Node<T>* node) : current(node) {}

        Iterator& operator++() {
            if (current == nullptr) throw out_of_range("Iterator out of range");
            current = current->next;
            return *this;
        }

        Iterator operator++(int) {
            Iterator temp = *this;
            ++(*this);
            return temp;
        }

        T& operator*() {
            if (current == nullptr) throw out_of_range("Iterator out of range");
            return current->data;
        }

        bool operator==(const Iterator& other) const {
            return current == other.current;
        }

        bool operator!=(const Iterator& other) const {
            return !(*this == other);
        }

    private:
        Node<T>* current;
    };

    Iterator begin() {
        return Iterator(nodes.front());
    }

    Iterator end() {
        return Iterator(nullptr);
    }
};
