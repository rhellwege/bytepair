bytepair: bytepair.cpp heap_map.hpp linked_array.hpp
	g++ -g -fsanitize=address -o bytepair ./bytepair.cpp
