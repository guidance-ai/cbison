TARGET = ../target/release

all:
	cd python && python -m cbison.test_llg
	cd ../llguidance_cbison && cargo build --release
	c++ -g -W -Wall -std=c++20 -o $(TARGET)/cbison cpp/*.cpp -Icpp 
	$(TARGET)/cbison $(TARGET)/libllguidance_cbison.dylib llg
