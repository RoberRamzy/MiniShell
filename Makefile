## you have command.cc command.h tokenizer.cc tokenizer.h you need compile them and link
## them to one object file called  : "myshell" 
## so we should run make , that will create our object myshell ,
## ./myshell should run your application 
all: myshell

myshell: command.cc tokenizer.cc command.h tokenizer.h
	g++ -std=c++11 -Wall -g -o myshell command.cc tokenizer.cc

# Rule to clean up compiled files
clean:
	rm -f myshell child_log.txt
