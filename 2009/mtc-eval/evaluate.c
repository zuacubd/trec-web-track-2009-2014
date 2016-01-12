// a stand-alone program for calculating confidence
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_cdf.h>
#include <gsl/gsl_sort.h>
#include <gsl/gsl_sort_vector.h>

#include "trec_eval.h"
#include "common.h"
#include "sysfunc.h"
#include "smart_error.h"

#include "hashtable.h"
#include "def.h"

void evaluate(char *topic);
void deltaAP(int, int, unsigned short int *, unsigned short int *, double *, double *, double *, int);
//double meanAP (int, unsigned short int *, double *);
//double meanP (int, int, unsigned short int *, double *);
//double meanNDCG (int, int, unsigned short int *, double *);
//double meanRprec (int, unsigned short int *, double *);

struct hashtable *topic_t;

int main (int argc, char *argv[])
{
	char qrelsfile[256], prelsfile[256];
	char run1file[256], run2file[256];
	char buf[256], basedir[256];
	int i, j, k, S;
	char *runid[255];
	int dir = 0, pEst = 0;

	// defaults
	strcpy(basedir, ".");
	int varapproxorder = 3;
	int mqt = 0;

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
			dir = 1;
		}
		else if (argv[1][1] == 'p')
		{
			strcpy(prelsfile, argv[2]);
			argc -= 2; argv += 2;
			pEst = 1;
		}
		else if (argv[1][1] == 'r')
		{
			if (argv[1][2] == '1')
			{
				strcpy(run1file, argv[2]);
				argc -= 2; argv += 2;
			}
			else if (argv[1][2] == '2')
			{
				strcpy(run2file, argv[2]);
				argc -= 2; argv += 2;
			}
		}
		else if (argv[1][1] == 'o')
		{
			varapproxorder = atoi(argv[2]);
			argc -= 2; argv += 2;
		}
		else if (argv[1][1] == '5')
		{
			mqt = 1;
			argc--; argv++;
		}
	}

	// init hashtable
	topic_t = create_hashtable(500, topic_hash, topic_keycmp);

	// read in qrels
	ALL_TREC_QRELS qrels;
	if (UNDEF == get_qrels(qrelsfile, &qrels, mqt))
		fprintf(stderr, "could not read qrels\n");

	// insert qrels into hashtable
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
				v->p = qrels.trec_qrels[i].text_qrels[j].rel?1:0;
	
				char *doc_key = (char *)malloc((strlen(qrels.trec_qrels[i].text_qrels[j].docno)+1)*sizeof(char));
				strcpy(doc_key, qrels.trec_qrels[i].text_qrels[j].docno);

				if (!insert_some(t->doc_t, doc_key, v))
					fprintf(stderr, "hash error\n");

			}
		}
		
		t->n = hashtable_count(t->doc_t);
	}

	// read in retrieval systems
	if (dir)
	{
		DIR *dp = opendir(basedir);
		struct dirent *ep;
		for (S=0; ep = readdir(dp); S++)
		{
			runid[S] = (char *)malloc((strlen(ep->d_name)+strlen(basedir)+2)*sizeof(char));
			sprintf(runid[S], "%s/%s", basedir, ep->d_name);
		}
	}
	else
	{
		runid[0] = (char *)malloc((strlen(run1file)+1)*sizeof(char));
		strcpy(runid[0], run1file);
		runid[1] = (char *)malloc((strlen(run2file)+1)*sizeof(char));
		strcpy(runid[1], run2file);
		S = 2;
	}

	for (k=0; k<S; k++)
	{
		ALL_TREC_TOP run;
		//sprintf(buf, "%s/%s", basedir, runid[k]);
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
			// skip queries with no judgments
			if (t == NULL) continue;

			t->runid[t->S] = (char *)malloc((strlen(run.run_id)+1)*sizeof(char));
			strcpy(t->runid[t->S], run.run_id);
			// rewrite runid
			runid[t->S] = (char *)realloc(runid[t->S], (strlen(run.run_id)+1)*sizeof(char));
			strcpy(runid[t->S], run.run_id);

			t->ranklen[t->S] = MIN(MAXRANK, run.trec_top[i].num_text_tr);
			t->ranklen[t->S] = MAXRANK;
			t->ranking[t->S] = (unsigned short *)malloc(t->ranklen[t->S]*sizeof(unsigned short));

			for (j=0; j<run.trec_top[i].num_text_tr && j<MAXRANK; j++)
			{
				struct doc_value *v;
				//fprintf(stderr, "%s %s %s\n", runid[t->S], run.trec_top[i].qid, run.trec_top[i].text_tr[j].docno);
				v = (struct doc_value *)search_some(t->doc_t, run.trec_top[i].text_tr[j].docno);
				if (v != NULL)
				{
					t->ranking[t->S][j] = v->index;
					continue;
				}

				v = (struct doc_value *)malloc(sizeof(struct doc_value));
				v->index = hashtable_count(t->doc_t);
				v->p = 0.5;

				t->ranking[t->S][j] = v->index;

				char *doc_key = (char *)malloc((strlen(run.trec_top[i].text_tr[j].docno)+1)*sizeof(char));
				strcpy(doc_key, run.trec_top[i].text_tr[j].docno);

				if (!insert_some(t->doc_t, doc_key, v))
					fprintf(stderr, "hash error\n");

			}
			// if there are fewer than MAXRANK docs, need to pad the list out
			// "hack" for confidence calculation
			for (; j<MAXRANK; j++)
			{
				char *nodoc;
				nodoc = (char *)malloc(sizeof(char)*255);
				sprintf(nodoc, "NODOC.%s.%s.%d", runid[k], run.trec_top[i].qid, j);
				struct doc_value *v = (struct doc_value *)malloc(sizeof(struct doc_value));
				v->index = hashtable_count(t->doc_t);
				v->p = 0.5;
				t->ranking[t->S][j] = v->index;
				if (!insert_some(t->doc_t, nodoc, v))
					fprintf(stderr, "hash error\n");
			}
			
			t->n = hashtable_count(t->doc_t);
			t->S++;
		}

		delete_top(&run);
	}

	if (pEst)
	{
		// user specified some prels.  read those in too.
		ALL_TREC_PRELS prels;
		if (UNDEF == get_prels(prelsfile, &prels))
			fprintf(stderr, "could not read prels\n");

		// insert judgments into hashtable
		for (i=0; i<prels.num_q_prels; i++)
		{
			_topic *t = (_topic *)search_topic(topic_t, prels.trec_prels[i].qid);
			if (t == NULL) continue;
	
			for (j=0; j<prels.trec_prels[i].num_text_prels; j++)
			{
				struct doc_value *v;
				if ((v = hashtable_search(t->doc_t, prels.trec_prels[i].text_prels[j].docno)) == NULL)
				{
					v = (struct doc_value *)malloc(sizeof(struct doc_value));
					v->index = hashtable_count(t->doc_t);
					v->p = 0.5;
		
					char *doc_key = (char *)malloc((strlen(prels.trec_prels[i].text_prels[j].docno)+1)*sizeof(char));
					strcpy(doc_key, prels.trec_prels[i].text_prels[j].docno);
	
					if (!insert_some(t->doc_t, doc_key, v))
						fprintf(stderr, "hash error\n");
				}

				if (v->p == 0.5)
					v->p = prels.trec_prels[i].text_prels[j].rel;
	
				// don't reset probabilities of judged documents
				//if (t->users[0]->p[v->index] == 0.5)
					//t->users[0]->p[v->index] = prels.trec_prels[i].text_prels[j].rel;
			}

			t->n = hashtable_count(t->doc_t);
		}

		delete_prels(&prels);
	}

	struct hashtable_itr *itr = (struct hashtable_itr *)hashtable_iterator(topic_t);
	do {
		_topic *t = (_topic *)hashtable_iterator_value(itr);
		t->users[0]->rel = gsl_vector_calloc(t->n);
		t->users[0]->p = (double *)malloc(t->n*sizeof(double));
		for (i=0; i<t->n; i++) t->users[0]->p[i] = 0.5;

		t->users[0]->eap = (double *)malloc(t->S*sizeof(double));
		t->users[0]->eRprec = (double *)malloc(t->S*sizeof(double));
		t->users[0]->ep10 = (double *)malloc(t->S*sizeof(double));
		t->users[0]->ep30 = (double *)malloc(t->S*sizeof(double));
		t->users[0]->ep50 = (double *)malloc(t->S*sizeof(double));
		t->users[0]->endcg10 = (double *)malloc(t->S*sizeof(double));
		t->users[0]->endcg30 = (double *)malloc(t->S*sizeof(double));
		t->users[0]->endcg50 = (double *)malloc(t->S*sizeof(double));
		for (i=0; i<t->S; i++) 
		{
			t->users[0]->eap[i] = 0;
			t->users[0]->eRprec[i] = 0;
			t->users[0]->ep10[i] = 0;
			t->users[0]->ep30[i] = 0;
			t->users[0]->ep50[i] = 0;
			t->users[0]->endcg10[i] = 0;
			t->users[0]->endcg30[i] = 0;
			t->users[0]->endcg50[i] = 0;
		}

		struct hashtable_itr *itr2 = (struct hashtable_itr *)hashtable_iterator(t->doc_t);
		do {
			struct doc_value *v = (struct doc_value *)hashtable_iterator_value(itr2);
			t->users[0]->p[v->index] = v->p;
			if (v->p == 1 || v->p == 0)
				gsl_vector_set(t->users[0]->rel, v->index, (int)v->p);
		} while (hashtable_iterator_advance(itr2));
		free(itr2);
	} while (hashtable_iterator_advance(itr));
	free(itr);
	
	// insert judgments into hashtable
	/*
	for (i=0; i<qrels.num_q_qrels; i++)
	{
		_topic *t = (_topic *)search_topic(topic_t, qrels.trec_qrels[i].qid);
		if (t == NULL) continue;

		for (j=0; j<qrels.trec_qrels[i].num_text_qrels; j++)
		{
			struct doc_value *v;
			if ((v = hashtable_search(t->doc_t, qrels.trec_qrels[i].text_qrels[j].docno)) == NULL)
				continue;

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
	*/

	delete_qrels(&qrels);

	// now evaluate
	double edeltamap[S][S];
	double vdeltamap[S][S];
	double emap[S], eRprec[S], ep10[S], ep30[S], ep50[S], endcg10[S], endcg30[S], endcg50[S];

	for (i=0; i<S; i++)
	{
		emap[i] = 0;
		eRprec[i] = 0;
		ep10[i] = 0; ep30[i] = 0; ep50[i] = 0;
		endcg10[i] = 0; endcg30[i] = 0; endcg50[i] = 0;

		for (j=0; j<S; j++)
		{
			edeltamap[i][j] = 0;
			vdeltamap[i][j] = 0;
		}
	}

	double numtopics = (double)hashtable_count(topic_t);
	fprintf(stderr, "%.2f topics\n", numtopics);

	fprintf(stderr, "topic run1 run2 deltaAP.mean deltaAP.var confidence\n");
	itr = (struct hashtable_itr *)hashtable_iterator(topic_t);
	do {
		_topic *t = (_topic *)hashtable_iterator_value(itr);

		double R = 0;
		for (i=0; i<t->n; i++)
			R += t->users[0]->p[i];
		if (R == 0) R = 1.0;

		for (i=0; i<t->S; i++)
		{
			t->users[0]->eap[i] = meanAP(t->ranklen[i], t->ranking[i], t->users[0]->p)/R;
			t->users[0]->eRprec[i] = meanRprec(t->ranklen[i], R, t->ranking[i], t->users[0]->p);
			t->users[0]->ep10[i] = meanP(t->ranklen[i], 10, t->ranking[i], t->users[0]->p);
			t->users[0]->ep30[i] = meanP(t->ranklen[i], 30, t->ranking[i], t->users[0]->p);
			t->users[0]->ep50[i] = meanP(t->ranklen[i], 50, t->ranking[i], t->users[0]->p);
			t->users[0]->endcg10[i] = meanNDCG(t->ranklen[i], 10, t->ranking[i], t->users[0]->p, t->n);
			t->users[0]->endcg30[i] = meanNDCG(t->ranklen[i], 30, t->ranking[i], t->users[0]->p, t->n);
			t->users[0]->endcg50[i] = meanNDCG(t->ranklen[i], 50, t->ranking[i], t->users[0]->p, t->n);

			emap[i] += t->users[0]->eap[i];
			eRprec[i] += t->users[0]->eRprec[i];
			ep10[i] += t->users[0]->ep10[i];
			ep30[i] += t->users[0]->ep30[i];
			ep50[i] += t->users[0]->ep50[i];
			endcg10[i] += t->users[0]->endcg10[i];
			endcg30[i] += t->users[0]->endcg30[i];
			endcg50[i] += t->users[0]->endcg50[i];

			fprintf(stderr, "%s\t%s\t%.2f\t%.4f\t%.4f\t%.4f\t%.4f\n", t->runid[i], (char *)hashtable_iterator_key(itr), R, t->users[0]->eap[i], t->users[0]->eRprec[i], t->users[0]->ep10[i], t->users[0]->endcg10[i]);
		}
		
		int o_eap[t->S];
		gsl_sort_index(o_eap, t->users[0]->eap, 1, t->S);

		for (i=0; i<t->S; i++)
		{
			for (j=i+1; j<t->S; j++)
			{
				double mean = 0, var = 0;
				deltaAP(t->ranklen[o_eap[i]], t->ranklen[o_eap[j]], t->ranking[o_eap[i]], t->ranking[o_eap[j]], t->users[0]->p, &mean, &var, varapproxorder);
				fprintf(stderr, "%s %s %s %.6f %.6f %.6f %.6f\n", (char *)hashtable_iterator_key(itr), t->runid[o_eap[i]], t->runid[o_eap[j]], mean, var, R, gsl_cdf_gaussian_P(-mean/R, sqrt(var)/R));
				edeltamap[o_eap[i]][o_eap[j]] += mean/R;
				edeltamap[o_eap[j]][o_eap[i]] -= mean/R;
				vdeltamap[o_eap[i]][o_eap[j]] += var/(R*R);
				vdeltamap[o_eap[j]][o_eap[i]] += var/(R*R);
			}
		}
	} while (hashtable_iterator_advance(itr));
	free(itr);

	int o_emap[S];
	gsl_sort_index(o_emap, emap, 1, S);

	printf("run eMAP eRprec eP10 eP30 eP50 eNDCG10 eNDCG30 eNDCG50\n");
	for (i=0; i<S; i++)
		printf("%s %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f\n", runid[o_emap[i]], emap[o_emap[i]]/numtopics,
		eRprec[o_emap[i]]/numtopics,
		ep10[o_emap[i]]/numtopics,
		ep30[o_emap[i]]/numtopics,
		ep50[o_emap[i]]/numtopics,
		endcg10[o_emap[i]]/numtopics,
		endcg30[o_emap[i]]/numtopics,
		endcg50[o_emap[i]]/numtopics);

	printf("\n");

	printf("\t");
	for (j=1; j<S; j++)
		printf("%s\t", runid[o_emap[j]]);
	printf("\n");

	for (i=0; i<S-1; i++)
	{
		printf("%s\t", runid[o_emap[i]]);
		for (j=0; j<i; j++)
			printf("\t");
		for (j=i+1; j<S; j++)
			printf("%.4f\t", edeltamap[o_emap[i]][o_emap[j]]/numtopics);
		printf("\n");

		for (j=0; j<=i; j++)
			printf("\t");
		for (j=i+1; j<S; j++)
			printf("%.4f\t", vdeltamap[o_emap[i]][o_emap[j]]/(numtopics*numtopics));
		printf("\n");

		for (j=0; j<=i; j++)
			printf("\t");
		for (j=i+1; j<S; j++)
			printf("%.4f\t", gsl_cdf_gaussian_P(-edeltamap[o_emap[i]][o_emap[j]], sqrt(vdeltamap[o_emap[i]][o_emap[j]])));
		printf("\n");
	}
			//fprintf(stderr, "%s %s %.4f %.4f %.4f\n", runid[o_emap[i]], runid[o_emap[j]], edeltamap[o_emap[i]][o_emap[j]], vdeltamap[o_emap[i]][o_emap[j]], gsl_cdf_gaussian_P(-edeltamap[o_emap[i]][o_emap[j]], sqrt(vdeltamap[o_emap[i]][o_emap[j]])));

}

/*
int cmp (const void *a, const void *b)
{
	int *ia = (int *)a;
	int *ib = (int *)b;
	return (*ia > *ib) - (*ia < *ib);
}

// there's got to be a better way
void deltaAP(int n1, int n2, unsigned short int *r1, unsigned short int *r2, double *p, double *mu, double *var)
{
	int i, j, k, z;
	int o1[n1], o2[n2];
	int r1_2[n1+1], r2_2[n2+1];

	gsl_sort_ushort_index(o1, r1, 1, n1);
	gsl_sort_ushort_index(o2, r2, 1, n2);

	for (i=0; i<n1; i++)
		r1_2[i] = r1[i];
	for (i=0; i<n2; i++)
		r2_2[i] = r2[i];
	r1_2[n1] = 100000;
	r2_2[n2] = 100000;

	qsort(r1_2, n1, sizeof(int), cmp);
	qsort(r2_2, n2, sizeof(int), cmp);

	double r1_l[n1+n2]; 
	double r2_l[n1+n2]; 
	double p_2[n1+n2];
	for (i=0; i<n1+n2; i++) { r1_l[i] = 1e15; r2_l[i] = 1e15; p_2[i] = 0; }

	// if this was R, i would do:
	// d <- which(!is.na(r1) | !is.na(r2))
	// r1_l <- r1[d]; r2_l <- r2[d]; p_2 <- p[d]
	// but it's not R, so instead I have to do all this...
	for (i=0, j=0, k=0; i<n1 || j<n2; )
	{
		for (; r1_2[i] < r2_2[j] && i<n1; i++, k++)
		{
			r1_l[k] = o1[i]+1;
			p_2[k] = p[r1_2[i]];
		}
		for (; r2_2[j] < r1_2[i] && j<n2; j++, k++)
		{
			r2_l[k] = o2[j]+1;
			p_2[k] = p[r2_2[j]];
		}
		if (r1_2[i] == r2_2[j] && i < n1 && j < n2)
		{
			r1_l[k] = o1[i]+1;
			r2_l[k] = o2[j]+1;
			p_2[k] = p[r1_2[i]];
			i++; j++; k++;
		}
	}
	z = k;

	// debug:  verify r1_l and r2_l
	//for (i=0; i<n1+n2; i++)
		//fprintf(stderr, "%d %.0f %.0f\n", i, r1_l[i], r2_l[i]);

	double **c = (double **)malloc(z*sizeof(double *));
	for (i=0; i<z; i++)
		c[i] = (double *)malloc(z*sizeof(double));

	double *rowmax = (double *)calloc(z, sizeof(double));
	double *colmax = (double *)calloc(z, sizeof(double));

	for (i=0; i<z; i++)
	{
		c[i][i] = 1.0/r1_l[i] - 1.0/r2_l[i];
		rowmax[i] = 0;

		*mu += c[i][i]*p_2[i];

		for (j=i+1; j<z; j++)
		{
			c[i][j] = 1.0/MAX(r1_l[i], r1_l[j]) - 1.0/MAX(r2_l[i], r2_l[j]);
			c[j][i] = c[i][j];

			if (fabs(c[i][j]*p_2[j]) > rowmax[i]) 
				rowmax[i] = fabs(c[i][j]*p_2[j]);
			if (fabs(c[j][i]*p_2[i]) > colmax[j]) 
				colmax[j] = fabs(c[j][i]*p_2[i]);

			*mu += c[i][j]*p_2[i]*p_2[j];
		}
	}

	double c_ii, p_i, c_ij, p_ij, p_j, p_ji;
	for (i=0; i<z; i++)
	{
		if (p_2[i] == 0) continue;
		c_ii = c[i][i];
		p_i = p_2[i]*(1-p_2[i]);
		*var += c_ii*c_ii*p_i;
		for (j=i+1; j<z; j++)
		{
			if (p_2[j] == 0) continue;
			c_ij = c[i][j];
			p_ij = p_i*p_2[j];
			p_j = p_2[j]*(1-p_2[j]);
			p_ji = p_j*p_2[i];
			*var += c_ij*c_ij*p_2[i]*p_2[j]*(1-p_2[i]*p_2[j]);
			*var += 2*c_ii*c_ij*p_ij;
			*var += 2*c[j][j]*c_ij*p_ji;

			if (varapproxorder == 2)
			{
				*var += (z-j)*2*rowmax[i]*c_ij*p_ij;
				*var += (z-j)*2*colmax[j]*c_ij*p_ji;
				*var += (z-j)*2*rowmax[i]*colmax[j]*MAX(p_2[i]*(1-p_2[i]), p_2[j]*(1-p_2[j]));
			}
			else
			{
				for (k=j+1; k<z; k++)
					if (p_2[k] != 0)
					{
						*var += 2*c[i][k]*c_ij*p_2[k]*p_ij;
						*var += 2*c[j][k]*c_ij*p_2[k]*p_ji;
						*var += 2*c[k][j]*c[k][i]*p_2[k]*p_2[i]*p_2[j]*(1-p_2[k]);
					}
			}
		}
	}

	free(rowmax);
	free(colmax);
	for (i=0; i<z; i++)
		free(c[i]);
	free(c);
}

double meanAP (int n, unsigned short int *r, double *p)
{
	int i;
	double relret = 0;
	double ap = 0;

	for (i=0; i<n; i++)
	{
		ap += (p[r[i]] + p[r[i]]*relret)/((double)i+1.0);
		relret += p[r[i]];
	}

	return ap;
}
*/
