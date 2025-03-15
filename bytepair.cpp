#include <iostream>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <chrono>
#include <omp.h>

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
public:
    unordered_map<Pair, size_t, PairHash> freqs;
    Pair most_freq_pair;
    size_t highest_freq;
    vector<size_t> buffer;
    vector<size_t> tokens;
    vector<Pair> grammar;
    size_t iterations;
    chrono::time_point<std::chrono::system_clock> start;

    BPE_Encoding(const string& input) {
        start = (chrono::system_clock::now());
        freqs = {};
        iterations = 0;
        highest_freq = 0;
        for (const unsigned char& c : input) {
            tokens.push_back((size_t)c);
        }

        for (size_t i = 0; i < 256; i++) {
            grammar.push_back(Pair{i, 0});
        }

        for (int i = 0; i < tokens.size(); i++) {
            if (i < tokens.size() - 1) {
                Pair pair = Pair{tokens[i], tokens[i+1]};
                freqs[pair] += 1;
            }
        }
    }

    BPE_Encoding(const vector<char>& input) {
        start = (chrono::system_clock::now());
        freqs = {};
        iterations = 0;
        highest_freq = 0;
        for (const unsigned char& c : input) {
            tokens.push_back((size_t)c);
        }

        for (size_t i = 0; i < 256; i++) {
            grammar.push_back(Pair{i, 0});
        }

        for (int i = 0; i < tokens.size(); i++) {
            if (i < tokens.size() - 1) {
                Pair pair = Pair{tokens[i], tokens[i+1]};
                freqs[pair] += 1;
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

    void update_freq() {
        highest_freq = 0;
#ifdef PARALLEL
        // Use a reduction to compute the per-thread maximum.
        // Note: If keys can be negative or zero, initialize accordingly.
        MaxData maxData = { {0, 0}, 0 };

        // Use the OpenMP parallel for with our custom reduction.
        #pragma omp parallel for reduction(maximum:maxData)
        for (size_t b = 0; b < freqs.bucket_count(); b++) {
            // Each thread uses a local result
            MaxData localData = { {0, 0}, 0 };

            // Iterate over each element in bucket 'b'
            for (auto it = freqs.begin(b); it != freqs.end(b); ++it) {
                if (it->second > localData.frequency) {
                    localData.key = it->first;
                    localData.frequency = it->second;
                }
            }

            // Combine the local value into the reduction variable.
            // The reduction clause will merge these automatically.
            if (localData.frequency > maxData.frequency) {
                maxData = localData;
            }
        }

        // The reduction has computed the overall maximum.
        highest_freq = maxData.frequency;
        most_freq_pair = maxData.key;
#else
        for (const auto& [key, val] : freqs) {
            if (val > highest_freq ) {
                highest_freq = val;
                most_freq_pair = key;
            }
        }
#endif
    }

    // only one iteration
    void reduce() {
        iterations += 1;
        // find most frequent:
        update_freq();

        if (highest_freq <= 1) return; // not compressable
        grammar.push_back(most_freq_pair); // introduce a new token

        int i = 0;
        while (i < tokens.size()) {
            if (tokens[i] >= grammar.size()) {
                cout << "Attempted to read token " << tokens[i] << " but that should be impossible." << endl;
                throw runtime_error("Token is impossible");
            }

            if (i == tokens.size() - 1){ // no pair can be created so just append the token
                buffer.push_back(tokens[i]);
                i += 1;
            }
            else {
                Pair pair = {tokens[i], tokens[i+1]};

                if (pair == most_freq_pair) { // pair is most frequent
                    // a[bc]d -> aZd
                    // decrease old left (ab) if a exists
                    if (buffer.size() > 0) {
                        Pair lpair = {buffer[buffer.size()-1], tokens[i]};
                        //assert lpair in pair_freqs
                        freqs[lpair] -= 1;
                        if (freqs[lpair] == 0)
                            freqs.erase(lpair);
                    }
                    // decrease old right (cd) if d exists
                    if (i < tokens.size() - 2) {
                        // print("Decreasing right: ", tokens_in[idx+1], tokens_in[idx+2])
                        Pair rpair = {tokens[i+1], tokens[i+2]};
                        // assert rpair in pair_freqs
                        freqs[rpair] -= 1;
                        if (freqs[rpair] == 0)
                            freqs.erase(rpair);
                    }
                    // increase new left (aZ) but only if there was already a token in tokens_out
                    if (buffer.size() > 0){
                        Pair lpair = {buffer[buffer.size() - 1], grammar.size() - 1};
                        freqs[lpair] += 1;
                    }
                    // increase new right (Zd) but only if there is a future token in input
                    if (i < tokens.size() - 2) {
                        Pair rpair = {grammar.size() - 1, tokens[i+2]};
                        freqs[rpair] += 1;
                    }
                    buffer.push_back(grammar.size() - 1); // replace with new token
                    // decrease frequency we just replaced.
                    freqs[pair] -= 1; // this will reach 0 at the end
                    if (freqs[pair] == 0)
                        freqs.erase(pair);
                    i += 2;                            // we just read the pair
                }
                else {
                    buffer.push_back(tokens[i]);
                    i += 1;
                }
            }
        }
        // interchange buffer and tokens.
        tokens = buffer;
        buffer.clear();
    } // END REDUCE

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
    e.reduce();
    cout << e;
    while (e.highest_freq > 1) {
        e.reduce();
        if (e.iterations % 10 == 0)
            cout << e;
    }

    e.serialize("shakie.bpe");

    return 0;
}
