#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <chrono>
#include <omp.h>
#include <set>
#include "heap_map.hpp"
#include "linked_array.hpp"

using namespace std;

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
            freqs[pair].occurences.insert(i);
        }
    }
    inline void dec_pair(Pair pair, size_t i) {
        if (!freqs.contains(pair)) {
            throw out_of_range("Pair not found, cannot decrement");
        } else {
            freqs[pair].occurences.erase(i);
            if (freqs[pair].occurences.empty()) {
                freqs.erase(pair);
            }
        }
    }
public:
    //unordered_map<Pair, size_t, PairHash> freqs;
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
        tokens_arr = LinkedArray<size_t>(tokens);

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
        tokens_arr = LinkedArray<size_t>(tokens);

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
#ifdef PARALLEL
        os << "Parallel -- ";
#endif
        os << "Token Length: " << bpe.tokens.size() << " Unique tokens: " << bpe.grammar.size() << " Highest freq: "
            << bpe.highest_freq << " (" << bpe.most_freq_pair.l << ", " << bpe.most_freq_pair.r << ") Freq table size: " << bpe.freqs.size() << endl;
        chrono::duration<double> elapsed_seconds = now - bpe.start;

        cout << "elapsed time: " << elapsed_seconds.count() << "s\n";
#if 0
        os << '[';
        for (int i = 0; i < bpe.tokens.size(); i++) {
            os << bpe.tokens[i];
            if (i < bpe.tokens.size() - 1) {
                os << ", ";
            }
        }
        os << ']' << endl;
        os << "Frequencies:" << endl << "{ ";
        for (const auto& [key, val] : bpe.freqs) {
            os << '(' << key.l << ", " << key.r << "): " << val << ", ";
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
        // find most frequent:
        //update_freq();
        pair<Pair, PairOccurences> most = freqs.max();
        most_freq_pair = most.first;
        highest_freq = most.second.occurences.size();

        if (highest_freq <= 1) return; // not compressable
        grammar.push_back(most_freq_pair); // introduce a new token

        for (size_t occurence : most.second.occurences) {
            if (tokens_arr[occurence] >= grammar.size()) {
                cout << "Attempted to read token " << tokens_arr[occurence] << " but that should be impossible." << endl;
                throw runtime_error("Token is impossible");
            }

            // decrease old left (ab) if a exists
            size_t prev_index = tokens_arr.get_prev_index(occurence);
            Pair lpair = {tokens_arr[prev_index],most_freq_pair.r};
            dec_pair(lpair, prev_index);

            // decrease old right (cd) if d exists
            size_t next_next_index = tokens_arr.get_second_next_index(occurence);
            Pair rpair = {most_freq_pair.r,tokens_arr[next_next_index]};
            dec_pair(rpair, next_next_index);


            // increase new left (aZ) but only if there was already a token in tokens_out
            lpair = {tokens_arr[prev_index], grammar.size() - 1};
            inc_pair(lpair, prev_index);

            // increase new right (Zd) but only if there is a future token in input
            rpair = {grammar.size() - 1, tokens_arr[next_next_index]};
            inc_pair(rpair, next_next_index);

            // finally perform the replacement
            tokens_arr.replace_pair(occurence, grammar.size() - 1);
            }
        }

#if 0
    void serialize(const string& filename) const {
        ofstream ofs(filename, ios::binary);
        if (!ofs) {
            cerr << "Error opening file for writing: " << filename << endl;
            return;
        }

        // Write the size of tokens
        size_t tokenSize = tokens.size();
        ofs.write(reinterpret_cast<const char*>(&tokenSize), sizeof(tokenSize));

        // Write the tokens
        ofs.write(reinterpret_cast<const char*>(tokens.data()), tokenSize * sizeof(size_t));

        // Write the size of the unordered_map
        size_t mapSize = freqs.size();
        ofs.write(reinterpret_cast<const char*>(&mapSize), sizeof(mapSize));

        // Write the contents of the unordered_map
        for (const auto& [key, val] : freqs) {
            ofs.write(reinterpret_cast<const char*>(&key.l), sizeof(key.l));
            ofs.write(reinterpret_cast<const char*>(&key.r), sizeof(key.r));
            ofs.write(reinterpret_cast<const char*>(&val), sizeof(val));
        }

        // Write most frequent pair and its frequency
        ofs.write(reinterpret_cast<const char*>(&most_freq_pair.l), sizeof(most_freq_pair.l));
        ofs.write(reinterpret_cast<const char*>(&most_freq_pair.r), sizeof(most_freq_pair.r));
        ofs.write(reinterpret_cast<const char*>(&highest_freq), sizeof(highest_freq));

        // Write the size of the buffer
        size_t bufferSize = buffer.size();
        ofs.write(reinterpret_cast<const char*>(&bufferSize), sizeof(bufferSize));
        ofs.write(reinterpret_cast<const char*>(buffer.data()), bufferSize * sizeof(size_t));

        // Write iterations
        ofs.write(reinterpret_cast<const char*>(&iterations), sizeof(iterations));

        ofs.close();
    }

    void deserialize(const string& filename) {
        ifstream ifs(filename, ios::binary);
        if (!ifs) {
            cerr << "Error opening file for reading: " << filename << endl;
            return;
        }

        // Read the size of tokens
        size_t tokenSize;
        ifs.read(reinterpret_cast<char*>(&tokenSize), sizeof(tokenSize));
        tokens.resize(tokenSize);

        // Read the tokens
        ifs.read(reinterpret_cast<char*>(tokens.data()), tokenSize * sizeof(size_t));

        // Read the size of the unordered_map
        size_t mapSize;
        ifs.read(reinterpret_cast<char*>(&mapSize), sizeof(mapSize));

        // Clear the existing map and read the contents
        freqs.clear();
        for (size_t i = 0; i < mapSize; ++i) {
            Pair key;
            size_t val;
            ifs.read(reinterpret_cast<char*>(&key.l), sizeof(key.l));
            ifs.read(reinterpret_cast<char*>(&key.r), sizeof(key.r));
            ifs.read(reinterpret_cast<char*>(&val), sizeof(val));
            freqs[key] = val;
        }

        // Read most frequent pair and its frequency
        ifs.read(reinterpret_cast<char*>(&most_freq_pair.l), sizeof(most_freq_pair.l));
        ifs.read(reinterpret_cast<char*>(&most_freq_pair.r), sizeof(most_freq_pair.r));
        ifs.read(reinterpret_cast<char*>(&highest_freq), sizeof(highest_freq));

        // Read the size of the buffer
        size_t bufferSize;
        ifs.read(reinterpret_cast<char*>(&bufferSize), sizeof(bufferSize));
        buffer.resize(bufferSize);
        ifs.read(reinterpret_cast<char*>(buffer.data()), bufferSize * sizeof(size_t));

        // Read iterations
        ifs.read(reinterpret_cast<char*>(&iterations), sizeof(iterations));

        ifs.close();
    }
#endif

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
    vector<char> shakie = readFileToBytes("./quixote.txt");
    cout << "done." << endl;
    BPE_Encoding e(shakie);
    e.reduce();
    cout << e;
    while (e.highest_freq > 1) {
        e.reduce();
        if (e.iterations % 1000 == 0)
            cout << e;
    }

    // e.serialize("shakie.bpe");

    return 0;
}
