RouteTesting: RouteTesting.cpp ../include/Network/Servers/*.hpp Normalization.o ROString.o ../include/Streams/*.hpp
	g++ -std=c++20 -I ../include -I ../../eCommon/include/ -g -O0 $< ROString.o Normalization.o -o $@

HeadersParsing: HeadersParsing.cpp ../include/Protocol/HTTP/*.hpp ../include/Reflection/*.hpp ROString.o
	g++ -std=c++23 -I ../include -I ../../eCommon/include/ -g -O0 $< ROString.o -o $@

PathNormalization: PathNormalization.cpp ../include/Path/Normalization.hpp ../src/Normalization.cpp ROString.o
	g++ -std=c++20 -I ../include -g -O0 $< ROString.o -o $@

eurl: eurl.cpp ../include/Network/Clients/*.hpp ../include/Network/Common/*.hpp ROString.o ../include/Streams/*.hpp
	g++ -std=c++20 -I ../include -I ../../eCommon/include -I ../../mbedtls/install/include -L ../../mbedtls/install/lib  -g -O0 $< ROString.o -lmbedtls -lmbedx509 -lmbedcrypto -o $@

ROString.o: ../../eCommon/src/Strings/ROString.cpp ../../eCommon/include/Strings/ROString.hpp
	g++ -std=c++20 -I ../include -I ../../eCommon/include/ -g -O0 -c $< -o $@

Normalization.o: ../src/Normalization.cpp ../include/Path/Normalization.hpp ROString.o
	g++ -std=c++20 -I ../include -I ../../eCommon/include/ -g -O0 -c $< -o $@

min:
	g++ -std=c++20 -I ../include -I ../../eCommon/include -Os -ffunction-sections -fdata-sections -c ../src/Normalization.cpp -o Normalization_s.o
	g++ -std=c++20 -I ../include -I ../../eCommon/include -Os -ffunction-sections -fdata-sections -c ../../eCommon/src/Strings/ROString.cpp -o ROString_s.o
	g++ -std=c++20 -I ../include -I ../../eCommon/include -Os -ffunction-sections -fdata-sections RouteTesting.cpp Normalization_s.o ROString_s.o -Wl,--gc-sections -s -o RouteTesting_s

