base64test: base64test.c ../File.c ../String.c ../Mem.c ../dpf.c ../ArrayU8.c
	gcc -Wall -o base64test $^
