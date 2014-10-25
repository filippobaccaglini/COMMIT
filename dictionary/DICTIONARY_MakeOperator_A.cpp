// NOTE: uncomment the following 2 lines are to fix a bug with XCode 5.1 in OSX
//#include <stdint.h>
//typedef uint16_t char16_t;

#include <pthread.h>
#include <cmath>
#include <matrix.h>
#include <mex.h>

// avoid to perform parameter checking each time
#define DO_CHECK 0

// number of THREADS
#ifdef nTHREADS
	#if (nTHREADS<1 || nTHREADS>16)
 	#error "nTHREADS" must be >= 1 and <= 16
	#endif
#else
	#error "nTHREADS" parameter must be passed to the compiler as "-DnTHREADS=<value>"
#endif

// number of intra-axonal compartments (different RADII)
#ifdef nIC
	#if (nIC<0 || nIC>4)
	#error nIC must be >= 0 and <= 4
	#endif
#else
	#error "nIC" parameter must be passed to the compiler as "-DnIC=<value>"
#endif

// number of extra-axonal compartments (different TORTUOSITY)
#ifdef nEC
	#if (nEC<0 || nEC>4)
	#error nEC must be >= 0 and <= 4
	#endif
#else
	#error "nEC" parameter must be passed to the compiler as "-DnEC=<value>"
#endif

// number of isotropic compartments (CSF and others)
#ifdef nISO
	#if (nISO<0 || nISO>4)
	#error nISO must be >= 0 and <= 4
	#endif
#else
	#error "nISO" parameter must be passed to the compiler as "-DnISO=<value>"
#endif

/* global variables */
int				n, nV, nE, nF, nS, Y_dimx, Y_dimy, Y_dimz;
double			*x, *Y;

UINT8_T			*threadForSegment;
UINT32_T		*fiber;
UINT32_T		*ICv;
UINT16_T		*ICo;
float			*len;
UINT32_T		*ECv;
UINT16_T		*ECo;
UINT32_T		*ISOv;

#if nIC>=1
double *wmrSFP0;
#endif
#if nIC>=2
double *wmrSFP1;
#endif
#if nIC>=3
double *wmrSFP2;
#endif
#if nIC>=4
double *wmrSFP3;
#endif

#if nEC>=1
double *wmhSFP0;
#endif
#if nEC>=2
double *wmhSFP1;
#endif
#if nEC>=3
double *wmhSFP2;
#endif
#if nEC>=4
double *wmhSFP3;
#endif

#if nISO>=1
double *isoSFP0, *isoSFP0ptr, *x_isoPtr0, *x_isoPtr0End;
#endif
#if nISO>=2
double *isoSFP1, *isoSFP1ptr, *x_isoPtr1;
#endif
#if nISO>=3
double *isoSFP2, *isoSFP2ptr, *x_isoPtr2;
#endif
#if nISO>=4
double *isoSFP3, *isoSFP3ptr, *x_isoPtr3;
#endif


/* Compute a sub-block of the MATRIX-VECTOR product */
void* computeProductBlock( void *ptr )
{
	int			id = (long)ptr;
	int			offset;
	double		w, x0, x1, x2, x3;
	double		*Yptr, *YptrEnd;
	UINT8_T		*t_threadForSegment = threadForSegment;

	UINT32_T	*t_fiber = fiber;
	UINT32_T	*t_v     = ICv;
	UINT16_T	*t_o     = ICo;
	float		*t_len   = len;

	#if nIC>=1
	double *wmrSFP0ptr;
	#endif
	#if nIC>=2
	double *wmrSFP1ptr;
	#endif
	#if nIC>=3
	double *wmrSFP2ptr;
	#endif
	#if nIC>=4
	double *wmrSFP3ptr;
	#endif

	#if nIC>=1
	UINT8_T		*t_threadForSegmentEnd = t_threadForSegment + n;
	while( t_threadForSegment != t_threadForSegmentEnd )
	{
		if ( *t_threadForSegment++ == id )
		{
			x0 = x[*t_fiber];
			#if nIC>=2
			x1 = x[*t_fiber+nF];
			#endif
			#if nIC>=3
			x2 = x[*t_fiber+2*nF];
			#endif
			#if nIC>=4
			x3 = x[*t_fiber+3*nF];
			#endif

			if ( x0 > 0
			#if nIC>=2
				|| x1 > 0
			#endif
			#if nIC>=3
				|| x2 > 0
			#endif
			#if nIC>=4
				|| x3 > 0
			#endif
			)
			{
				Yptr         = Y         + nS * (*t_v);
				YptrEnd      = Yptr      + nS;
				w            = (double)(*t_len);
				offset       = nS * (*t_o);
				wmrSFP0ptr   = wmrSFP0   + offset;
				#if nIC>=2
				wmrSFP1ptr   = wmrSFP1   + offset;
				#endif
				#if nIC>=3
				wmrSFP2ptr   = wmrSFP2   + offset;
				#endif
				#if nIC>=4
				wmrSFP3ptr   = wmrSFP3   + offset;
				#endif

				while( Yptr != YptrEnd )
					(*Yptr++) += w * (
							  x0 * (*wmrSFP0ptr++)
							#if nIC>=2
							+ x1 * (*wmrSFP1ptr++)
							#endif
							#if nIC>=3
							+ x2 * (*wmrSFP2ptr++)
							#endif
							#if nIC>=4
							+ x3 * (*wmrSFP3ptr++)
							#endif
					);
			}
		}

		t_fiber++;
		t_v++;
		t_o++;
		t_len++;
	}
	#endif

    pthread_exit( 0 );
}


void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
	mxArray* tmp;

	/* ================ */
	/* Check the INPUTS */
	/* ================ */
	#if DO_CHECK > 0
 	if( nrhs != 4 )
		mexErrMsgIdAndTxt("InvalidInput:nrhs", "Require 4 inputs.");
	if ( !mxIsStruct(prhs[0]) )
		mexErrMsgIdAndTxt("InvalidInput:DICTIONARY", "'DICTIONARY' must be a struct");
	#endif

	mxArray* IC  = mxGetField( prhs[0], 0, "IC" );
	mxArray* EC  = mxGetField( prhs[0], 0, "EC" );
	mxArray* ISO = mxGetField( prhs[0], 0, "ISO" );
	#if DO_CHECK > 0
	if ( !mxIsStruct(IC) || !mxIsStruct(EC) || !mxIsStruct(ISO) )
		mexErrMsgIdAndTxt("InvalidInput:DICTIONARY", "'DICTIONARY' format is wrong");
	#endif

	// Parse "nS"
	tmp = mxGetField( prhs[1], 0, "nS" );
	#if DO_CHECK > 0
	if( !mxIsDouble(tmp) || mxIsComplex(tmp) ||  mxGetNumberOfElements(tmp)!=1 )
		mexErrMsgIdAndTxt("InvalidInput:nS","'nS' must be a real scalar");
	#endif
	nS = mxGetScalar( tmp );

	// Parse "nV"
	tmp = mxGetField( prhs[0], 0, "nV" );
	#if DO_CHECK > 0
	if( !mxIsDouble(tmp) || mxIsComplex(tmp) ||  mxGetNumberOfElements(tmp)!=1 )
		mexErrMsgIdAndTxt("InvalidInput:nV","'nV' must be a real scalar");
	#endif
	nV = mxGetScalar( tmp );

	// Parse "nE"
	tmp = mxGetField( EC, 0, "nE" );
	#if DO_CHECK > 0
	if( !mxIsDouble(tmp) || mxIsComplex(tmp) ||  mxGetNumberOfElements(tmp)!=1 )
		mexErrMsgIdAndTxt("InvalidInput:EC.nE","'EC.nE' must be a real scalar");
	#endif
	nE = mxGetScalar( tmp );

	// Parse "n"
	tmp = mxGetField( IC, 0, "n" );
	#if DO_CHECK > 0
	if( !mxIsDouble(tmp) || mxIsComplex(tmp) ||  mxGetNumberOfElements(tmp)!=1 )
		mexErrMsgIdAndTxt("InvalidInput:n","'n' must be a real scalar");
	#endif
	n = mxGetScalar( tmp );

	// Parse "fiber"
	tmp = mxGetField( IC, 0, "fiber" );
	#if DO_CHECK > 0
	if ( mxGetNumberOfDimensions(tmp) != 2 || mxGetN(tmp) != 1 )
		mexErrMsgIdAndTxt("InvalidInput:fiber","'fiber' must be a n*1 vector");
	#endif
 	fiber = (UINT32_T*) mxGetData( tmp );

	// Parse "len"
	tmp = mxGetField( IC, 0, "len" );
	#if DO_CHECK > 0
	if ( mxGetNumberOfDimensions(tmp) != 2 || mxGetN(tmp) != 1 )
		mexErrMsgIdAndTxt("InvalidInput:len","'len' must be a n*1 vector");
	#endif
 	len = (float*) mxGetData(tmp);

	// Parse "ICv", "ICo"
	tmp = mxGetField( IC, 0, "v" );
	#if DO_CHECK > 0
	if ( mxGetNumberOfDimensions(tmp) != 2 || mxGetN(tmp) != 1 )
		mexErrMsgIdAndTxt("InvalidInput:IC.v","'IC.v' must be a n*1 vector");
	#endif
	ICv = (UINT32_T*) mxGetData(tmp);
	tmp = mxGetField( IC, 0, "o" );
	#if DO_CHECK > 0
	if ( mxGetNumberOfDimensions(tmp) != 2 || mxGetN(tmp) != 1 )
		mexErrMsgIdAndTxt("InvalidInput:IC.o","'IC.o' must be a n*1 vector");
	#endif
	ICo = (UINT16_T*) mxGetData(tmp);

	// Parse "ECv","ECo"
	tmp = mxGetField( EC, 0, "v" );
	#if DO_CHECK > 0
	if ( mxGetNumberOfDimensions(tmp) != 2 || mxGetN(tmp) != 1 )
		mexErrMsgIdAndTxt("InvalidInput:EC.v","'EC.v' must be a n*1 vector");
	#endif
	ECv = (UINT32_T*) mxGetData(tmp);
	tmp = mxGetField( EC, 0, "o" );
	#if DO_CHECK > 0
	if ( mxGetNumberOfDimensions(tmp) != 2 || mxGetN(tmp) != 1 )
		mexErrMsgIdAndTxt("InvalidInput:EC.o","'EC.o' must be a n*1 vector");
	#endif
	ECo = (UINT16_T*) mxGetData(tmp);

	// Parse "ISOv"
	tmp = mxGetField( ISO, 0, "v" );
	#if DO_CHECK > 0
	if ( mxGetNumberOfDimensions(tmp) != 2 || mxGetN(tmp) != 1 )
		mexErrMsgIdAndTxt("InvalidInput:ISO.v","'ISO.v' must be a n*1 vector");
	#endif
	ISOv = (UINT32_T*) mxGetData(tmp);

	// Parse "KERNELS.wmr", "KERNELS.wmh" and "KERNELS.iso"
	mxArray* wmr = mxGetField( prhs[1], 0, "wmr" );
	#if DO_CHECK > 0
	if ( !mxIsCell(wmr) )
		mexErrMsgIdAndTxt("InvalidInput:wmr","'wmr' must be a cell array");
	#endif
	#if nIC>=1
	wmrSFP0 = (double*) mxGetData( mxGetCell(wmr,0) );
	#endif
	#if nIC>=2
	wmrSFP1 = (double*) mxGetData( mxGetCell(wmr,1) );
	#endif
	#if nIC>=3
	wmrSFP2 = (double*) mxGetData( mxGetCell(wmr,2) );
	#endif
	#if nIC>=4
	wmrSFP3 = (double*) mxGetData( mxGetCell(wmr,3) );
	#endif

	mxArray* wmh = mxGetField( prhs[1], 0, "wmh" );
	#if DO_CHECK > 0
	if ( !mxIsCell(wmh) )
		mexErrMsgIdAndTxt("InvalidInput:wmh","'wmh' must be a cell array");
	#endif
	#if nEC>=1
	wmhSFP0 = (double*) mxGetData( mxGetCell(wmh,0) );
	#endif
	#if nEC>=2
	wmhSFP1 = (double*) mxGetData( mxGetCell(wmh,1) );
	#endif
	#if nEC>=3
	wmhSFP2 = (double*) mxGetData( mxGetCell(wmh,2) );
	#endif
	#if nEC>=4
	wmhSFP3 = (double*) mxGetData( mxGetCell(wmh,3) );
	#endif

	mxArray* iso = mxGetField( prhs[1], 0, "iso" );
	#if DO_CHECK > 0
	if ( !mxIsCell(iso) )
		mexErrMsgIdAndTxt("InvalidInput:iso","'iso' must be a cell array");
	#endif
	#if nISO>=1
	isoSFP0 = (double*) mxGetData( mxGetCell(iso,0) );
	#endif
	#if nISO>=2
	isoSFP1 = (double*) mxGetData( mxGetCell(iso,1) );
	#endif
	#if nISO>=3
	isoSFP2 = (double*) mxGetData( mxGetCell(iso,2) );
	#endif
	#if nISO>=4
	isoSFP3 = (double*) mxGetData( mxGetCell(iso,3) );
	#endif

	// Parse "Y_dimx", "Y_dimy" and "Y_dimz"
	mxArray* dim = mxGetField( prhs[0], 0, "dim" );
	#if DO_CHECK > 0
	if ( !mxIsDouble(dim) || mxIsComplex(dim) || mxGetNumberOfElements(dim)!=3 )
		mexErrMsgIdAndTxt("InvalidInput:dim","'dim' must be a 3x1 double array");
	#endif
	double* Y_dim = (double*) mxGetData(dim);
	Y_dimx = Y_dim[0];
	Y_dimy = Y_dim[1];
	Y_dimz = Y_dim[2];

	// parse "threadForSegment"
	#if DO_CHECK > 0
	if ( mxGetNumberOfDimensions( prhs[2] ) != 2 || mxGetN(prhs[2]) != 1 )
		mexErrMsgIdAndTxt("InvalidInput:threadForSegment","'threadForSegment' must be a n*1 vector");
	#endif
 	threadForSegment = (UINT8_T*) mxGetData( prhs[2] );

	// Parse "x"
	#if DO_CHECK > 0
	if ( mxGetNumberOfDimensions( prhs[3] ) != 2 || mxGetN(prhs[3]) != 1 )
		mexErrMsgIdAndTxt("InvalidInput:x","'x' must be a n*1 vector");
	#endif
 	x = (double*) mxGetData( prhs[3] );
	if ( nIC > 0 )
		nF = ( mxGetM(prhs[3]) - nE*nEC - nV*nISO) / nIC;
	else
		nF = 0;


	/* =============== */
	/* Set the OUTPUTS */
	/* =============== */
	#if DO_CHECK > 0
	if( nlhs != 1 )
		mexErrMsgIdAndTxt("InvalidOutput:nlhs", "Required 1 output.");
	#endif

	const int outDims[4] = { nS, Y_dimx, Y_dimy, Y_dimz };
    plhs[0] = mxCreateNumericArray(4, outDims, mxDOUBLE_CLASS, mxREAL);
    Y = (double*)mxGetData( plhs[0] );

	/* ====================================================== */
	/* Run SEPARATE THREADS for intra- and extra-compartments */
	/* ====================================================== */
	#if nIC>=1
	pthread_t threads[nTHREADS];
	for(int t=0; t<nTHREADS ; t++)
		pthread_create( &threads[t], NULL, computeProductBlock, (void *) (long int)t );
	for(int t=0; t<nTHREADS ; t++)
		pthread_join( threads[t], NULL );
	#endif


	/* =================================================== */
	/* EC and ISO compartments are computed by MAIN THREAD */
	/* =================================================== */
	double	x0, x1, x2, x3;
	double	*Yptr, *YptrEnd;
	int		offset;


	/* extra-cellular compartments */
	#if nEC>=1
	double *wmhSFP0ptr;
	double *x_wmhPtr0    = x + nIC*nF;
	double *x_wmhPtr0End = x_wmhPtr0 + nE;
	#if nEC>=2
	double *wmhSFP1ptr, *x_wmhPtr1 = x_wmhPtr0 + nE;
	#endif
	#if nEC>=3
	double *wmhSFP2ptr, *x_wmhPtr2 = x_wmhPtr1 + nE;
	#endif
	#if nEC>=4
	double *wmhSFP3ptr, *x_wmhPtr3 = x_wmhPtr2 + nE;
	#endif

	while( x_wmhPtr0 != x_wmhPtr0End )
	{
		x0 = *x_wmhPtr0++;
		#if nEC>=2
		x1 = *x_wmhPtr1++;
		#endif
		#if nEC>=3
		x2 = *x_wmhPtr2++;
		#endif
		#if nEC>=4
		x3 = *x_wmhPtr3++;
		#endif
		if (
			   x0 > 0
			#if nEC>=2
			|| x1 > 0
			#endif
			#if nEC>=3
			|| x2 > 0
			#endif
			#if nEC>=4
			|| x3 > 0
			#endif
		  )
		{
			Yptr          = Y         + nS * (*ECv);
			YptrEnd       = Yptr      + nS;
			offset        = nS * (*ECo);
			wmhSFP0ptr    = wmhSFP0  + offset;
			#if nEC>=2
			wmhSFP1ptr    = wmhSFP1  + offset;
			#endif
			#if nEC>=3
			wmhSFP2ptr    = wmhSFP2  + offset;
			#endif
			#if nEC>=4
			wmhSFP3ptr    = wmhSFP3  + offset;
			#endif

			while( Yptr != YptrEnd )
				(*Yptr++) += (
					  x0 * (*wmhSFP0ptr++)
					#if nEC>=2
					+ x1 * (*wmhSFP1ptr++)
					#endif
					#if nEC>=3
					+ x2 * (*wmhSFP2ptr++)
					#endif
					#if nEC>=4
					+ x3 * (*wmhSFP3ptr++)
					#endif
				);
		}
		ECv++;
		ECo++;
	}
	#endif


	/* isotropic compartments */
	#if nISO>=1

	x_isoPtr0		= x + nIC*nF + nEC*nE;
	x_isoPtr0End	= x_isoPtr0 + nV;
	#if nISO>=2
	x_isoPtr1		= x_isoPtr0 + nV;
	#endif
	#if nISO>=3
	x_isoPtr2		= x_isoPtr1 + nV;
	#endif
	#if nISO>=4
	x_isoPtr3		= x_isoPtr2 + nV;
	#endif

	while( x_isoPtr0 != x_isoPtr0End )
	{
		x0 = *x_isoPtr0++;
		#if nISO>=2
		x1 = *x_isoPtr1++;
		#endif
		#if nISO>=3
		x2 = *x_isoPtr2++;
		#endif
		#if nISO>=4
		x3 = *x_isoPtr3++;
		#endif
		if (
				x0 > 0
			#if nISO>=2
			 || x1 > 0
			#endif
			#if nISO>=3
			 || x2 > 0
			#endif
			#if nISO>=4
			 || x3 > 0
			#endif
			)
		{
			Yptr         = Y         + nS * (*ISOv);
			YptrEnd      = Yptr      + nS;
			isoSFP0ptr   = isoSFP0;
			#if nISO>=2
			isoSFP1ptr   = isoSFP1;
			#endif
			#if nISO>=3
			isoSFP2ptr   = isoSFP2;
			#endif
			#if nISO>=4
			isoSFP3ptr   = isoSFP3;
			#endif
			while( Yptr != YptrEnd )
				(*Yptr++) +=
						    x0 * (*isoSFP0ptr++)
						  #if nISO>=2
						  + x1 * (*isoSFP1ptr++)
						  #endif
						  #if nISO>=3
						  + x2 * (*isoSFP2ptr++)
						  #endif
						  #if nISO>=4
						  + x3 * (*isoSFP3ptr++)
						  #endif
				;
		}
		ISOv++;
	}
	#endif

	return;
}
