CC = g++

project2: project2.cpp
	@echo "Making project2 object file..."
	${CC} project2.cpp -o project2

clean:
	@echo "Cleaning up..."
	rm -rvf project2