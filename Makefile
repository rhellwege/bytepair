bytepair: bytepair.cpp heap_map.hpp linked_array.hpp fib_heap_map.hpp
	g++ -fsanitize=address -o bytepair ./bytepair.cpp
