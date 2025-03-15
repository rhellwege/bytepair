#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <chrono>
#include <omp.h>
#include <set>
#include <assert.h>
#include "heap_map.hpp"
#include "linked_array.hpp"

using namespace std;

//#define VERBOSE
#define PRINT_EVERY 1000

struct Pair {
    size_t l;
    size_t r;

    bool operator==(const Pair& b) const {
        return (l == b.l && r == b.r);
    }
};

struct PairHash {
    size_t operator()(const Pair& p) const {
        return hash<size_t>()(p.l) ^ hash<size_t>()(p.r);
    }
};

struct PairOccurences {
    Pair pair;
    set<size_t> occurences;

    friend ostream& operator<<(ostream& os, const PairOccurences& p) {
        os << "(" << p.pair.l << ", " << p.pair.r << ") -> [";
        for (auto i : p.occurences) {
            os << i << " ";
        }
        os << "]";
        return os;
    }
};

size_t heapKeyFunc(const PairOccurences& p) {
    return p.occurences.size();
}

#ifdef PARALLEL
// A struct to track the current maximum pair and its frequency.
struct MaxData {
    Pair key;
    size_t frequency;
};

// Custom reduction: We want to pick the object with the higher frequency.
#pragma omp declare reduction(maximum : MaxData :                      \
    omp_out = (omp_in.frequency > omp_out.frequency ? omp_in : omp_out))    \
    initializer(omp_priv = { {0, 0}, 0 })
#endif



class BPE_Encoding {
private:
    inline void inc_pair(Pair pair, size_t i) {
        if (!freqs.contains(pair)) {
            freqs.push(pair, PairOccurences{pair, {i}});
        } else {
            freqs.update(pair, [&](PairOccurences& po) {
                po.occurences.insert(i);
            });
        }
    }
    inline void dec_pair(Pair pair, size_t i) {
        if (!freqs.contains(pair)) {
            throw out_of_range("Pair not found, cannot decrement");
        } else {
            freqs.update(pair, [&](PairOccurences& po) {
                po.occurences.erase(i);
            });
            if (freqs.view(pair).occurences.empty()) {
                freqs.erase(pair);
            }
        }
    }
public:
    HeapMap<Pair, PairOccurences, PairHash, function<size_t(const PairOccurences&)>> freqs;
    Pair most_freq_pair;
    size_t highest_freq;
    LinkedArray<size_t> tokens_arr;
    vector<size_t> buffer;
    vector<Pair> grammar;
    size_t iterations;
    chrono::time_point<std::chrono::system_clock> start;

    BPE_Encoding(const string& input) : freqs([](const PairOccurences& p) { return p.occurences.size(); }) {
        start = (chrono::system_clock::now());
        vector<size_t> tokens;
        iterations = 0;
        highest_freq = 0;
        for (const unsigned char& c : input) {
            tokens.push_back((size_t)c);
        }

        // construct the linked array of tokens
        tokens_arr.fill(tokens);

        for (size_t i = 0; i < 256; i++) {
            grammar.push_back(Pair{i, 0});
        }

        for (size_t i = 0; i < tokens.size(); i++) {
            if (i < tokens.size() - 1) {
                Pair pair = Pair{tokens[i], tokens[i+1]};
                inc_pair(pair, i);
            }
        }
    }

    BPE_Encoding(const vector<char>& input) : freqs([](const PairOccurences& p) { return p.occurences.size(); }) {
        start = (chrono::system_clock::now());
        vector<size_t> tokens;
        iterations = 0;
        highest_freq = 0;
        for (const unsigned char& c : input) {
            tokens.push_back((size_t)c);
        }

        // construct the linked array of tokens
        tokens_arr.fill(tokens);

        for (size_t i = 0; i < 256; i++) {
            grammar.push_back(Pair{i, 0});
        }

        for (size_t i = 0; i < tokens.size(); i++) {
            if (i < tokens.size() - 1) {
                Pair pair = Pair{tokens[i], tokens[i+1]};
                inc_pair(pair, i);
            }
        }
    }

    friend ostream& operator << (ostream& os, BPE_Encoding& bpe) {
        chrono::time_point<chrono::system_clock> now = chrono::system_clock::now();
        os << "-------------------------------------------------------------------------------------------------------" << endl;
        os << "Token Length: " << bpe.tokens_arr.size() << " Unique tokens: " << bpe.grammar.size() << " Highest freq: "
            << bpe.highest_freq << " (" << bpe.most_freq_pair.l << ", " << bpe.most_freq_pair.r << ") Freq table size: " << bpe.freqs.size() << endl;
        chrono::duration<double> elapsed_seconds = now - bpe.start;

        cout << "elapsed time: " << elapsed_seconds.count() << "s\n";
        cout << "iteration: " << bpe.iterations << "\n";
#ifdef VERBOSE
        os << '[';
        for (const auto& tok : bpe.tokens_arr) {
            os << tok << ", ";
        }
        os << ']' << endl;
        os << "Frequencies:" << endl << "{ ";
        for (const auto& po : bpe.freqs) {
            os << po.second << endl;
        }

        os << "}" << endl;
#endif

        os << endl;
        bpe.start = chrono::system_clock::now();
        return os;
    }

    // only one iteration
    void reduce() {
        iterations += 1;
        pair<Pair, PairOccurences> most = freqs.max();
        most_freq_pair = most.first;
        highest_freq = most.second.occurences.size();

        if (highest_freq <= 1 && iterations > 1) return; // not compressable
        grammar.push_back(most_freq_pair); // introduce a new token

        while (freqs.contains(most_freq_pair) && freqs.view(most_freq_pair).occurences.size() > 0) {
            size_t occurence = *freqs.view(most_freq_pair).occurences.begin();
            if (tokens_arr.get_raw(occurence) == nullptr) continue; // TODO: Am I correct?
            try {
                if (tokens_arr[occurence] >= grammar.size()) {
                    cout << "Attempted to read token " << tokens_arr[occurence] << " but that should be impossible." << endl;
                    throw runtime_error("Token is impossible");
                }
            } catch (const out_of_range& e) {
                cout << "occurence: " << occurence << endl;
                cout << "Attempted to read token " << tokens_arr[occurence] << " but that should be impossible." << endl;
                throw runtime_error("Token is impossible");
            }
            Node<size_t>* raw = tokens_arr.get_raw(occurence);
            assert(raw != nullptr);

            // if previous exists, decrease old left (ab) if a exists
            if (raw->prev != nullptr) {
                Pair lpair = {raw->prev->data,most_freq_pair.l};
                dec_pair(lpair, raw->prev->index);
            }

            // decrease old right (cd) if d exists
            if (raw->next != nullptr && raw->next->next != nullptr) {
                Pair rpair = {most_freq_pair.r,raw->next->next->data};
                dec_pair(rpair, raw->next->index);
            }

            // dont forget to decrease THIS occurence:
            dec_pair(most_freq_pair, occurence);

            // finally perform the replacement
            tokens_arr.replace_pair(occurence, grammar.size() - 1);
            raw = tokens_arr.get_raw(occurence);

            // increase new left (aZ) but only if there was already a token in tokens_out
            if (raw->prev != nullptr) {
                Pair lpair = {raw->prev->data, grammar.size() - 1};
                inc_pair(lpair, raw->prev->index);
            }

            // increase new right (Zd) but only if there is a future token in input
            if (raw->next != nullptr) {
                Pair rpair = {grammar.size() - 1, raw->next->data};
                inc_pair(rpair, occurence);
            }

        }
    }

    void compress() {
        reduce();
        while (highest_freq >= 1) {
            reduce();
        }
    }
};

vector<char> readFileToBytes(const string& filename) {
    ifstream file(filename, ios::binary); // Open file in binary mode
    if (!file) {
        cerr << "Error opening file: " << filename << endl;
        return {};
    }

    // Get the size of the file
    file.seekg(0, ios::end); // Move to the end of the file
    streamsize size = file.tellg(); // Get the current position (size of the file)
    file.seekg(0, ios::beg); // Move back to the beginning of the file

    vector<char> buffer(size); // Create a buffer to hold the bytes
    if (!file.read(buffer.data(), size)) { // Read the file into the buffer
        cerr << "Error reading file: " << filename << endl;
        return {};
    }

    return buffer; // Return the buffer containing the bytes
}

int main() {
    cout << "Loading shakespear..." << endl;
    vector<char> shakie = readFileToBytes("./shakespear.txt");
    cout << "done." << endl;
    BPE_Encoding e(shakie);
    cout << e;
    e.reduce();
    cout << e;
    while (e.highest_freq > 1) {
        e.reduce();
        if (e.iterations % PRINT_EVERY == 0)
            cout << e;
    }

    return 0;
}
