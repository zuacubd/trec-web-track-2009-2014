#include "def.h"

int cmp (const void *a, const void *b)
{
	int *ia = (int *)a;
	int *ib = (int *)b;
	return (*ia > *ib) - (*ia < *ib);
}

int cmpd (const void *a, const void *b)
{
	double *ia = (double *)a;
	double *ib = (double *)b;
	return (*ia > *ib) - (*ia < *ib);
}

void deltaAP(int n1, int n2, unsigned short int *r1, unsigned short int *r2, double *p, double *mu, double *var, int varapproxorder)
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

double meanAP (int n, unsigned short *r, double *p)
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

double meanP (int n, int k, unsigned short *r, double *p)
{
	int i;
	double prec = 0.0;

	for (i=0; i<k; i++)
	{
		prec += p[r[i]]/((double)k);
	}

	return prec;
}

double meanNDCG (int n, int k, unsigned short *r, double *p, int N)
{
	int i, j=0;
	double dcg = 0.0, denom = 0.0;
	double p2[N];

	for (i=0; i<N; i++)
		p2[i] = p[i];

	for (i=0; i<k; i++)
		dcg += 2*p[r[i]]/(log(i+2)/log(2));

	// find the denominator
	qsort(p2, N, sizeof(double), cmpd);
	for (i=N-1; i>=N-k; i--)
	{
		j++;
		denom += 2*p2[i]/(log(j+1)/log(2));
	}

	return dcg/denom;
}

double meanRprec (int n, double R, unsigned short *r, double *p)
{
	int i;
	double Rprec = 0.0;

	// i think i'm going to do this the easy way---estimate R, and estimate precision to R
	for (i=0; i<n && i<R; i++)
		Rprec += p[r[i]]/R;

	return Rprec;
}
