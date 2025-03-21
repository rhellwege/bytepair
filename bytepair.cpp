#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <chrono>
#include <unordered_set>
//#include <set>
#include <assert.h>
#include "heap_map.hpp"
#include "linked_array.hpp"

using namespace std;

//#define VERBOSE
#define PRINT_EVERY 1000

typedef uint32_t Token;

struct Pair {
    Token l;
    Token r;

    bool operator==(const Pair& b) const {
        return (l == b.l && r == b.r);
    }
};

struct PairHash {
    size_t operator()(const Pair& p) const {
        return hash<Token>()(p.l) ^ (hash<Token>()(p.r) << 1);
    }
};

struct PairOccurrences {
    Pair pair;
    unordered_set<Token> occurrences; // compare based on the size of the set

    friend ostream& operator<<(ostream& os, const PairOccurrences& p) {
        os << "(" << p.pair.l << ", " << p.pair.r << ") -> [";
        for (auto i : p.occurrences) {
            os << i << " ";
        }
        os << "]";
        return os;
    }
};

size_t heapKeyFunc(const pair<Pair, PairOccurrences>& p) {
    return p.second.occurrences.size();
}

class BPE_Encoding {
private:
    inline void inc_pair(Pair pair, Token i) {
        if (!freqs.contains(pair)) {
            freqs.push(pair, PairOccurrences{pair, {i}});
        } else {
            freqs.update(pair, [&](PairOccurrences& po) {
                po.occurrences.insert(i);
            });
        }
    }
    inline void dec_pair(Pair pair, Token i) {
        if (!freqs.contains(pair)) {
            throw out_of_range("Pair not found, cannot decrement");
        } else {
            freqs.update(pair, [&](PairOccurrences& po) {
                po.occurrences.erase(i);
            });
            if (freqs.view(pair).occurrences.empty()) {
                freqs.erase(pair);
            }
        }
    }
public:
    HeapMap<Pair, PairOccurrences, PairHash, function<size_t(const pair<Pair, PairOccurrences>&)>> freqs;
    //FibHeapMap<Pair, PairOccurrences, PairHash, function<size_t(const PairOccurrences&)>> freqs;
    Pair most_freq_pair;
    size_t highest_freq;
    LinkedArray<Token> tokens_arr;
    vector<Pair> grammar;
    size_t iterations;
    chrono::time_point<std::chrono::system_clock> start;
    chrono::time_point<std::chrono::system_clock> since_last_print;

    BPE_Encoding(const string& input) : freqs([](const pair<Pair, PairOccurrences>& p) { return p.second.occurrences.size(); }) {
        since_last_print = chrono::system_clock::now();
        start = chrono::system_clock::now();
        vector<Token> tokens;
        iterations = 0;
        highest_freq = 0;
        for (const unsigned char& c : input) {
            tokens.push_back((size_t)c);
        }

        // construct the linked array of tokens
        tokens_arr.fill(tokens);

        for (Token i = 0; i < 256; i++) {
            grammar.push_back(Pair{i, 0});
        }

        for (size_t i = 0; i < tokens.size(); i++) {
            if (i < tokens.size() - 1) {
                Pair pair = Pair{tokens[i], tokens[i+1]};
                inc_pair(pair, i);
            }
        }
    }

    BPE_Encoding(const vector<char>& input) : freqs([](const pair<Pair, PairOccurrences>& p) { return p.second.occurrences.size(); }) {
        since_last_print = chrono::system_clock::now();
        start = chrono::system_clock::now();
        vector<Token> tokens;
        iterations = 0;
        highest_freq = 0;
        for (const unsigned char& c : input) {
            tokens.push_back((size_t)c);
        }

        // construct the linked array of tokens
        tokens_arr.fill(tokens);

        for (Token i = 0; i < 256; i++) {
            grammar.push_back(Pair{i, 0});
        }

        for (size_t i = 0; i < tokens.size(); i++) {
            if (i < tokens.size() - 1) {
                Pair pair = Pair{tokens[i], tokens[i+1]};
                inc_pair(pair, i);
            }
        }
    }

    friend ostream& serialize(ostream& os, BPE_Encoding& bpe) {
        os << "bpe";
        os.write(reinterpret_cast<char*>(bpe.tokens_arr.size()), sizeof(bpe.tokens_arr.size()));
        for (const auto& tok : bpe.tokens_arr) {
            os.write(reinterpret_cast<char*>(tok), sizeof(tok));
        }
        return os;
    }

    friend ostream& operator << (ostream& os, BPE_Encoding& bpe) {
        chrono::time_point<chrono::system_clock> now = chrono::system_clock::now();
        os << "-------------------------------------------------------------------------------------------------------" << endl;
        os << "Token Length: " << bpe.tokens_arr.size() << " Unique tokens: " << bpe.grammar.size() << " Highest freq: "
            << bpe.highest_freq << " (" << bpe.most_freq_pair.l << ", " << bpe.most_freq_pair.r << ") Freq table size: " << bpe.freqs.size() << endl;
        chrono::duration<double> elapsed_seconds = now - bpe.since_last_print;

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
        if (bpe.highest_freq == 1) {
            now = chrono::system_clock::now();
            elapsed_seconds = now - bpe.start;
            os << "Total elapsed time: " << elapsed_seconds.count() << "s\n";
        }
        bpe.since_last_print = chrono::system_clock::now();
        return os;
    }

    // only one iteration
    void reduce() {
        iterations += 1;
        pair<Pair, PairOccurrences> most = freqs.max();
        most_freq_pair = most.first;
        highest_freq = most.second.occurrences.size();

        if (highest_freq <= 1 && iterations > 1) return; // not compressable
        grammar.push_back(most_freq_pair); // introduce a new token

        while (freqs.contains(most_freq_pair) && freqs.view(most_freq_pair).occurrences.size() > 0) {
            size_t occurence = *freqs.view(most_freq_pair).occurrences.begin();
            if (tokens_arr.get_raw(occurence) == nullptr) continue;
            Node<Token>* raw = tokens_arr.get_raw(occurence);
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
                Pair lpair = {raw->prev->data, (Token)(grammar.size() - 1)};
                inc_pair(lpair, raw->prev->index);
            }

            // increase new right (Zd) but only if there is a future token in input
            if (raw->next != nullptr) {
                Pair rpair = {(Token)(grammar.size() - 1), raw->next->data};
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

    void serialize(const string& fname) {
        ofstream file(fname, ios::binary); // Open file in binary mode
        if (!file) {
            cerr << "Error opening file: " << fname << endl;
            return;
        }

        file.write("bpe0.01", 7);
        file.write(reinterpret_cast<const char*>(&iterations), sizeof(iterations)); // not needed
        size_t size = tokens_arr.size();
        file.write(reinterpret_cast<const char*>(&size), sizeof(size));
        for (const auto& token : tokens_arr) {
            file.write(reinterpret_cast<const char*>(&token), sizeof(token)); // not needed
        }
        size = grammar.size();
        file.write(reinterpret_cast<const char*>(&size), sizeof(size));
        for (const auto& pair : grammar) {
            file.write(reinterpret_cast<const char*>(&pair.l), sizeof(pair.l));
            file.write(reinterpret_cast<const char*>(&pair.r), sizeof(pair.r));
        }
        file.close();
    }

    void deserialize(const string& fname) {
        ifstream file(fname, ios::binary); // Open file in binary mode
        size_t grammar_size;
        size_t tokens_arr_size;
        string version;

        if (!file) {
            cerr << "Error opening file: " << fname << endl;
            return;
        }

        file >> version;
        if (version != "bpe0.01") {
            cerr << "Unsupported version: " << version << endl;
            return;
        }

        file >> iterations;
        file >> tokens_arr_size;

        //tokens_arr.clear();
        assert(tokens_arr.size() == 0);
        for (size_t i = 0; i < tokens_arr_size; ++i) {
            size_t token;
            file >> token;
            //tokens_arr.push(token);
        }

        file >> grammar_size;
        grammar.clear();
        for (Token i = 0; i < grammar.size(); ++i) {
            Token l, r;
            file >> l >> r;
            grammar.push_back({l, r});
        }
        file.close();
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
    cout << e;
    // cout << "Serializing to shakie.bpe..." << endl;
    // e.serialize("shakie.bpe");
    // cout << "Serialization complete." << endl;
    return 0;
}
