program : derle calistir

derle:
	gcc osProje.c -o osProje

calistir:
	./osProje

test:
	gcc -Itests/unity tests/test_osProje.c tests/unity/unity.c -o tests/test_osProje
	./tests/test_osProje

.PHONY: program derle calistir test
