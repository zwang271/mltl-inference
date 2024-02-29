all: interpret interpret_batch

interpret: 
	g++ ./interpret.cpp ./evaluate_mltl.cpp ./utils.cpp -o ./bin/interpret

interpret_batch:
	g++ ./interpret_batch.cpp ./evaluate_mltl.cpp ./utils.cpp -o ./bin/interpret_batch

clean:
	rm -f ./bin/interpret
	rm -f ./bin/interpret_batch
	