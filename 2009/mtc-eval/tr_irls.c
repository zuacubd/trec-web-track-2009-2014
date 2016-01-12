#include <stdio.h>
#include <math.h>

void tr_irls(int l, double *x, double *beta, double epsilon, double delta)
{
	int i, j, k, it, it2;
	int pos, n;
	double yhat = 0, p, w, z;
	double wz, wy_i, wy_j;
	double rz, pv, rnz, alpha, gamma;
	double ll, dev, olddev = -1000;
	//double *p_epsilon, *p_delta;
	double rnew[l], v[l], q[l], r[l], b[l], *A;
	int irls_done = 0, cg_done = 0;
	//for (i=0; i<l; i++) y[l] = beta[l];
	double *y = beta;

	n = (l+1)*(l+1);
	
	A = (double *)malloc(sizeof(double)*l*l);

	for (it=0; !irls_done; it++)
	{
		ll = 0;
		for (i=0; i<l; i++) 
		{
			rnew[i] = 0;
			v[i] = 0;
			q[i] = 0;
			r[i] = 0;
			b[i] = 0;
			for (j=0; j<l; j++) A[l*i+j] = 0;
		}
		
		for (k=0, i=0, j=0; k<n; k++)
		{
			// compute log odds ratio
			if (i != 0 && j != 0) yhat = y[i-1]-y[j-1];
			else if (i != 0) yhat = y[i-1];
			else if (j != 0) yhat = -y[j-1];
	
			// compute p and z
			p = exp(yhat)/(1+exp(yhat));
			z = yhat + 1/(p==0?1:p);
			ll += x[k]*log(p);

			// precompute products
			w = x[k]*p*(1-p);
			wz = w*z;
			if (i != 0) wy_i = w*y[i-1];
			if (j != 0) wy_j = w*y[j-1];
	
			// compute entries in XtWX
			if (i != 0) A[l*(i-1)+i-1] += w;
			if (j != 0) A[l*(j-1)+j-1] += w;
			if (i != 0 && j != 0) A[l*(i-1)+(j-1)] -= w;
			if (i != 0 && j != 0) A[l*(j-1)+(i-1)] -= w;
	
			// compute entries in XtWZ
			if (i != 0) b[i-1] += wz;
			if (j != 0) b[j-1] -= wz;
	
			// compute residuals
			if (i != 0) 
			{
				r[i-1] += wz;
				r[i-1] -= wy_i;
				q[i-1] += wz;
				q[i-1] -= wy_i;
			}
			if (j != 0) 
			{
				r[j-1] -= wz;
				r[j-1] -= wy_j;
				q[j-1] -= wz;
				q[j-1] -= wy_j;
			}
			if (i != 0 && j != 0) 
			{
				r[i-1] += wy_j;
				q[i-1] += wy_j;
			}
			if (i != 0 && j != 0) 
			{
				r[j-1] += wy_i;
				q[j-1] += wy_i;
			}
			
			// update i and j
			j++;
			if (j == l+1)
			{
				i++;
				j = 0;
			}
		}
	
		// now do gradient descent
		cg_done = 0;
		for (it2=0; !cg_done; it2++)
		{
			rz = 0; pv = 0; rnz = 0;

			for (i=0; i<l; i++)
			{
				v[i] = 0;
				for (j=0; j<l; j++)
				{
					v[i] += A[l*i+j]*q[j];
				}
	
				rz += r[i]*r[i];
				pv += q[i]*v[i];
			}
	
			alpha = rz/pv;
	
			for (i=0; i<l; i++)
			{
				y[i] += alpha*q[i];
				rnew[i] = r[i] - alpha*v[i];
				rnz += rnew[i]*rnew[i];
			}
	
			gamma = rnz/rz;
	
			for (i=0; i<l; i++)
			{
				q[i] = rnew[i] + gamma*q[i];
				r[i] = rnew[i];
			}
	
			if (sqrt(rnz) < epsilon) cg_done = 1;
		}

		dev = -2*ll;
		if (fabs(dev - olddev) < delta*dev) irls_done = 1;
		olddev = dev;
	}

	free(A);
}
