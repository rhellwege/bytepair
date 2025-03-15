import signal
from typing import List
from typing import Tuple
from pickle import dump, load

def render_tokens(toks, grammar):
    for token in toks:
        rule = grammar[token]
        if rule[0] == token: # leaf, just render as character
            print(chr(token), end='')
        else:
            print(f"[{token}]", end='')
    print()
    print()

running = True

def make_bpe_encoding(input: str) -> tuple[List[int], List[tuple[int, int]]]:
    # these two lists will act as a double buffer
    # contains pairs of ints (more than bytes to account for nonterminals)
    tokens_in = []
    tokens_out = []
    pair_freqs = {} # we will reuse this
    iterations = 0

    #tokenize the input by iterating overlapping pairs
    for i in range(len(input) - 1):
        if type(input[i]) == str:
            tokens_in.append(ord(input[i]))
        else:
            tokens_in.append(int(input[i]))

    # leaf nodes (terminal symbols) have a left child that refers to themselves
    # stores pairs where the index is the token id
    grammar = []
    # setup terminals
    for i in range(256):
        grammar.append((i, 0))

    # initialize pair frequencies to update in place
    for i in range(len(tokens_in)-1):
        pair = (tokens_in[i], tokens_in[i+1])
        if pair not in pair_freqs:
            pair_freqs[pair] = 1
        else:
            pair_freqs[pair] += 1

    while True:
        iterations += 1
        if not running:
            break
        # if iterations > 10:
        #     break
        # print("-"*50)

        # print(tokens_in)
        # print()
        # print(pair_freqs)


        # find most frequent pair
        max_freq = 0
        most_frequent_pair = None
        for pair, freq in pair_freqs.items():
            if freq > max_freq:
                max_freq = freq
                most_frequent_pair = pair


        if max_freq <= 1:
            break # we cannot compress further
        grammar.append(most_frequent_pair)
        new_tok = len(grammar) - 1

        if iterations % 10 == 0:
            print("most frequent pair = ", most_frequent_pair, max_freq)
            print("token length, grammar length = ", len(tokens_in), len(grammar))

        # now we apply the new non-terminal replacement and update frequencies.
        idx = 0
        while idx < (len(tokens_in)):
            if tokens_in[idx] >= len(grammar):
                raise ValueError("Token is impossible")

            if idx == len(tokens_in) - 1: # no pair can be created so just append the token
                tokens_out.append(tokens_in[idx])
                idx += 1
            else:
                pair = (tokens_in[idx], tokens_in[idx+1])
                if pair == grammar[-1]: # pair is most frequent
                    # a[bc]d -> aZd
                    # decrease old left (ab) if a exists
                    if len(tokens_out) > 0:
                        lpair = (tokens_out[-1], tokens_in[idx])
                        # print("Decreasing left: ", lpair)
                        assert lpair in pair_freqs
                        pair_freqs[lpair] -= 1
                        if pair_freqs[lpair] == 0:
                            del pair_freqs[lpair]

                    # decrease old right (cd) if d exists
                    if idx < len(tokens_in) - 2:
                        # print("Decreasing right: ", tokens_in[idx+1], tokens_in[idx+2])
                        rpair = (tokens_in[idx+1], tokens_in[idx+2])
                        assert rpair in pair_freqs
                        pair_freqs[rpair] -= 1
                        if pair_freqs[rpair] == 0:
                            del pair_freqs[rpair]

                    # increase new left (aZ) but only if there was already a token in tokens_out
                    if len(tokens_out) > 0:
                        lpair = (tokens_out[-1], new_tok)
                        if lpair in pair_freqs:
                            pair_freqs[lpair] += 1
                        else:
                            pair_freqs[lpair] = 1

                    # increase new right (Zd) but only if there is a future token in input
                    if idx < len(tokens_in) - 2:
                        rpair = (new_tok, tokens_in[idx+2])
                        if rpair in pair_freqs:
                            pair_freqs[rpair] += 1
                        else:
                            pair_freqs[rpair] = 1

                    tokens_out.append(new_tok) # replace with new token
                    # decrease frequency we just replaced.
                    pair_freqs[pair] -= 1 # this will reach 0 at the end
                    if pair_freqs[pair] == 0:
                        del pair_freqs[pair]
                    idx += 2                            # we just read the pair
                else:
                    tokens_out.append(tokens_in[idx])
                    idx += 1

        #render_tokens(tokens_in, grammar)
        # get ready for next iteration:
        tokens_in, tokens_out = tokens_out, []
    # at this point, text is maximally compressed.
    return (tokens_in, grammar)


def reverse_bpe(tokens_in, vocab):
    output = []
    for token in tokens_in:
        output += (vocab[token])
    output_bytes = [token.to_bytes(1, 'little') for token in output]
    return output_bytes

def handler(signum, frame):
    global running
    running = False

def get_terminals(grammar, token, mem):
    if grammar[token][0] == token:
        mem[token] = [token]
        return [token]
    else:
        if token in mem:
            return mem[token]
        s = get_terminals(grammar, grammar[token][0], mem) + get_terminals(grammar, grammar[token][1], mem)
        mem[token] = s
        return s

def generate_vocab(grammar):
    vocab = {}
    for i in range(len(grammar)):
        get_terminals(grammar, i, vocab)
    return vocab

def generate_reverse_vocab(vocab):
    reverse_vocab = {tuple(v): k for k, v in vocab.items()}
    return reverse_vocab

def tokenize(input, reverse_vocab):
    if type(input[0]) == str:
        input = [ord(x) for x in input]
    else:
        input = [int(x) for x in input]
    tokens = []
    # try to get the biggest token first
    end = len(input)
    print(input)
    while end >= 0:
        #print("Checking", tuple(input[0:end]))
        if tuple(input[0:end]) in reverse_vocab:
            tokens.append(reverse_vocab[tuple(input[0:end])])
            input = input[end:]
            print(input)
            end = len(input)
        else:
            end -= 1
    return tokens

if __name__ == '__main__':
    signal.signal(signal.SIGINT, handler)
    with open("./shakespear.txt", "rb") as f:
        input = f.read()
        tokens, grammar = make_bpe_encoding(input)
        print("writing tokens, grammar with ", len(tokens), " tokens, and ", len(grammar), " rules")
        with open('shakespear_inplace.pkl', 'wb') as t:
            dump((tokens, grammar), t)

    # with open("./jeeves.pkl", "rb") as f:
    #     (tokens, grammar) = load(f, encoding='utf-8')

        vocab = generate_vocab(grammar)
        reverse_vocab = generate_reverse_vocab(vocab)
        with open("./shakespear_inplace_vocab.pkl", "wb") as f:
            dump((vocab, reverse_vocab), f)
