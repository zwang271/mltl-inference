all: interpret interpret_batch

interpret:
	mkdir -p bin
	g++ ./interpret.cpp ./evaluate_mltl.cpp ./utils.cpp -o ./bin/interpret

interpret_batch:
	mkdir -p bin
	g++ ./interpret_batch.cpp ./evaluate_mltl.cpp ./utils.cpp -o ./bin/interpret_batch

clean:
	rm -rf ./bin

