// expert model fit and predict
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_cblas.h>
#include <gsl/gsl_linalg.h>

#include "trec_eval.h"
#include "common.h"
#include "sysfunc.h"
#include "smart_error.h"

#include "hashtable.h"
#include "def.h"

void calc_baseline_p();
void calibrate(int, char **, gsl_matrix *, double);
void lr(gsl_vector *, gsl_matrix *, gsl_vector *, gsl_vector *);
void lr_cv(gsl_vector *, gsl_matrix *, gsl_vector *, gsl_vector *, int);

struct hashtable *topic_t;

int main(int argc, char *argv[])
{
	char qrelsfile[256], basedir[256], buf[256];
	int i, j, k, S;
	char *runid[255];
	int mqt = 0;
	double cal_smooth = 1.0;

	//mtrace();

	// read command line
	while (argc > 1 && argv[1][0] == '-')
	{
		if (argv[1][1] == 'q')
		{
			strcpy(qrelsfile, argv[2]);
			argc -= 2; argv += 2;
		}
		else if (argv[1][1] == 'd')
		{
			strcpy(basedir, argv[2]);
			argc -= 2; argv += 2;
		}
		else if (argv[1][1] == '5')
		{
			mqt = 1;
			argc--; argv++;
		}
		else if (argv[1][1] == 'w')
		{
			cal_smooth = (double)atof(argv[2]);
			argc -= 2; argv += 2;
		}
	}

	// read in qrels
	topic_t = create_hashtable(500, topic_hash, topic_keycmp);

	ALL_TREC_QRELS qrels;
	if (UNDEF == get_qrels(qrelsfile, &qrels, mqt))
		fprintf(stderr, "could not read qrels\n");

	for (i=0; i<qrels.num_q_qrels; i++)
	{
		_topic *t = (_topic *)search_topic(topic_t, qrels.trec_qrels[i].qid);
		if (t == NULL)
		{
			t = (_topic *)malloc(sizeof(_topic));
			struct hashtable *doc_t = create_hashtable(5000, doc_hash, doc_keycmp);
			t->doc_t = doc_t;
			t->S = 0;
			t->n = 0;

			t->users[0] = (user_topic *)malloc(sizeof(user_topic));

			char *topic_key = (char *)malloc((strlen(qrels.trec_qrels[i].qid)+1)*sizeof(char));
			strcpy(topic_key, qrels.trec_qrels[i].qid);

			if (!insert_topic(topic_t, topic_key, t))
				fprintf(stderr, "hash error inserting topic %s\n", topic_key);
		}

		for (j=0; j<qrels.trec_qrels[i].num_text_qrels; j++)
		{
			struct doc_value *v;
			if ((v = hashtable_search(t->doc_t, qrels.trec_qrels[i].text_qrels[j].docno)) == NULL)
			{
				v = (struct doc_value *)malloc(sizeof(struct doc_value));
				v->index = hashtable_count(t->doc_t);
				v->ranks = (unsigned short *)calloc(25, sizeof(unsigned short));

				char *doc_key = (char *)malloc((strlen(qrels.trec_qrels[i].text_qrels[j].docno)+1)*sizeof(char));
				strcpy(doc_key, qrels.trec_qrels[i].text_qrels[j].docno);

				if (!insert_some(t->doc_t, doc_key, v))
					fprintf(stderr, "hash error\n");
			}
		}
		t->n = hashtable_count(t->doc_t);
	}

	// read in runs
	// pad as reading in
	DIR *dp = opendir(basedir);
	struct dirent *ep;
	for (S=0; ep = readdir(dp); S++)
	{
		runid[S] = (char *)malloc((strlen(ep->d_name)+strlen(basedir)+2)*sizeof(char));
		sprintf(runid[S], "%s/%s", basedir, ep->d_name);
	}

	for (k=0; k<S; k++)
	{
		ALL_TREC_TOP run;
		if (UNDEF == get_top(runid[k], &run))
		{
			fprintf(stderr, "could not read run %s\n", runid[k]);
			int k2;
			for (k2=k; k2<S; k2++)
				runid[k2] = runid[k2+1];
			S--; k--;
			continue;
		}
		fprintf(stderr, "read run %s\n", runid[k]);

		for (i=0; i<run.num_q_tr; i++)
		{
			_topic *t;
			t = (_topic *)search_topic(topic_t, run.trec_top[i].qid);
			if (t == NULL) continue;

			t->runid[t->S] = (char *)malloc((strlen(run.run_id)+1)*sizeof(char));
			strcpy(t->runid[t->S], run.run_id);

			runid[t->S] = (char *)realloc(runid[t->S], (strlen(run.run_id)+1)*sizeof(char));
			strcpy(runid[t->S], run.run_id);

			t->ranklen[t->S] = MAXRANK;
			t->ranking[t->S] = (unsigned short *)malloc(t->ranklen[t->S]*sizeof(unsigned short));

			for (j=0; j<run.trec_top[i].num_text_tr && j<MAXRANK; j++)
			{
				struct doc_value *v;
				v = (struct doc_value *)search_some(t->doc_t, run.trec_top[i].text_tr[j].docno);
				if (v != NULL)
				{
					t->ranking[t->S][j] = v->index;
					if (v->ranks == NULL)
						v->ranks = (unsigned short *)calloc(25, sizeof(unsigned short));
					v->ranks[t->S] = j+1;
					continue;
				}

				//printf("%s %s %d %s\n", runid[t->S], run.trec_top[i].qid, j, run.trec_top[i].text_tr[j].docno);
				v = (struct doc_value *)malloc(sizeof(struct doc_value));
				v->index = hashtable_count(t->doc_t);
				v->ranks = (unsigned short *)calloc(25, sizeof(unsigned short));
				v->ranks[t->S] = j+1;

				t->ranking[t->S][j] = v->index;

				char *doc_key = (char *)malloc((strlen(run.trec_top[i].text_tr[j].docno)+1)*sizeof(char));
				strcpy(doc_key, run.trec_top[i].text_tr[j].docno);

				if (!insert_some(t->doc_t, doc_key, v))
					fprintf(stderr, "hash error\n");
			}
			for (; j<MAXRANK; j++)
			{
				char *nodoc;
				nodoc = (char *)malloc(sizeof(char)*255);
				sprintf(nodoc, "NODOC.%s.%s.%d", runid[k], run.trec_top[i].qid, j);
				struct doc_value *v = (struct doc_value *)malloc(sizeof(struct doc_value));
				v->index = hashtable_count(t->doc_t);
				v->ranks = (unsigned short *)calloc(25, sizeof(unsigned short));
				v->ranks[t->S] = j+1;
				t->ranking[t->S][j] = v->index;
				if (!insert_some(t->doc_t, nodoc, v))
					fprintf(stderr, "hash error\n");
			}

			t->n = hashtable_count(t->doc_t);
			t->S++;
		}

		delete_top(&run);
	}

	struct hashtable_itr *itr = (struct hashtable_itr *)hashtable_iterator(topic_t);
	do {
		_topic *t = (_topic *)hashtable_iterator_value(itr);
		t->users[0]->rel = gsl_vector_calloc(t->n);
		t->users[0]->p = (double *)malloc(t->n*sizeof(double));
		for (i=0; i<t->n; i++) t->users[0]->p[i] = 0.5;
	} while (hashtable_iterator_advance(itr));
	free(itr);

	int totaldocs = 0, totaljudged = 0;

	for (i=0; i<qrels.num_q_qrels; i++)
	{
		_topic *t = (_topic *)search_topic(topic_t, qrels.trec_qrels[i].qid);
		if (t == NULL) continue;

		totaldocs += t->n;

		for (j=0; j<qrels.trec_qrels[i].num_text_qrels; j++)
		{
			struct doc_value *v;
			if ((v = hashtable_search(t->doc_t, qrels.trec_qrels[i].text_qrels[j].docno)) == NULL)
				continue;

			if (v->ranks == NULL)
				v->ranks = (unsigned short *)calloc(25, sizeof(unsigned short));

			totaljudged++;

			if (qrels.trec_qrels[i].text_qrels[j].rel >= 1)
			{
				gsl_vector_set(t->users[0]->rel, v->index, 1);
				t->users[0]->p[v->index] = 1.0;
			}
			else
			{
				gsl_vector_set(t->users[0]->rel, v->index, 0);
				t->users[0]->p[v->index] = 0.0;
			}
		}
	}

	delete_qrels(&qrels);

	// do preferences with LR
	fprintf(stderr, "calculating baseline probabilities..\n");
	calc_baseline_p();

	// do calibration with LR
	int T = hashtable_count(topic_t);
	fprintf(stderr, "calibrating...\n");
	gsl_matrix *calibrated = gsl_matrix_alloc((MAXRANK+1)*T, S);

	calibrate(S, runid, calibrated, cal_smooth);

	// fit and predict with LR
	// to generate training data:
	// iterate over topics
	// iterate over docs
	// locate rank of doc in each system
	// get probabilities from calibration matrix
	gsl_vector *y = gsl_vector_alloc(totaljudged);
	gsl_matrix *train = gsl_matrix_alloc(totaljudged, S);
	//gsl_matrix *test = gsl_matrix_alloc(totaldocs, S);

	int bl = 0;
	int trainct = 0;
	//int testct = 0;

	//char **docno = (char **)malloc(totaldocs*sizeof(char *));
	//char **doctopic = (char **)malloc(totaldocs*sizeof(char *));

	itr = (struct hashtable_itr *)hashtable_iterator(topic_t);
	do {
		_topic *t = (_topic *)hashtable_iterator_value(itr);
		char *topic = (char *)hashtable_iterator_key(itr);

		struct hashtable_itr *itr2 = (struct hashtable_itr *)hashtable_iterator(t->doc_t);
		do {
			struct doc_value *v = (struct doc_value *)hashtable_iterator_value(itr2);
			char *key = (char *)hashtable_iterator_key(itr2);

			//docno[testct] = (char *)malloc((strlen(key)+1)*sizeof(char));
			//strcpy(docno[testct], key);
			//doctopic[testct] = (char *)malloc((strlen(topic)+1)*sizeof(char));
			//strcpy(doctopic[testct], topic);

			if (t->users[0]->p[v->index] != 0.5)
			{
				int rel = (int)gsl_vector_get(t->users[0]->rel, v->index);
				gsl_vector_set(y, trainct, rel);

				//printf("%d %s (%d):\n", trainct, key, rel);
				for (i=0; i<S; i++)
				{
					//printf("%d bl:%d ranks:%d %d\n", (MAXRANK+1)*T, bl, v->ranks[i], i);
					gsl_matrix_set(train, trainct, i, gsl_matrix_get(calibrated, bl+(v->ranks[i]?(v->ranks[i]-1):MAXRANK), i));
				}

				trainct++;
			}

			//for (i=0; i<S; i++)
				//gsl_matrix_set(test, testct, i, gsl_matrix_get(calibrated, bl+(v->ranks[i]?(v->ranks[i]-1):MAXRANK), i));

			//testct++;
		} while (hashtable_iterator_advance(itr2));
		free(itr2);

		bl += MAXRANK+1;
	} while (hashtable_iterator_advance(itr));
	free(itr);

	/*
	for (i=0; i<S; i++)
	{
		gsl_vector_set(y, totaljudged+i, 1);
		gsl_vector_set(y, totaljudged+i+S, 0);
		gsl_matrix_set(train, totaljudged+i, i, 1);
		gsl_matrix_set(train, totaljudged+i+S, i, 1);
	}
	*/

	gsl_vector *w = gsl_vector_alloc(totaljudged+2*S);
	gsl_vector_set_all(w, 1);

	gsl_vector *beta = gsl_vector_calloc(S);
	fprintf(stderr, "training...\n");
	lr_cv(y, train, w, beta, 5);

	// test 1000 docs at a time
	gsl_matrix *test = gsl_matrix_alloc(1000, S);

	bl = 0;
	int testct = 0;
	int tcm = 0;

	char **docno = (char **)malloc(1000*sizeof(char *));
	char **doctopic = (char **)malloc(1000*sizeof(char *));

	itr = (struct hashtable_itr *)hashtable_iterator(topic_t);
	do {
		_topic *t = (_topic *)hashtable_iterator_value(itr);
		char *topic = (char *)hashtable_iterator_key(itr);

		struct hashtable_itr *itr2 = (struct hashtable_itr *)hashtable_iterator(t->doc_t);
		do {
			struct doc_value *v = (struct doc_value *)hashtable_iterator_value(itr2);
			char *key = (char *)hashtable_iterator_key(itr2);

			docno[tcm] = (char *)malloc((strlen(key)+1)*sizeof(char));
			strcpy(docno[tcm], key);
			doctopic[tcm] = (char *)malloc((strlen(topic)+1)*sizeof(char));
			strcpy(doctopic[tcm], topic);

			for (i=0; i<S; i++)
				gsl_matrix_set(test, tcm, i, gsl_matrix_get(calibrated, bl+(v->ranks[i]?(v->ranks[i]-1):MAXRANK), i));

			testct++;
			tcm = testct % 1000;

			if (tcm == 0 || testct == totaldocs)
			{
				gsl_vector *yhat = gsl_vector_alloc(1000);
				gsl_blas_dgemv(CblasNoTrans, 1, test, beta, 0, yhat);

				gsl_vector *p = gsl_vector_alloc(1000);
				for (j=0; j<1000; j++)
				{
					double yh = gsl_vector_get(yhat, j);
					gsl_vector_set(p, j, exp(yh)/(1+exp(yh)));
				}

				// write probabilities to disk
				for (j=0; j<1000 && j<totaldocs; j++)
					printf("%s Q0 %s %g\n", doctopic[j], docno[j], gsl_vector_get(p, j));
			}
		} while (hashtable_iterator_advance(itr2));
		free(itr2);

		bl += MAXRANK+1;
	} while (hashtable_iterator_advance(itr));
	free(itr);

}

// needs to return the C equivalent of an R list
// an array of structs perhaps would be best
void calc_baseline_p()
{
	int X[MAXRANK+2][MAXRANK+2];
	double *x;
	int alpha = 1, beta = 1;
	int i, j, k;

	for (i=0; i<MAXRANK+2; i++)
		for (j=0; j<MAXRANK+2; j++) X[i][j] = 0;

	// first make an NxN design matrix, where N = MAXRANK
	for (i=1; i<MAXRANK+2; i++)
		for (j=1; j<i; j++)
			X[i][j] = 1;

	// then iterate over topics
	// run trirls to get thetas
	struct hashtable_itr *itr = (struct hashtable_itr *)hashtable_iterator(topic_t);
	do {
		_topic *t = (_topic *)hashtable_iterator_value(itr);
		char *topic = (char *)hashtable_iterator_key(itr);

		// count rels and nonrels
		int nonct = 0, relct = 0;
		for (i=0; i<t->n; i++)
		{
			if (t->users[0]->p[i] == 0.0) nonct++;
			if (t->users[0]->p[i] == 1.0) relct++;
		}

		// set priors
		for (i=1; i<MAXRANK+2; i++)
		{
			X[0][i] = alpha+relct;
			X[i][0] = beta+nonct;
		}

		topic_pr *tp = (topic_pr *)malloc(sizeof(topic_pr));
		tp->theta = (double *)calloc(MAXRANK+1, sizeof(double));
		tp->baseline_p = (double *)malloc((MAXRANK+1)*sizeof(double));

		x = (double *)malloc((MAXRANK+2)*(MAXRANK+2)*sizeof(double));
		for (i=0, k=0; i<MAXRANK+2; i++)
			for (j=0; j<MAXRANK+2; j++, k++)
				x[k] = (double)X[j][i];

		tr_irls(MAXRANK+1, x, tp->theta, 0.0001, 0.0001);
		free(x);

		for (i=0; i<MAXRANK+1; i++)
			tp->baseline_p[i] = exp(tp->theta[i])/(1+exp(tp->theta[i]));

		t->users[0]->topic_p = tp;
	} while(hashtable_iterator_advance(itr));
	free(itr);
}

void calibrate(int S, char *runid[255], gsl_matrix *calibrated, double smooth)
{
	int i, j, k, ind;
	int T = hashtable_count(topic_t);
	int len = (MAXRANK+1)*T;
	double yh;
	gsl_matrix *X;
	gsl_vector *y, *w, *p;

	for (i=0; i<S; i++)
	{
		y = gsl_vector_calloc(2*len);
		for (j=0; j<len; j++)
			gsl_vector_set(y, j, 1);

		w = gsl_vector_alloc(2*len);
		gsl_vector_set_all(w, smooth);

		X = gsl_matrix_alloc(2*len, 2);
		gsl_matrix_set_all(X, 1);

		p = gsl_vector_alloc(len);

		k = 0;

		int tc = 0;
		struct hashtable_itr *itr = (struct hashtable_itr *)hashtable_iterator(topic_t);
		do {
			_topic *t = (_topic *)hashtable_iterator_value(itr);
			tc++;

			for (j=0; j<t->ranklen[i]; j++)
			{
				gsl_matrix_set(X, k, 1, t->users[0]->topic_p->baseline_p[j]);
				gsl_matrix_set(X, len+k, 1, t->users[0]->topic_p->baseline_p[j]);

				ind = t->ranking[i][j];
				if (t->users[0]->p[ind] == 1.0)
					gsl_vector_set(w, k, (int)gsl_vector_get(w, k)+1);
				else if (t->users[0]->p[ind] == 0.0)
					gsl_vector_set(w, len+k, (int)gsl_vector_get(w, len+k)+1);
				k++;
			}

			// now do unranked docs
			gsl_matrix_set(X, k, 1, 0);
			gsl_matrix_set(X, len+k, 1, 0);

			struct hashtable_itr *itr2 = (struct hashtable_itr *)hashtable_iterator(t->doc_t);
			do {
				struct doc_value *v = (struct doc_value *)hashtable_iterator_value(itr2);

				int ind = v->index;
				if (t->users[0]->p[ind] == .5) continue;

				if (v->ranks[i] == 0)
					if (t->users[0]->p[ind] == 1.0)
						gsl_vector_set(w, k, (int)gsl_vector_get(w, k)+1);
					else
						gsl_vector_set(w, len+k, (int)gsl_vector_get(w, len+k)+1);

			} while(hashtable_iterator_advance(itr2));
			free(itr2);

			k++;

		} while (hashtable_iterator_advance(itr));
		free(itr);

		// lr it
		fprintf(stderr, "calibrating system %d...", i);
		gsl_vector *beta = gsl_vector_calloc(2);
		lr(y, X, w, beta);

		gsl_vector *yhat = gsl_vector_alloc(2*len);
		gsl_blas_dgemv(CblasNoTrans, 1, X, beta, 0, yhat);

		for (j=0; j<len; j++)
		{
			yh = gsl_vector_get(yhat, j);
			gsl_vector_set(p, j, exp(yh)/(1+exp(yh)));
		}

		gsl_matrix_set_col(calibrated, i, p);

		gsl_vector_free(yhat);
		gsl_vector_free(beta);
	}
	gsl_vector_free(y);
	gsl_vector_free(w);
	gsl_vector_free(p);
	gsl_matrix_free(X);
}

void lr_cv(gsl_vector *y, gsl_matrix *X, gsl_vector *w, gsl_vector *beta, int folds)
{
	int i, j, k, f;
	int S = X->size2;
	int n = X->size1;
	int ct[folds], fold[n];
	int inbin[20], relbin[20];

	for (i=0; i<20; i++)
	{
		inbin[i] = 0;
		relbin[i] = 0;
	}

	for (i=0; i<folds; i++)
		ct[i] = 0;

	for (i=0; i<n; i++)
	{
		f = i % folds;
		ct[f]++;
		fold[i] = f;
	}

	for (i=0; i<folds; i++)
	{
		int trainct = 0;
		for (j=0; j<folds; j++)
			if (j != i) trainct += ct[j];

		gsl_matrix *train = gsl_matrix_alloc(trainct+2*S, S);
		gsl_matrix *test = gsl_matrix_alloc(ct[i], S);
		gsl_vector *ytrain = gsl_vector_alloc(trainct+2*S);
		gsl_vector *ytest = gsl_vector_alloc(ct[i]);

		gsl_vector *w = gsl_vector_alloc(trainct+2*S);
		gsl_vector_set_all(w, 1);
		gsl_vector *theta = gsl_vector_calloc(S);

		int tct = 0, testct = 0;
		for (j=0; j<n; j++)
		{
			gsl_vector_view instance = gsl_matrix_row(X, j);
			if (fold[j] == i)
			{
				gsl_matrix_set_row(test, testct, &instance.vector);
				gsl_vector_set(ytest, testct, gsl_vector_get(y, j));
				testct++;
			}
			else
			{
				gsl_matrix_set_row(train, tct, &instance.vector);
				gsl_vector_set(ytrain, tct, gsl_vector_get(y, j));

				tct++;
			}
		}

		for (j=0; j<S; j++)
		{
			gsl_vector_set(ytrain, trainct+j, 1);
			gsl_vector_set(ytrain, trainct+j+S, 0);

			for (k=0; k<S; k++)
			{
				gsl_matrix_set(train, trainct+j, k, 0);
				gsl_matrix_set(train, trainct+j+S, k, 0);
			}
			gsl_matrix_set(train, trainct+j, j, 1);
			gsl_matrix_set(train, trainct+j+S, j, 1);
		}

		lr(ytrain, train, w, theta);

		// test (not really necessary)
		gsl_vector *yhat = gsl_vector_alloc(ct[i]);
		gsl_blas_dgemv(CblasNoTrans, 1, test, theta, 0, yhat);

		gsl_vector_add(beta, theta);
	
		for (j=0; j<ct[i]; j++)
		{
			double yh = gsl_vector_get(yhat, j);
			double p = exp(yh)/(1+exp(yh));
			inbin[(int)floor(p*20)]++;
			if (gsl_vector_get(ytest, j))
				relbin[(int)floor(p*20)]++;
		}

		gsl_matrix_free(train);
		gsl_matrix_free(test);
		gsl_vector_free(ytrain);
		gsl_vector_free(ytest);
		gsl_vector_free(w);
		gsl_vector_free(yhat);
		gsl_vector_free(theta);
	}

	gsl_vector_scale(beta, 1.0/(double)folds);

	//for (i=0; i<20; i++)
		//printf("%.2f-%.2f %d %d %.4f\n", (double)i/100, ((double)i+1)/100, inbin[i], relbin[i], (double)relbin[i]/(double)inbin[i]);
}

void lr(gsl_vector *y, gsl_matrix *X, gsl_vector *w, gsl_vector *beta)
{
	int n = y->size, d = beta->size, i, j, it, ret;
	double yh, ty, tp, ll = 1000, lastll = -1000;
	gsl_vector *yhat = gsl_vector_alloc(n);
	gsl_vector *p = gsl_vector_alloc(n);
	gsl_vector *q = gsl_vector_alloc(n);
	gsl_vector *W = gsl_vector_alloc(n);
	gsl_vector *Z = gsl_vector_alloc(n);
	gsl_vector *XTWz = gsl_vector_alloc(d);
	gsl_matrix *WX = gsl_matrix_alloc(n, d);
	gsl_matrix *XTWX = gsl_matrix_alloc(d, d);

	// check X for singularity??
	// debugging
	/*
	for (i=0; i<n; i++)
	{
		fprintf(stderr, "%d (%g): ", i, gsl_vector_get(y, i));
		for (j=0; j<d; j++)
			fprintf(stderr, "%.2f ", gsl_matrix_get(X, i, j));
		fprintf(stderr, "\n");
	}
	*/

	for (it=0; it<25; it++)
	{
		ll = 0;
		// debugging
		/*
		for (i=0; i<d; i++)
			fprintf(stderr, "b_%d=%.3f ", i, gsl_vector_get(beta, i));
		fprintf(stderr, "\n");
		*/

		gsl_blas_dgemv(CblasNoTrans, 1, X, beta, 0, yhat);

		for (i=0; i<n; i++)
		{
			yh = gsl_vector_get(yhat, i);
			gsl_vector_set(p, i, exp(yh)/(1+exp(yh)));
			
			ty = gsl_vector_get(y, i);
			tp = gsl_vector_get(p, i);
			if (tp <= 0 || tp >= 1)
				fprintf(stderr, "fitted probability %.4f\n", tp);
			if (tp == 1 || tp == 0)
			{
				// ???
				if (tp == 1) tp = 1-.001;
				else tp = 0+.001;
				gsl_vector_set(p, i, tp);
			}
			ll += ty*log(tp) + (1-ty)*log(1-tp);
		}
		ll = -2*ll;
	
		gsl_vector_memcpy(q, p);
		gsl_vector_scale(q, -1);
		gsl_vector_add_constant(q, 1);
	
		gsl_vector_memcpy(W, p);
		gsl_vector_mul(W, q);
	
		gsl_vector_memcpy(Z, y);
		gsl_vector_sub(Z, p);
		gsl_vector_div(Z, W);
		gsl_vector_add(Z, yhat);
	
		gsl_vector_mul(W, w);
	
		// compute X^TWZ
		gsl_vector_mul(Z, W);
		gsl_blas_dgemv(CblasTrans, 1, X, Z, 0, XTWz);
	
		// compute X^TWX
		for (i=0; i<d; i++)
			gsl_matrix_set_col(WX, i, W);
		gsl_matrix_mul_elements(WX, X);
		gsl_blas_dgemm(CblasTrans, CblasNoTrans, 1, X, WX, 0, XTWX);
	
		// compute beta
		gsl_linalg_HH_solve(XTWX, XTWz, beta);
	
		fprintf(stderr, "%d %.4f\n", it, ll);
		if (fabs(ll - lastll) < 0.0001*ll) break;
		lastll = ll;
	}

	gsl_vector_free(yhat);
	gsl_vector_free(p);
	gsl_vector_free(q);
	gsl_vector_free(W);
	gsl_vector_free(Z);
	gsl_vector_free(XTWz);
	gsl_matrix_free(WX);
	gsl_matrix_free(XTWX);
}
