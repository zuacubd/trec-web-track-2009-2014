#ifndef DEF
#define DEF	1

#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <pthread.h>

#include "trec_eval.h"
#include "common.h"

#define MAXTOPICS 2000
#define MAXRANK 100
#define MAXRUNS 255

#define NUMALGS 4
#define ALGSNONALT 2

#define UMASS 0
#define NEU 1
#define ALTERNATE 2
#define ALTERNATE2 3
#define NEUNEW 4

#define UMASSALG(x) ((x) == UMASS || (x) == ALTERNATE)

struct doc_value
{
	unsigned short index;  // allows 65,545 docs per topic--adequate for forseeable uses
	unsigned short *ranks;
	double p;
};

unsigned long sdbm (unsigned char *str);
unsigned int doc_hash (void *ky);
int doc_keycmp (void *k1, void *k2);

typedef struct
{
	double *theta;
	double *baseline_p;
} topic_pr;

typedef struct
{
	int user;
	int alt;

	gsl_vector *judged;
	gsl_vector *rel;
	short int *judged_by;
	short int *judgment_list;
	double *p;

	topic_pr *topic_p;

	double *eap;
	double conf;

	double *ep10, *ep30, *ep50;
	double *endcg10, *endcg30, *endcg50;
	double *eRprec;

	gsl_matrix *rel_w;
	gsl_matrix *non_w;
	gsl_vector *rel_w2;
	gsl_vector *non_w2;
} user_topic;

typedef struct
{
	user_topic *users[255];
	int numusers;
	int evaluser;

	int n;
	int S;

	int algorithm;

	pthread_mutex_t lock;
	pthread_mutex_t evallock;

	char **docno;
	struct hashtable *doc_t;
	unsigned short *ranking[MAXRUNS];
	unsigned short ranklen[MAXRUNS];
	char *runid[MAXRUNS];
} _topic;

unsigned int topic_hash (void *ky);
int topic_keycmp (void *k1, void *k2);

void *handle_requests_loop (void *);

void deltaAP(int, int, unsigned short *, unsigned short *, double *, double *, double *, int);
double meanAP (int, unsigned short *, double *);
double meanP (int, int, unsigned short int *, double *);
double meanNDCG (int, int, unsigned short int *, double *, int);
double meanRprec (int, double, unsigned short int *, double *);

struct request
{
	int number;
	struct request *next;
};

#endif
