CC=gcc
flags=-O4 -g -static
libraries=-lgsl -lgslcblas -lm

objects = doc_hash.o topic_hash.o error_msgs.o get_qrels.o get_prels.o get_top.o get_judgment_list.o hashtable.o hashtable_itr.o evals.o
evaluate = evaluate.o
expert = expert.o tr_irls.o

all: evaluate expert

evaluate: $(objects) $(evaluate)
	$(CC) $(flags) -o evaluate $+ $(libraries)

expert: $(objects) $(expert)
	$(CC) $(flags) -o expert $+ $(libraries)

%.o: %.c
	$(CC) $(flags) -c $+
