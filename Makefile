# APD - Tema 1
# Octombrie 2021

build:
	@echo "Building..."
	@gcc -o tema1_par tema1_par.c genetic_algorithm_par.c -lm -Wall -Werror -lpthread -g
	@echo "Done"

build_debug:
	@echo "Building debug..."
	@gcc -o tema1_par tema1_par.c genetic_algorithm_par.c -lm -Wall -Werror -O0 -g3 -DDEBUG -lpthread
	@echo "Done"

clean:
	@echo "Cleaning..."
	@rm -rf tema1_par
	@echo "Done"
