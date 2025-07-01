RouteTesting: RouteTesting.cpp ../include/Network/Servers/*.hpp Normalization.o ROString.o ../include/Streams/*.hpp
	g++ -std=c++20 -I ../include -g -O0 $< ROString.o Normalization.o -o $@




HeadersParsing: HeadersParsing.cpp ../include/Protocol/HTTP/*.hpp ../include/Reflection/*.hpp ROString.o
	g++ -std=c++23 -I ../include -g -O0 $< ROString.o -o $@

PathNormalization: PathNormalization.cpp ../include/Path/Normalization.hpp ../src/Normalization.cpp ROString.o
	g++ -std=c++20 -I ../include -g -O0 $< ROString.o -o $@

ROString.o: ../src/ROString.cpp ../include/Strings/ROString.hpp
	g++ -std=c++20 -I ../include -I ../include/Strings -g -O0 -c $< -o $@

Normalization.o: ../src/Normalization.cpp ../include/Path/Normalization.hpp ROString.o
	g++ -std=c++20 -I ../include -I ../include/Strings -g -O0 -c $< -o $@
