// Oscar Steila ik1xpv 2017 - ik1xpv<at>gmail.com
// RFddc.cpp 
// converts the real input stream at 64Msps to 16Msps, 8Msps, 4Msps , 2Msps Complex I&Q

#include "rfddc.h"
#include "mutex"
#include "ExtIO_sddc.h"


// ht filters
#include "Ht_257_64M_7200.h"
#include "Ht_257_64M_3400.h"
#include "Ht_257_64M_1500.h"
#include "Ht_257_64M_0600.h"

// rfddc signal buffers
static fftwf_complex  tAdcCplx[NVIA][WLENXBUF + 1][FFTN];  // time real ADC sequence float complex, im = 0
static fftwf_complex  fAdcCplx[NVIA][WLENXBUF + 1][FFTN];  // frequency ADC sequence complex 
static fftwf_complex  fDecCplx[NVIA][(WLENXBUF + 1)*FFTN]; // frequency decimated sequence complex 
static fftwf_complex  tOutCplx[NVIA][(WLENXBUF + 1)*FFTN]; // time decimated sequence
static fftwf_plan	  planT2F[NVIA][WLENXBUF + 1];  // fftw plan buffers 
static fftwf_plan     planF2T[NVIA][WLENXBUF + 1];  // fftw plan buffers 
static fftwf_complex  Hwfilter[FFTN];			// LPB filter Hw
static fftwf_complex  outbufrs[NVIA][DFRAMEN];	// time decimated output buffers

// global 
int gtunebin;	// global tune bin
bool gR820Ton;  // R820T2 flag reference

void f_thread(rfdata& mdata, int n_via)
{
	int nvia = n_via;
	while (!mdata.isdying)
	{
		std::unique_lock<std::mutex> lock(mdata.mut[nvia]);
		if ((mdata.iipath == nvia) && (mdata.initialized))
		{
			lock.unlock();
			int d_fftn = mdata.d_fftn;
			fftwf_complex * pouttime;
			pouttime = outbufrs[nvia];
			short * tAdcReal = (short *) mdata.pbufin[nvia];
// deliver past buffer
			memcpy( pouttime, outbufrs[nvia], sizeof(fftwf_complex)* d_fftn * FFTXBUF);
// compute nvia path 
			for (int m = 0; m < WLENXBUF + 1; m++)
			{
				for (int j = 0; j < WLEN; )
				{
					tAdcCplx[nvia][m][j][0] = tAdcReal[m*WLEN + j++] * mdata.gainscale;
					tAdcCplx[nvia][m][j][0] = tAdcReal[m*WLEN + j++] * mdata.gainscale;
					tAdcCplx[nvia][m][j][0] = tAdcReal[m*WLEN + j++] * mdata.gainscale;
					tAdcCplx[nvia][m][j][0] = tAdcReal[m*WLEN + j++] * mdata.gainscale;
					tAdcCplx[nvia][m][j][0] = tAdcReal[m*WLEN + j++] * mdata.gainscale;
					tAdcCplx[nvia][m][j][0] = tAdcReal[m*WLEN + j++] * mdata.gainscale;
					tAdcCplx[nvia][m][j][0] = tAdcReal[m*WLEN + j++] * mdata.gainscale;
					tAdcCplx[nvia][m][j][0] = tAdcReal[m*WLEN + j++] * mdata.gainscale;
				}
				fftwf_execute(planT2F[nvia][m]);
				int md = m * d_fftn;
				int j,s;
				switch (gtunebin & 3)
				{
				case 0:
				{
					for (int k = 0; k < (d_fftn / 2); k++)
					{
						j = (gtunebin + k) % FFTN;
						fDecCplx[nvia][md + k][0] = (fAdcCplx[nvia][m][j][0] * Hwfilter[k][0] - fAdcCplx[nvia][m][j][1] * Hwfilter[k][1]);
						fDecCplx[nvia][md + k][1] = (fAdcCplx[nvia][m][j][1] * Hwfilter[k][0] + fAdcCplx[nvia][m][j][0] * Hwfilter[k][1]);
					}

					for (int k = (d_fftn / 2); k < d_fftn; k++)
					{
						j = (gtunebin - d_fftn + k + FFTN ) % FFTN;
						s = FFTN - d_fftn + k;
						fDecCplx[nvia][md + k][0] = (fAdcCplx[nvia][m][j][0] * Hwfilter[s][0] - fAdcCplx[nvia][m][j][1] * Hwfilter[s][1]);
						fDecCplx[nvia][md + k][1] = (fAdcCplx[nvia][m][j][1] * Hwfilter[s][0] + fAdcCplx[nvia][m][j][0] * Hwfilter[s][1]);
					}
				}
				break;
				case 1:
					switch (m & 3)
					{
					case 3:
					{
						for (int k = 0; k < (d_fftn / 2); k++)
						{
							j = (gtunebin + k) % FFTN;
							fDecCplx[nvia][md + k][1] = (fAdcCplx[nvia][m][j][0] * Hwfilter[k][0] - fAdcCplx[nvia][m][j][1] * Hwfilter[k][1]);
							fDecCplx[nvia][md + k][0] = -(fAdcCplx[nvia][m][j][1] * Hwfilter[k][0] + fAdcCplx[nvia][m][j][0] * Hwfilter[k][1]);
						}

						for (int k = (d_fftn / 2); k < d_fftn; k++)
						{
							j = (gtunebin - d_fftn + k + FFTN ) % FFTN;
							s = FFTN - d_fftn + k;
							fDecCplx[nvia][md + k][1] = (fAdcCplx[nvia][m][j][0] * Hwfilter[s][0] - fAdcCplx[nvia][m][j][1] * Hwfilter[s][1]);
							fDecCplx[nvia][md + k][0] = -(fAdcCplx[nvia][m][j][1] * Hwfilter[s][0] + fAdcCplx[nvia][m][j][0] * Hwfilter[s][1]);
						}
					}
					break;
					case 2:
					{
						for (int k = 0; k < (d_fftn / 2); k++)
						{
							j = (gtunebin + k) % FFTN;
							fDecCplx[nvia][md + k][0] = (fAdcCplx[nvia][m][j][0] * Hwfilter[k][0] - fAdcCplx[nvia][m][j][1] * Hwfilter[k][1]);
							fDecCplx[nvia][md + k][1] = (fAdcCplx[nvia][m][j][1] * Hwfilter[k][0] + fAdcCplx[nvia][m][j][0] * Hwfilter[k][1]);
						}

						for (int k = (d_fftn / 2); k < d_fftn; k++)
						{
							j = (gtunebin - d_fftn + k + FFTN ) % FFTN;
							s = FFTN - d_fftn + k;
							fDecCplx[nvia][md + k][0] = (fAdcCplx[nvia][m][j][0] * Hwfilter[s][0] - fAdcCplx[nvia][m][j][1] * Hwfilter[s][1]);
							fDecCplx[nvia][md + k][1] = (fAdcCplx[nvia][m][j][1] * Hwfilter[s][0] + fAdcCplx[nvia][m][j][0] * Hwfilter[s][1]);
						}
					}
					break;
					case 1:
					{
						for (int k = 0; k < (d_fftn / 2); k++)
						{
							j = (gtunebin + k) % FFTN;
							fDecCplx[nvia][md + k][1] = -(fAdcCplx[nvia][m][j][0] * Hwfilter[k][0] - fAdcCplx[nvia][m][j][1] * Hwfilter[k][1]);
							fDecCplx[nvia][md + k][0] = (fAdcCplx[nvia][m][j][1] * Hwfilter[k][0] + fAdcCplx[nvia][m][j][0] * Hwfilter[k][1]);
						}

						for (int k = (d_fftn / 2); k < d_fftn; k++)
						{
							j = (gtunebin - d_fftn + k + FFTN ) % FFTN;
							s = FFTN - d_fftn + k;
							fDecCplx[nvia][md + k][1] = -(fAdcCplx[nvia][m][j][0] * Hwfilter[s][0] - fAdcCplx[nvia][m][j][1] * Hwfilter[s][1]);
							fDecCplx[nvia][md + k][0] = (fAdcCplx[nvia][m][j][1] * Hwfilter[s][0] + fAdcCplx[nvia][m][j][0] * Hwfilter[s][1]);
						}
					}
					break;
					case 0:
					{
						for (int k = 0; k < (d_fftn / 2); k++)
						{
							j = (gtunebin + k) % FFTN;
							fDecCplx[nvia][md + k][0] = -(fAdcCplx[nvia][m][j][0] * Hwfilter[k][0] - fAdcCplx[nvia][m][j][1] * Hwfilter[k][1]);
							fDecCplx[nvia][md + k][1] = -(fAdcCplx[nvia][m][j][1] * Hwfilter[k][0] + fAdcCplx[nvia][m][j][0] * Hwfilter[k][1]);
						}
						for (int k = (d_fftn / 2); k < d_fftn; k++)
						{
							j = (gtunebin - d_fftn + k + FFTN) % FFTN;
							s = FFTN - d_fftn + k;
							fDecCplx[nvia][md + k][0] = -(fAdcCplx[nvia][m][j][0] * Hwfilter[s][0] - fAdcCplx[nvia][m][j][1] * Hwfilter[s][1]);
							fDecCplx[nvia][md + k][1] = -(fAdcCplx[nvia][m][j][1] * Hwfilter[s][0] + fAdcCplx[nvia][m][j][0] * Hwfilter[s][1]);
						}
					}
					break;
					}
					break;
				case 2:
				{
					if (m % 2 == 0)
					{
						for (int k = 0; k < (d_fftn / 2); k++)
						{
							j = (gtunebin + k) % FFTN;
							fDecCplx[nvia][md + k][0] = (fAdcCplx[nvia][m][j][0] * Hwfilter[k][0] - fAdcCplx[nvia][m][j][1] * Hwfilter[k][1]);
							fDecCplx[nvia][md + k][1] = (fAdcCplx[nvia][m][j][1] * Hwfilter[k][0] + fAdcCplx[nvia][m][j][0] * Hwfilter[k][1]);
						}

						for (int k = (d_fftn / 2); k < d_fftn; k++)
						{
							j = (gtunebin - d_fftn + k + FFTN) % FFTN;
							s = FFTN - d_fftn + k;
							fDecCplx[nvia][md + k][0] = (fAdcCplx[nvia][m][j][0] * Hwfilter[s][0] - fAdcCplx[nvia][m][j][1] * Hwfilter[s][1]);
							fDecCplx[nvia][md + k][1] = (fAdcCplx[nvia][m][j][1] * Hwfilter[s][0] + fAdcCplx[nvia][m][j][0] * Hwfilter[s][1]);
						}
					}
					else
					{
						for (int k = 0; k < (d_fftn / 2); k++)
						{
							j = (gtunebin + k) % FFTN;
							fDecCplx[nvia][md + k][0] = -(fAdcCplx[nvia][m][j][0] * Hwfilter[k][0] - fAdcCplx[nvia][m][j][1] * Hwfilter[k][1]);
							fDecCplx[nvia][md + k][1] = -(fAdcCplx[nvia][m][j][1] * Hwfilter[k][0] + fAdcCplx[nvia][m][j][0] * Hwfilter[k][1]);
						}

						for (int k = (d_fftn / 2); k < d_fftn; k++)
						{
							j = (gtunebin - d_fftn + k + FFTN) % FFTN;
							s = FFTN - d_fftn + k;
							fDecCplx[nvia][md + k][0] = -(fAdcCplx[nvia][m][j][0] * Hwfilter[s][0] - fAdcCplx[nvia][m][j][1] * Hwfilter[s][1]);
							fDecCplx[nvia][md + k][1] = -(fAdcCplx[nvia][m][j][1] * Hwfilter[s][0] + fAdcCplx[nvia][m][j][0] * Hwfilter[s][1]);
						}
					}
				}
				break;
				case 3:
					switch (m & 3)
					{
					case 3:
					{
						for (int k = 0; k < (d_fftn / 2); k++)
						{
							j = (gtunebin + k) % FFTN;
							fDecCplx[nvia][md + k][1] = (fAdcCplx[nvia][m][j][0] * Hwfilter[k][0] - fAdcCplx[nvia][m][j][1] * Hwfilter[k][1]);
							fDecCplx[nvia][md + k][0] = -(fAdcCplx[nvia][m][j][1] * Hwfilter[k][0] + fAdcCplx[nvia][m][j][0] * Hwfilter[k][1]);
						}

						for (int k = (d_fftn / 2); k < d_fftn; k++)
						{
							j = (gtunebin - d_fftn + k + FFTN) % FFTN;
							s = FFTN - d_fftn + k;
							fDecCplx[nvia][md + k][1] = (fAdcCplx[nvia][m][j][0] * Hwfilter[s][0] - fAdcCplx[nvia][m][j][1] * Hwfilter[s][1]);
							fDecCplx[nvia][md + k][0] = -(fAdcCplx[nvia][m][j][1] * Hwfilter[s][0] + fAdcCplx[nvia][m][j][0] * Hwfilter[s][1]);
						}
					}
					break;
					case 2:
					{
						for (int k = 0; k < (d_fftn / 2); k++)
						{
							j = (gtunebin + k) % FFTN;
							fDecCplx[nvia][md + k][0] = -(fAdcCplx[nvia][m][j][0] * Hwfilter[k][0] - fAdcCplx[nvia][m][j][1] * Hwfilter[k][1]);
							fDecCplx[nvia][md + k][1] = -(fAdcCplx[nvia][m][j][1] * Hwfilter[k][0] + fAdcCplx[nvia][m][j][0] * Hwfilter[k][1]);
						}

						for (int k = (d_fftn / 2); k < d_fftn; k++)
						{
							j = (gtunebin - d_fftn + k + FFTN) % FFTN;
							s = FFTN - d_fftn + k;
							fDecCplx[nvia][md + k][0] = -(fAdcCplx[nvia][m][j][0] * Hwfilter[s][0] - fAdcCplx[nvia][m][j][1] * Hwfilter[s][1]);
							fDecCplx[nvia][md + k][1] = -(fAdcCplx[nvia][m][j][1] * Hwfilter[s][0] + fAdcCplx[nvia][m][j][0] * Hwfilter[s][1]);
						}
					}
					break;
					case 1:
					{
						for (int k = 0; k < (d_fftn / 2); k++)
						{
							j = (gtunebin + k) % FFTN;
							fDecCplx[nvia][md + k][1] = -(fAdcCplx[nvia][m][j][0] * Hwfilter[k][0] - fAdcCplx[nvia][m][j][1] * Hwfilter[k][1]);
							fDecCplx[nvia][md + k][0] = (fAdcCplx[nvia][m][j][1] * Hwfilter[k][0] + fAdcCplx[nvia][m][j][0] * Hwfilter[k][1]);
						}

						for (int k = (d_fftn / 2); k < d_fftn; k++)
						{
							j = (gtunebin - d_fftn + k + FFTN) % FFTN;
							s = FFTN - d_fftn + k;
							fDecCplx[nvia][md + k][1] = -(fAdcCplx[nvia][m][j][0] * Hwfilter[s][0] - fAdcCplx[nvia][m][j][1] * Hwfilter[s][1]);
							fDecCplx[nvia][md + k][0] = (fAdcCplx[nvia][m][j][1] * Hwfilter[s][0] + fAdcCplx[nvia][m][j][0] * Hwfilter[s][1]);
						}
					}
					break;
					case 0:
					{
						for (int k = 0; k < (d_fftn / 2); k++)
						{
							j = (gtunebin + k) % FFTN;
							fDecCplx[nvia][md + k][0] = (fAdcCplx[nvia][m][j][0] * Hwfilter[k][0] - fAdcCplx[nvia][m][j][1] * Hwfilter[k][1]);
							fDecCplx[nvia][md + k][1] = (fAdcCplx[nvia][m][j][1] * Hwfilter[k][0] + fAdcCplx[nvia][m][j][0] * Hwfilter[k][1]);
						}
						for (int k = (d_fftn / 2); k < d_fftn; k++)
						{
							j = (gtunebin - d_fftn + k + FFTN) % FFTN;
							s = FFTN - d_fftn + k;
							fDecCplx[nvia][md + k][0] = (fAdcCplx[nvia][m][j][0] * Hwfilter[s][0] - fAdcCplx[nvia][m][j][1] * Hwfilter[s][1]);
							fDecCplx[nvia][md + k][1] = (fAdcCplx[nvia][m][j][1] * Hwfilter[s][0] + fAdcCplx[nvia][m][j][0] * Hwfilter[s][1]);
						}
					}
					break;
					}
					break;
				}
				fftwf_execute(planF2T[nvia][m]);
			}
// generate output
			memset(outbufrs[nvia], 0, sizeof(fftwf_complex) * mdata.d_fftn * FFTXBUF);
			int d_fftn4 = mdata.d_fftn / 4;
			int wlen = 3 * d_fftn4;
			int mwlen;
			int md_fftn;
			int m,j;

			if (gR820Ton == false)
			{
				for ( m = 0; m < WLENXBUF + 1; m++)
				{
					mwlen = m*wlen - d_fftn4;
					md_fftn = m * mdata.d_fftn;
					if (m != 0)
					{
						for (j = 0; j < d_fftn4; )
						{
							pouttime[mwlen + j][0] += tOutCplx[nvia][md_fftn + j][0];
							pouttime[mwlen + j][1] += tOutCplx[nvia][md_fftn + j++][1];
							pouttime[mwlen + j][0] += tOutCplx[nvia][md_fftn + j][0];
							pouttime[mwlen + j][1] += tOutCplx[nvia][md_fftn + j++][1];
							pouttime[mwlen + j][0] += tOutCplx[nvia][md_fftn + j][0];
							pouttime[mwlen + j][1] += tOutCplx[nvia][md_fftn + j++][1];
							pouttime[mwlen + j][0] += tOutCplx[nvia][md_fftn + j][0];
							pouttime[mwlen + j][1] += tOutCplx[nvia][md_fftn + j++][1];
						}
					}
					for (j = d_fftn4; j < 2 * d_fftn4;)
					{
						pouttime[mwlen + j][0] = tOutCplx[nvia][md_fftn + j][0];
						pouttime[mwlen + j][1] = tOutCplx[nvia][md_fftn + j++][1];
						pouttime[mwlen + j][0] = tOutCplx[nvia][md_fftn + j][0];
						pouttime[mwlen + j][1] = tOutCplx[nvia][md_fftn + j++][1];
						pouttime[mwlen + j][0] = tOutCplx[nvia][md_fftn + j][0];
						pouttime[mwlen + j][1] = tOutCplx[nvia][md_fftn + j++][1];
						pouttime[mwlen + j][0] = tOutCplx[nvia][md_fftn + j][0];
						pouttime[mwlen + j][1] = tOutCplx[nvia][md_fftn + j++][1];
					}
					if (m != WLENXBUF)
					{
						for (j = 2 * d_fftn4; j < 3 * d_fftn4;)
						{
							pouttime[mwlen + j][0] = tOutCplx[nvia][md_fftn + j][0];
							pouttime[mwlen + j][1] = tOutCplx[nvia][md_fftn + j++][1];
							pouttime[mwlen + j][0] = tOutCplx[nvia][md_fftn + j][0];
							pouttime[mwlen + j][1] = tOutCplx[nvia][md_fftn + j++][1];
							pouttime[mwlen + j][0] = tOutCplx[nvia][md_fftn + j][0];
							pouttime[mwlen + j][1] = tOutCplx[nvia][md_fftn + j++][1];
							pouttime[mwlen + j][0] = tOutCplx[nvia][md_fftn + j][0];
							pouttime[mwlen + j][1] = tOutCplx[nvia][md_fftn + j++][1];
						}
						for (j = 3 * d_fftn4; j < mdata.d_fftn; )
						{
							pouttime[mwlen + j][0] += tOutCplx[nvia][md_fftn + j][0];
							pouttime[mwlen + j][1] += tOutCplx[nvia][md_fftn + j++][1];
							pouttime[mwlen + j][0] += tOutCplx[nvia][md_fftn + j][0];
							pouttime[mwlen + j][1] += tOutCplx[nvia][md_fftn + j++][1];
							pouttime[mwlen + j][0] += tOutCplx[nvia][md_fftn + j][0];
							pouttime[mwlen + j][1] += tOutCplx[nvia][md_fftn + j++][1];
							pouttime[mwlen + j][0] += tOutCplx[nvia][md_fftn + j][0];
							pouttime[mwlen + j][1] += tOutCplx[nvia][md_fftn + j++][1];
						}
					}
				}
			}
			else
			{
				for ( m = 0; m < WLENXBUF + 1; m++)
				{
					mwlen = m*wlen - d_fftn4;
					md_fftn = m * mdata.d_fftn;
					if (m != 0)
					{
						for (j = 0; j < d_fftn4; )
						{
							pouttime[mwlen + j][0] += tOutCplx[nvia][md_fftn + j][0];
							pouttime[mwlen + j][1] -= tOutCplx[nvia][md_fftn + j++][1];
							pouttime[mwlen + j][0] += tOutCplx[nvia][md_fftn + j][0];
							pouttime[mwlen + j][1] -= tOutCplx[nvia][md_fftn + j++][1];
							pouttime[mwlen + j][0] += tOutCplx[nvia][md_fftn + j][0];
							pouttime[mwlen + j][1] -= tOutCplx[nvia][md_fftn + j++][1];
							pouttime[mwlen + j][0] += tOutCplx[nvia][md_fftn + j][0];
							pouttime[mwlen + j][1] -= tOutCplx[nvia][md_fftn + j++][1];
						}
					}
					for (j = d_fftn4; j < 2 * d_fftn4;)
					{
						pouttime[mwlen + j][0] = tOutCplx[nvia][md_fftn + j][0];
						pouttime[mwlen + j][1] = -tOutCplx[nvia][md_fftn + j++][1];
						pouttime[mwlen + j][0] = tOutCplx[nvia][md_fftn + j][0];
						pouttime[mwlen + j][1] = -tOutCplx[nvia][md_fftn + j++][1];
						pouttime[mwlen + j][0] = tOutCplx[nvia][md_fftn + j][0];
						pouttime[mwlen + j][1] = -tOutCplx[nvia][md_fftn + j++][1];
						pouttime[mwlen + j][0] = tOutCplx[nvia][md_fftn + j][0];
						pouttime[mwlen + j][1] = -tOutCplx[nvia][md_fftn + j++][1];
					}
					if (m != WLENXBUF)
					{
						for (j = 2 * d_fftn4; j < 3 * d_fftn4;)
						{
							pouttime[mwlen + j][0] = tOutCplx[nvia][md_fftn + j][0];
							pouttime[mwlen + j][1] = -tOutCplx[nvia][md_fftn + j++][1];
							pouttime[mwlen + j][0] = tOutCplx[nvia][md_fftn + j][0];
							pouttime[mwlen + j][1] = -tOutCplx[nvia][md_fftn + j++][1];
							pouttime[mwlen + j][0] = tOutCplx[nvia][md_fftn + j][0];
							pouttime[mwlen + j][1] = -tOutCplx[nvia][md_fftn + j++][1];
							pouttime[mwlen + j][0] = tOutCplx[nvia][md_fftn + j][0];
							pouttime[mwlen + j][1] = -tOutCplx[nvia][md_fftn + j++][1];
						}
						for (j = 3 * d_fftn4; j < mdata.d_fftn; )
						{
							pouttime[mwlen + j][0] += tOutCplx[nvia][md_fftn + j][0];
							pouttime[mwlen + j][1] -= tOutCplx[nvia][md_fftn + j++][1];
							pouttime[mwlen + j][0] += tOutCplx[nvia][md_fftn + j][0];
							pouttime[mwlen + j][1] -= tOutCplx[nvia][md_fftn + j++][1];
							pouttime[mwlen + j][0] += tOutCplx[nvia][md_fftn + j][0];
							pouttime[mwlen + j][1] -= tOutCplx[nvia][md_fftn + j++][1];
							pouttime[mwlen + j][0] += tOutCplx[nvia][md_fftn + j][0];
							pouttime[mwlen + j][1] -= tOutCplx[nvia][md_fftn + j++][1];
						}
					}
				}
			}
			mdata.active[nvia] = false;
		}
		else
		{
			mdata.active[nvia] = false;
			mdata.data_cond[nvia].wait(lock);
		}
	}
	DbgPrintf("\nf_thread%d end", nvia);
};


rfddc::rfddc() :
	t0(f_thread, std::ref(mdata), 0),
	t1(f_thread, std::ref(mdata), 1),
	t2(f_thread, std::ref(mdata), 2),
	t3(f_thread, std::ref(mdata), 3),
	t4(f_thread, std::ref(mdata), 4),
	t5(f_thread, std::ref(mdata), 5),
	t6(f_thread, std::ref(mdata), 6),
	t7(f_thread, std::ref(mdata), 7),
	t8(f_thread, std::ref(mdata), 8),
	t9(f_thread, std::ref(mdata), 9),
	t10(f_thread, std::ref(mdata), 10),
	t11(f_thread, std::ref(mdata), 11),
	t12(f_thread, std::ref(mdata), 12),
	t13(f_thread, std::ref(mdata), 13),
	t14(f_thread, std::ref(mdata), 14),
	t15(f_thread, std::ref(mdata), 15),
	grd0(t0),
	grd1(t1),
	grd2(t2),
	grd3(t3),
	grd4(t4),
	grd5(t5),
	grd6(t6),
	grd7(t7),
	grd8(t8),
	grd9(t9),
	grd10(t10),
	grd11(t11),
	grd12(t12),
	grd13(t13),
	grd14(t14),
	grd15(t15)
{
	SetThreadPriority((HANDLE)t0.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
	SetThreadPriority((HANDLE)t1.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
	SetThreadPriority((HANDLE)t2.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
	SetThreadPriority((HANDLE)t3.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
	SetThreadPriority((HANDLE)t4.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
	SetThreadPriority((HANDLE)t5.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
	SetThreadPriority((HANDLE)t6.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
	SetThreadPriority((HANDLE)t7.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
	SetThreadPriority((HANDLE)t8.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
	SetThreadPriority((HANDLE)t9.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
	SetThreadPriority((HANDLE)t10.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
	SetThreadPriority((HANDLE)t11.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
	SetThreadPriority((HANDLE)t12.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
	SetThreadPriority((HANDLE)t13.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
	SetThreadPriority((HANDLE)t14.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
	SetThreadPriority((HANDLE)t15.native_handle(), THREAD_PRIORITY_BELOW_NORMAL);
};


void rfddc::initp(int dfftn, unsigned char * buffers[16])
{
		mdata.d_fftn = dfftn;
		for (int n = 0; n < NVIA; n++) // input pointers
		{
			mdata.pbufin[n] = buffers[n];
			mdata.active[n] = false;
		}
		memset(tAdcCplx, 0, sizeof(tAdcCplx));
		memset(fDecCplx, 0, sizeof(fDecCplx));
		memset(fAdcCplx, 0, sizeof(fAdcCplx));
		memset(tOutCplx, 0, sizeof(tOutCplx));
		memset(planT2F, 0, sizeof(planT2F));
		memset(planF2T, 0, sizeof(planF2T));
		memset(Hwfilter, 0, sizeof(Hwfilter));

		for (int nvia = 0; nvia < NVIA; nvia++)
		{
			for (int m = 0; m < WLENXBUF + 1; m++)
			{
				planT2F[nvia][m] = fftwf_plan_dft_1d(FFTN, tAdcCplx[nvia][m], fAdcCplx[nvia][m], FFTW_FORWARD, FFTW_MEASURE);
				planF2T[nvia][m] = fftwf_plan_dft_1d(mdata.d_fftn, &fDecCplx[nvia][m*mdata.d_fftn], &tOutCplx[nvia][m*mdata.d_fftn], FFTW_BACKWARD, FFTW_MEASURE);
			}
		}
		fftwf_complex  htintime[FFTN];
		fftwf_plan ptime2Hw;     // fftw plan 

		ptime2Hw = fftwf_plan_dft_1d(FFTN, htintime, Hwfilter, FFTW_FORWARD, FFTW_ESTIMATE);

		for (int t = 0; t < FFTN; t++)
		{
			htintime[t][0] = htintime[t][1] = 0.0F;
		}

		switch (mdata.d_fftn)
		{
		case D_32:
			for (int t = 0; t < 257; t++)
				htintime[t][0] = Ht_257_64M_0600[t];
			break;
		case D_64:
			for (int t = 0; t < 257; t++)
				htintime[t][0] = Ht_257_64M_1500[t];
			break;

		case D_128:
			for (int t = 0; t < 257; t++)
				htintime[t][0] = Ht_257_64M_3400[t];
			break;
		case D_256:
		default:
			for (int t = 0; t < 257; t++)
				htintime[t][0] = Ht_257_64M_7200[t];
			break;
		}
		fftwf_execute(ptime2Hw);

#ifdef _RFDDC_SEQUENCE
		handleW = fopen("RFddc_sequence.bin ", "wb");
#endif
		mdata.initialized = true;
};

rfddc::~rfddc()
{
	mdata.isdying = true;   
	for (int iix = 0; iix < NVIA; iix++) // fluss all threads
	{
		std::lock_guard<std::mutex> lock(mdata.mut[iix]);
		mdata.iipath = iix;
		mdata.active[iix] = false;
		mdata.data_cond[iix].notify_one();
	}
 
#ifdef _RFDDC_SEQUENCE
	fclose(handleW);
#endif

};


int rfddc::arun( int iix, fftwf_complex * bufout )
{
#ifdef _RFDDC_SEQUENCE
	short s = 8000;
	while (mdata.active[iix])
	{
		s = -8000;
		std::this_thread::yield(); // in case wait
	}
	// write sequence of index with - sign if wait for thead 
	short data[2];
	data[0] = (short)(iix * 1000);
	data[1] = s;
	fwrite(&data, sizeof(short), 2, handleW);
#else
	while (mdata.active[iix])
	{
		std::this_thread::yield(); // in case wait
	}
#endif
	{
		std::lock_guard<std::mutex> lock(mdata.mut[iix]);
		mdata.iipath = iix;
		memcpy(bufout, outbufrs[iix], sizeof(fftwf_complex)*mdata.d_fftn * FFTXBUF);
		mdata.active[iix] = true;
		mdata.data_cond[iix].notify_one(); // runs thread
	}
	return 0;
};
