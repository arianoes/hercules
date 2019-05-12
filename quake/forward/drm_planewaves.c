/* -*- C -*- */

/* @copyright_notice_start
 *
 * This file is part of the CMU Hercules ground motion simulator developed
 * by the CMU Quake project.
 *
 * Copyright (C) Carnegie Mellon University. All rights reserved.
 *
 * This program is covered by the terms described in the 'LICENSE.txt' file
 * included with this software package.
 *
 * This program comes WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * 'LICENSE.txt' file for more details.
 *
 *  @copyright_notice_end
 */

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#include "psolve.h"
#include "octor.h"
#include "util.h"
#include "stiffness.h"
#include "quake_util.h"
#include "cvm.h"
#include "drm_planewaves.h"
#include "topography.h"
#include "geometrics.h"


static pwtype_t      	thePlaneWaveType;
static fnctype_t        theFncType;
static int32_t	        theDRMBox_halfwidthElements_ew = 0;
static int32_t	        theDRMBox_halfwidthElements_ns = 0;
static int32_t	        theDRMBox_DepthElements = 0;
static double 	        thedrmbox_esize         = 0.0;
static double 	        thedrmbox_xo            = 0.0;
static double 	        thedrmbox_yo            = 0.0;

static double 	        theUg_Dt;
static double           *theUg_str;
static double           *theUg_nrm;
static int32_t	        the_Ug_NoData = 0;
static int32_t	        the_NoComp = 1;


static double 	        theTs = 0.0;
static double 	        thefc = 0.0;
static double           theUo = 0.0;
static double 	        theplanewave_strike = 0.0;
static double 	        theplanewave_Zangle = 0.0;
static double 	        theXc  = 0.0;
static double 	        theYc  = 0.0;

static int32_t          *myDRMFace1ElementsMapping;
static int32_t          *myDRMFace2ElementsMapping;
static int32_t          *myDRMFace3ElementsMapping;
static int32_t          *myDRMFace4ElementsMapping;
static int32_t          *myDRMBottomElementsMapping;

static int32_t          *myDRMBorder1ElementsMapping;
static int32_t          *myDRMBorder2ElementsMapping;
static int32_t          *myDRMBorder3ElementsMapping;
static int32_t          *myDRMBorder4ElementsMapping;

static int32_t          *myDRMBorder5ElementsMapping;
static int32_t          *myDRMBorder6ElementsMapping;
static int32_t          *myDRMBorder7ElementsMapping;
static int32_t          *myDRMBorder8ElementsMapping;
//static double              theDRMdepth;

static int32_t          myDRM_Face1Count  = 0;
static int32_t          myDRM_Face2Count  = 0;
static int32_t          myDRM_Face3Count  = 0;
static int32_t          myDRM_Face4Count  = 0;
static int32_t          myDRM_BottomCount = 0;
static int32_t          myDRM_Brd1 = 0;
static int32_t          myDRM_Brd2 = 0;
static int32_t          myDRM_Brd3 = 0;
static int32_t          myDRM_Brd4 = 0;
static int32_t          myDRM_Brd5 = 0;
static int32_t          myDRM_Brd6 = 0;
static int32_t          myDRM_Brd7 = 0;
static int32_t          myDRM_Brd8 = 0;

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))

void drm_planewaves_init ( int32_t myID, const char *parametersin ) {

    int     int_message[7];
    double  double_message[11];

    /* Capturing data from file --- only done by PE0 */
    if (myID == 0) {
        if ( drm_planewaves_initparameters( parametersin ) != 0 ) {
            fprintf(stderr,"Thread %d: drm_planewaves_init: "
                    "incidentPlaneWaves_initparameters error\n",myID);
            MPI_Abort(MPI_COMM_WORLD, ERROR);
            exit(1);
        }
    }

    /* Broadcasting data */
    int_message   [0]    = (int)thePlaneWaveType;
    int_message   [1]    = theDRMBox_halfwidthElements_ew;
    int_message   [2]    = theDRMBox_halfwidthElements_ns;
    int_message   [3]    = theDRMBox_DepthElements;
    int_message   [4]    = theFncType;
    int_message   [5]    = the_Ug_NoData;
    int_message   [6]    = the_NoComp;

    double_message[0]  = theTs;
    double_message[1]  = thefc;
    double_message[2]  = theUo;
    double_message[3]  = theplanewave_strike;
    double_message[4]  = theXc;
    double_message[5]  = theYc;
    double_message[6]  = thedrmbox_esize;
    double_message[7]  = theplanewave_Zangle;
    double_message[8]  = theUg_Dt;
    double_message[9]  = thedrmbox_xo;
    double_message[10] = thedrmbox_yo;

    MPI_Bcast(double_message, 11, MPI_DOUBLE, 0, comm_solver);
    MPI_Bcast(int_message,     7, MPI_INT,    0, comm_solver);

    thePlaneWaveType                = int_message[0];
    theDRMBox_halfwidthElements_ew  = int_message[1];
    theDRMBox_halfwidthElements_ns  = int_message[2];
    theDRMBox_DepthElements         = int_message[3];
    theFncType                      = int_message[4];
    the_Ug_NoData                   = int_message[5];
    the_NoComp                      = int_message[6];

    theTs               = double_message[0];
    thefc               = double_message[1];
    theUo               = double_message[2];
    theplanewave_strike = double_message[3];
    theXc               = double_message[4];
    theYc               = double_message[5];
    thedrmbox_esize     = double_message[6];
    theplanewave_Zangle = double_message[7];
    theUg_Dt            = double_message[8];
    thedrmbox_xo        = double_message[9];
    thedrmbox_yo        = double_message[10];

	    /* allocate table of properties for all other PEs */
	 if (myID != 0 && theFncType == THST ) {
		theUg_str        = (double*)malloc( sizeof(double) * the_Ug_NoData );
		if ( the_NoComp == 2 )
			theUg_nrm        = (double*)malloc( sizeof(double) * the_Ug_NoData );
	 }

	MPI_Bcast(theUg_str,   the_Ug_NoData, MPI_DOUBLE, 0, comm_solver);
	if ( the_NoComp == 2 )
		MPI_Bcast(theUg_nrm,   the_Ug_NoData, MPI_DOUBLE, 0, comm_solver);

    return;

}



int32_t
drm_planewaves_initparameters ( const char *parametersin ) {
	FILE                *fp;

	double      drmbox_halfwidth_elements_ew, drmbox_halfwidth_elements_ns, drmbox_depth_elements, Ts, fc, Uo,
	            planewave_strike, planewave_zAngle, L_ew, L_ns, drmbox_esize, ug_dt, drmbox_xo, drmbox_yo;
	char        type_of_wave[64], type_of_fnc[64], ugstr_file[256], ugnrm_file[256];

	int         no_datastr, no_datanrm, i_ug=0, no_comp;

	FILE        *fp_ugstr, *fp_ugnrm;

	pwtype_t    planewave;
	fnctype_t   fnc_type;


	/* Opens parametersin file */

	if ( ( fp = fopen(parametersin, "r" ) ) == NULL ) {
		fprintf( stderr,
				"Error opening %s\n at drm_planewaves_initparameters",
				parametersin );
		return -1;
	}


	/* Parses parametersin to capture drm_planewaves single-value parameters */
	if ( ( parsetext(fp, "type_of_wave",                      's', &type_of_wave                  ) != 0) ||
			( parsetext(fp, "DRMBox_Noelem_Halfwidth_EW",        'd', &drmbox_halfwidth_elements_ew  ) != 0) ||
			( parsetext(fp, "DRMBox_Noelem_Halfwidth_NS",        'd', &drmbox_halfwidth_elements_ns  ) != 0) ||
			( parsetext(fp, "DRMBox_Noelem_depth",               'd', &drmbox_depth_elements         ) != 0) ||
			( parsetext(fp, "DRMBox_element_size",               'd', &drmbox_esize                  ) != 0) ||
			( parsetext(fp, "DRM_xo",                            'd', &drmbox_xo                     ) != 0) ||
			( parsetext(fp, "DRM_yo",                            'd', &drmbox_yo                     ) != 0) ||
			( parsetext(fp, "ug_alongstrike",                    's', &ugstr_file                    ) != 0) ||
			( parsetext(fp, "ug_alongnormal",                    's', &ugnrm_file                    ) != 0) ||
			( parsetext(fp, "ug_timestep",                       'd', &ug_dt                         ) != 0) ||
			( parsetext(fp, "no_components",                     'i', &no_comp                       ) != 0) ||
			( parsetext(fp, "Ts",                                'd', &Ts                            ) != 0) ||
			( parsetext(fp, "region_length_east_m",              'd', &L_ew                          ) != 0) ||
			( parsetext(fp, "region_length_north_m",             'd', &L_ns                          ) != 0) ||
			( parsetext(fp, "fc",                                'd', &fc                            ) != 0) ||
			( parsetext(fp, "Uo",                                'd', &Uo                            ) != 0) ||
			( parsetext(fp, "planewave_strike",                  'd', &planewave_strike              ) != 0) ||
			( parsetext(fp, "planewave_Z_angle",                 'd', &planewave_zAngle              ) != 0) ||
			( parsetext(fp, "fnc_type",                          's', &type_of_fnc                   ) != 0)
	)
	{
		fprintf( stderr,
				"Error parsing planewaves parameters from %s\n",
				parametersin );
		return -1;
	}

	if ( strcasecmp(type_of_wave, "SV") == 0 ) {
		planewave = SV1;
	} else if ( strcasecmp(type_of_wave, "P") == 0 ) {
		planewave = P1;
	} else {
		fprintf(stderr,
				"Illegal type_of_wave for incident plane wave analysis"
				"(SV, P): %s\n", type_of_wave);
		return -1;
	}

	if ( strcasecmp(type_of_fnc, "RICKER") == 0 ) {
		fnc_type = RICK;
	} else if ( strcasecmp(type_of_fnc, "time_hist") == 0 ) {
		fnc_type = THST;

		if ( ( fp_ugstr   = fopen ( ugstr_file, "r") ) == NULL ) {
			fprintf(stderr, "Error opening file of displ ground motion along strike \n" );
			return -1;
		}

		fscanf( fp_ugstr,   " %i ", &no_datastr );

		if ( no_comp == 2 ) {
			if ( ( fp_ugnrm   = fopen ( ugnrm_file, "r") ) == NULL ) {
				fprintf(stderr, "Error opening file of displ ground motion along normal to strike \n" );
				return -1;
			}
			fscanf( fp_ugnrm,   " %i ", &no_datanrm );
		}

		if ( no_comp == 2 ) {
			if ( no_datastr == no_datanrm ) {
				the_Ug_NoData = no_datastr;
			} else {
				fprintf(stderr, "Ground motion files of different size  \n" );
				return -1;
			}
		} else {
			the_Ug_NoData = no_datastr;
		}

		theUg_Dt      = ug_dt;
		theUg_str     = (double*)malloc( sizeof(double) * the_Ug_NoData );

		if ( no_comp == 2 ) {
			theUg_nrm     = (double*)malloc( sizeof(double) * the_Ug_NoData );

			if ( theUg_nrm == NULL   ) {
				fprintf( stderr, "Error allocating array for ug data along normal"
						"in drm_planewaves_initparameters " );
				return -1; }

			for ( i_ug = 0; i_ug < the_Ug_NoData; ++i_ug ) {
				fscanf(fp_ugnrm,   " %lf ", &(theUg_nrm[i_ug]));
			}

			fclose(fp_ugnrm);
		}

		if (  theUg_str == NULL  ) {
			fprintf( stderr, "Error allocating array for ug data along strike"
					"in drm_planewaves_initparameters " );
			return -1; }

		for ( i_ug = 0; i_ug < the_Ug_NoData; ++i_ug ) {
			fscanf(fp_ugstr,   " %lf ", &(theUg_str[i_ug]));
		}

		fclose(fp_ugstr);

	} else {
		fprintf(stderr,
				"Illegal fnc_type for incident plane wave analysis"
				"RICKER, or TIME_HIST): %s\n", type_of_fnc);
		return -1;
	}

	//theFncType


	/*  Initialize the static global variables */
	thePlaneWaveType                 = planewave;
	theDRMBox_halfwidthElements_ew   = drmbox_halfwidth_elements_ew;
	theDRMBox_halfwidthElements_ns   = drmbox_halfwidth_elements_ns;
	theDRMBox_DepthElements          = drmbox_depth_elements;
	theTs                            = Ts;
	thefc                            = fc;
	theUo                            = Uo;
	theplanewave_strike              = planewave_strike * PI / 180.00;
	theplanewave_Zangle              = planewave_zAngle * PI / 180.00;
	theXc                            = L_ew / 2.0;
	theYc                            = L_ns / 2.0;
	thedrmbox_esize                  = drmbox_esize;
	thedrmbox_xo                     = drmbox_xo;
	thedrmbox_yo                     = drmbox_yo;
	theFncType                       = fnc_type;
	the_NoComp                       = no_comp;

	fclose(fp);

	return 0;
}

void PlaneWaves_solver_init( int32_t myID, mesh_t *myMesh, mysolver_t *mySolver) {

	int32_t theFaceElem_ew, theFaceElem_ns, theBaseElem;
	double  theDRMdepth, DRM_D = theDRMBox_DepthElements * thedrmbox_esize;

	//double DRM_B = theDRMBox_halfwidthElements * thedrmbox_esize;
	double thebase_zcoord = get_thebase_topo();

	theFaceElem_ew = 2 * theDRMBox_halfwidthElements_ew * theDRMBox_DepthElements;
	theFaceElem_ns = 2 * theDRMBox_halfwidthElements_ns * theDRMBox_DepthElements;
	theBaseElem    = 4 * theDRMBox_halfwidthElements_ew * theDRMBox_halfwidthElements_ns;
	theDRMdepth    = DRM_D;

	double DRM_EW = theDRMBox_halfwidthElements_ew * thedrmbox_esize;
	double DRM_NS = theDRMBox_halfwidthElements_ns * thedrmbox_esize;

	/*  mapping of face1 elements */
	int32_t eindex;
	int32_t countf1 = 0, countf2 = 0, countf3 = 0, countf4 = 0, countbott=0;
	int32_t countb1 = 0, countb2 = 0, countb3 = 0, countb4 = 0;
	int32_t countb5 = 0, countb6 = 0, countb7 = 0, countb8 = 0;

	XMALLOC_VAR_N(myDRMFace1ElementsMapping, int32_t, theFaceElem_ns); // right (XY view)
	XMALLOC_VAR_N(myDRMFace2ElementsMapping, int32_t, theFaceElem_ns); // left (XY view)
	XMALLOC_VAR_N(myDRMFace3ElementsMapping, int32_t, theFaceElem_ew); // bottom (XY view)
	XMALLOC_VAR_N(myDRMFace4ElementsMapping, int32_t, theFaceElem_ew); // top (XY view)
	XMALLOC_VAR_N(myDRMBottomElementsMapping , int32_t, theBaseElem); // base (XY view)

	/* border elements*/
	XMALLOC_VAR_N(myDRMBorder1ElementsMapping, int32_t, theDRMBox_DepthElements + 1); //  bottom right-hand corner (XY view)
	XMALLOC_VAR_N(myDRMBorder2ElementsMapping, int32_t, theDRMBox_DepthElements + 1); //  top right-hand corner (XY view)
	XMALLOC_VAR_N(myDRMBorder3ElementsMapping, int32_t, theDRMBox_DepthElements + 1); //  bottom left-hand corner (XY view)
	XMALLOC_VAR_N(myDRMBorder4ElementsMapping, int32_t, theDRMBox_DepthElements + 1); //  top left-hand corner (XY view)

	XMALLOC_VAR_N(myDRMBorder5ElementsMapping, int32_t, theDRMBox_halfwidthElements_ns * 2); // right
	XMALLOC_VAR_N(myDRMBorder6ElementsMapping, int32_t, theDRMBox_halfwidthElements_ew * 2); // left
	XMALLOC_VAR_N(myDRMBorder7ElementsMapping, int32_t, theDRMBox_halfwidthElements_ns * 2); // bottom
	XMALLOC_VAR_N(myDRMBorder8ElementsMapping, int32_t, theDRMBox_halfwidthElements_ew * 2); // top

	for (eindex = 0; eindex < myMesh->lenum; eindex++) {

		elem_t     *elemp;
		node_t     *node0dat;
		edata_t    *edata;
		double      xo, yo, zo;
		int32_t	    node0;

		elemp    = &myMesh->elemTable[eindex]; //Takes the information of the "eindex" element
		node0    = elemp->lnid[0];             //Takes the ID for the zero node in element eindex
		node0dat = &myMesh->nodeTable[node0];
		edata    = (edata_t *)elemp->data;

		/* get coordinates of element zero node */
		xo = (node0dat->x)*(myMesh->ticksize);
		yo = (node0dat->y)*(myMesh->ticksize);
		zo = (node0dat->z)*(myMesh->ticksize);


		if (    ( yo ==  thedrmbox_yo +  2.0 * DRM_EW )   &&                             /* face 1: right */
				( xo >= thedrmbox_xo ) &&
				( xo <  ( thedrmbox_xo + 2.0 * DRM_NS ) ) &&
				( zo <  DRM_D + thebase_zcoord ) &&
				( zo >=  thebase_zcoord ) ) {

			myDRMFace1ElementsMapping[countf1] = eindex;
			countf1++;
		} else 	if ( ( ( yo + thedrmbox_esize )   == thedrmbox_yo  ) &&                  /* face 2: left*/
				     ( xo >= thedrmbox_xo ) &&
				     ( xo <  ( thedrmbox_xo + 2.0 * DRM_NS ) ) &&
				     ( zo  <  DRM_D + thebase_zcoord ) &&
				     ( zo  >=  thebase_zcoord ) ) {

			myDRMFace2ElementsMapping[countf2] = eindex;
			countf2++;
		} else 	if ( ( ( xo + thedrmbox_esize ) == thedrmbox_xo ) &&                     /* face 3: bottom */
				       ( yo >= thedrmbox_yo ) &&
				       ( yo <  ( thedrmbox_yo + 2.0 * DRM_EW ) ) &&
				       ( zo <  DRM_D + thebase_zcoord ) &&
				       ( zo >=  thebase_zcoord ) ) {

			myDRMFace3ElementsMapping[countf3] = eindex;
			countf3++;
		} else 	if ( (   xo == thedrmbox_xo + 2.0 * DRM_NS ) &&                          /* face 4: top */
				       ( yo >= thedrmbox_yo ) &&
				       ( yo <  ( thedrmbox_yo + 2.0 * DRM_EW ) ) &&
				       ( zo <  DRM_D + thebase_zcoord ) &&
				       ( zo >=  thebase_zcoord ) ) {

			myDRMFace4ElementsMapping[countf4] = eindex;
			countf4++;

		} else 	if ( ( yo >= thedrmbox_yo ) &&                                           /* base */
				     ( yo <  ( thedrmbox_yo + 2.0 * DRM_EW ) ) &&
				     ( xo >= thedrmbox_xo ) &&
				     ( xo <  ( thedrmbox_xo + 2.0 * DRM_NS ) ) &&
				     ( zo ==  DRM_D + thebase_zcoord ) ) {

			myDRMBottomElementsMapping[countbott] = eindex;
			countbott++;

		} else 	if ( (   yo == ( thedrmbox_yo + 2.0 * DRM_EW ) ) &&                     /* border 1 */
				     ( ( xo + thedrmbox_esize ) == thedrmbox_xo ) &&
				       ( zo <=  DRM_D + thebase_zcoord ) &&
				       ( zo >=  thebase_zcoord ) ) {

			myDRMBorder1ElementsMapping[countb1] = eindex;
			countb1++;

		} else if ( (   yo == ( thedrmbox_yo + 2.0 * DRM_EW )  )   &&                  /*border 2*/
				      ( xo  == ( thedrmbox_xo + 2.0 * DRM_NS ) ) &&
				      ( zo <=  DRM_D + thebase_zcoord ) &&
				      ( zo >=  thebase_zcoord ) ) {

			myDRMBorder2ElementsMapping[countb2] = eindex;
			countb2++;

		} else if ( ( ( yo + thedrmbox_esize ) == thedrmbox_yo ) &&                   /* border 3*/
				    ( ( xo + thedrmbox_esize)  == thedrmbox_xo ) &&
				      ( zo <=  DRM_D + thebase_zcoord ) &&
				      ( zo >=  thebase_zcoord ) ) {

			myDRMBorder3ElementsMapping[countb3] = eindex;
			countb3++;

		} else if ( ( (  yo + thedrmbox_esize  ) == thedrmbox_yo ) &&                /* border 4*/
				      (  xo  == ( thedrmbox_xo + 2.0 * DRM_NS ) ) &&
				      ( zo <=  DRM_D + thebase_zcoord ) &&
				      ( zo >=  thebase_zcoord ) ) {

			myDRMBorder4ElementsMapping[countb4] = eindex;
			countb4++;

		} else 	if ( (  yo == thedrmbox_yo + 2.0 * DRM_EW ) &&                      /* border 5 : right*/
				       ( xo >= thedrmbox_xo ) &&
				       ( xo <  ( thedrmbox_xo + 2.0 * DRM_NS ) ) &&
				       ( zo ==  DRM_D + thebase_zcoord ) ) {

			myDRMBorder5ElementsMapping[countb5] = eindex;
			countb5++;

		} else if ( ( ( yo + thedrmbox_esize ) == thedrmbox_yo ) &&                /* border 6: left*/
			        ( xo >= thedrmbox_xo ) &&
			        ( xo <  ( thedrmbox_xo + 2.0 * DRM_NS ) ) &&
				    ( zo ==  DRM_D + thebase_zcoord ) ) {

			myDRMBorder6ElementsMapping[countb6] = eindex;
			countb6++;

		} else if ( ( ( xo + thedrmbox_esize )  == thedrmbox_xo ) &&               /* border 7: bottom*/
				      ( yo >=  thedrmbox_yo  ) &&
				      ( yo <  ( thedrmbox_yo + 2.0 * DRM_EW ) ) &&
				      ( zo ==  DRM_D + thebase_zcoord ) ) {

			myDRMBorder7ElementsMapping[countb7] = eindex;
			countb7++;

		} else if ( (   xo   == thedrmbox_xo + 2.0 * DRM_NS ) &&                    /* border 8: top*/
			        ( yo >=  thedrmbox_yo  ) &&
			        ( yo <  ( thedrmbox_yo + 2.0 * DRM_EW ) ) &&
				    ( zo ==  DRM_D + thebase_zcoord ) ) {

			myDRMBorder8ElementsMapping[countb8] = eindex;
			countb8++;
		}
	}

	myDRM_Face1Count  = countf1;
	myDRM_Face2Count  = countf2;
	myDRM_Face3Count  = countf3;
	myDRM_Face4Count  = countf4;
	myDRM_BottomCount = countbott;
	myDRM_Brd1        = countb1;
	myDRM_Brd2        = countb2;
	myDRM_Brd3        = countb3;
	myDRM_Brd4        = countb4;
	myDRM_Brd5        = countb5;
	myDRM_Brd6        = countb6;
	myDRM_Brd7        = countb7;
	myDRM_Brd8        = countb8;

	/*	fprintf(stdout,"myID = %d, myDRM_Face1Count= %d, myDRM_Face2Count= %d, myDRM_Face3Count= %d, myDRM_Face4Count= %d, myDRM_BottomCount=%d \n"
				       "myDRM_Brd1=%d, myDRM_Brd2=%d, myDRM_Brd3=%d, myDRM_Brd4=%d, myDRM_Brd5=%d, myDRM_Brd6=%d, myDRM_Brd7=%d, myDRM_Brd8=%d \n\n",
				       myID, countf1, countf2, countf3, countf4,countbott,countb1,countb2,countb3,countb4,countb5,countb6,countb7,countb8); */

}

void compute_addforce_PlaneWaves ( mesh_t     *myMesh,
                                mysolver_t *mySolver,
                                double      theDeltaT,
                                int         step,
                                fmatrix_t (*theK1)[8], fmatrix_t (*theK2)[8])
{

    int32_t   eindex;
    int32_t   face_eindex;

    double theDRMdepth	= theDRMBox_DepthElements * thedrmbox_esize;

    double thebase_zcoord = get_thebase_topo();

    int  f_nodes_face1[4] = { 0, 1, 4, 5 };
    int  e_nodes_face1[4] = { 2, 3, 6, 7 };

    int  f_nodes_face2[4] = { 2, 3, 6, 7 };
    int  e_nodes_face2[4] = { 0, 1, 4, 5 };

    int  f_nodes_face3[4] = { 1, 3, 5, 7 };
    int  e_nodes_face3[4] = { 0, 2, 4, 6 };

    int  f_nodes_face4[4] = { 0, 2, 4, 6 };
    int  e_nodes_face4[4] = { 1, 3, 5, 7 };

    int  f_nodes_bottom[4] = { 0, 1, 2, 3 };
    int  e_nodes_bottom[4] = { 4, 5, 6, 7 };

    int  f_nodes_border1[2] = { 1, 5 };
    int  e_nodes_border1[6] = { 0, 2, 3, 4, 6, 7 };

    int  f_nodes_border2[2] = { 0, 4 };
    int  e_nodes_border2[6] = { 1, 2, 3, 5, 6, 7 };

    int  f_nodes_border3[2] = { 3, 7 };
    int  e_nodes_border3[6] = { 0, 1, 2, 4, 5, 6 };

    int  f_nodes_border4[2] = { 2, 6 };
    int  e_nodes_border4[6] = { 0, 1, 3, 4, 5, 7 };

    int  f_nodes_border5[2] = { 0, 1 };
    int  e_nodes_border5[6] = { 2, 3, 4, 5, 6, 7 };

    int  f_nodes_border6[2] = { 2, 3 };
    int  e_nodes_border6[6] = { 0, 1, 4, 5, 6, 7 };

    int  f_nodes_border7[2] = { 1, 3 };
    int  e_nodes_border7[6] = { 0, 2, 4, 5, 6, 7 };

    int  f_nodes_border8[2] = { 0, 2 };
    int  e_nodes_border8[6] = { 1, 3, 4, 5, 6, 7 };

    double tt = theDeltaT * step;
    int  *f_nodes, *e_nodes;

    /* Loop over face1 elements */
    f_nodes = &f_nodes_face1[0];
    e_nodes = &e_nodes_face1[0];
    for ( face_eindex = 0; face_eindex < myDRM_Face1Count ; face_eindex++) {
    	eindex = myDRMFace1ElementsMapping[face_eindex];
    	DRM_ForcesinElement ( myMesh, mySolver, theK1, theK2, f_nodes, e_nodes, eindex, tt, 4, 4 );
    } /* all elements in face 1*/

    /* Loop over face2 elements */
    f_nodes = &f_nodes_face2[0];
    e_nodes = &e_nodes_face2[0];
    for ( face_eindex = 0; face_eindex < myDRM_Face2Count ; face_eindex++) {
    	eindex = myDRMFace2ElementsMapping[face_eindex];
    	DRM_ForcesinElement ( myMesh, mySolver, theK1, theK2, f_nodes, e_nodes, eindex, tt, 4, 4 );
    } /* all elements in face 2*/

    /* Loop over face3 elements */
    f_nodes = &f_nodes_face3[0];
    e_nodes = &e_nodes_face3[0];
    for ( face_eindex = 0; face_eindex < myDRM_Face3Count ; face_eindex++) {
    	eindex = myDRMFace3ElementsMapping[face_eindex];
    	DRM_ForcesinElement ( myMesh, mySolver, theK1, theK2, f_nodes, e_nodes, eindex, tt, 4, 4 );
    } /* all elements in face 3*/

    /* Loop over face4 elements */
    f_nodes = &f_nodes_face4[0];
    e_nodes = &e_nodes_face4[0];
    for ( face_eindex = 0; face_eindex < myDRM_Face4Count ; face_eindex++) {
    	eindex = myDRMFace4ElementsMapping[face_eindex];
    	DRM_ForcesinElement ( myMesh, mySolver, theK1, theK2, f_nodes, e_nodes, eindex, tt, 4, 4 );
    } /* all elements in face 4*/

    /* Loop over bottom elements */
    f_nodes = &f_nodes_bottom[0];
    e_nodes = &e_nodes_bottom[0];
    for ( face_eindex = 0; face_eindex < myDRM_BottomCount ; face_eindex++) {
    	eindex = myDRMBottomElementsMapping[face_eindex];
    	DRM_ForcesinElement ( myMesh, mySolver, theK1, theK2, f_nodes, e_nodes, eindex, tt, 4, 4 );
    } /* all elements in bottom*/

    /* Loop over border1 elements */
    f_nodes = &f_nodes_border1[0];
    e_nodes = &e_nodes_border1[0];
    for ( face_eindex = 0; face_eindex < myDRM_Brd1 ; face_eindex++) {
    	eindex = myDRMBorder1ElementsMapping[face_eindex];

    	/* check for bottom element */
    	elem_t        *elemp;
    	int32_t	      node0;
    	node_t        *node0dat;
    	double        zo;

    	elemp        = &myMesh->elemTable[eindex];
    	node0        = elemp->lnid[0];
    	node0dat     = &myMesh->nodeTable[node0];
    	zo           = (node0dat->z)*(myMesh->ticksize);

    	if ( zo != theDRMdepth + thebase_zcoord ) {
    		DRM_ForcesinElement ( myMesh, mySolver, theK1, theK2, f_nodes, e_nodes, eindex, tt, 6, 2 );
    	} else {
    	    int  f_corner[1] = { 1 };
    	    int  e_corner[7] = { 0, 2, 3, 4, 5, 6, 7 };
    	    int  *fcorner_nodes, *ecorner_nodes;
    	    fcorner_nodes = &f_corner[0];
    	    ecorner_nodes = &e_corner[0];
    	    DRM_ForcesinElement ( myMesh, mySolver, theK1, theK2, fcorner_nodes, ecorner_nodes, eindex, tt, 7, 1 );

    	}
    } /* all elements in border1*/

    /* Loop over border2 elements */
    f_nodes = &f_nodes_border2[0];
    e_nodes = &e_nodes_border2[0];
    for ( face_eindex = 0; face_eindex < myDRM_Brd2 ; face_eindex++) {
    	eindex = myDRMBorder2ElementsMapping[face_eindex];

    	/* check for bottom element */
    	elem_t        *elemp;
    	int32_t	      node0;
    	node_t        *node0dat;
    	double        zo;

    	elemp        = &myMesh->elemTable[eindex];
    	node0        = elemp->lnid[0];
    	node0dat     = &myMesh->nodeTable[node0];
    	zo           = (node0dat->z)*(myMesh->ticksize);

    	if ( zo != theDRMdepth + thebase_zcoord ) {
    		DRM_ForcesinElement ( myMesh, mySolver, theK1, theK2, f_nodes, e_nodes, eindex, tt, 6, 2 );
    	} else {
    	    int  f_corner[1] = { 0 };
    	    int  e_corner[7] = { 1, 2, 3, 4, 5, 6, 7 };
    	    int  *fcorner_nodes, *ecorner_nodes;
    	    fcorner_nodes = &f_corner[0];
    	    ecorner_nodes = &e_corner[0];
    	    DRM_ForcesinElement ( myMesh, mySolver, theK1, theK2, fcorner_nodes, ecorner_nodes, eindex, tt, 7, 1 );

    	}

    } /* all elements in border2*/

    /* Loop over border3 elements */
    f_nodes = &f_nodes_border3[0];
    e_nodes = &e_nodes_border3[0];
    for ( face_eindex = 0; face_eindex < myDRM_Brd3 ; face_eindex++) {
    	eindex = myDRMBorder3ElementsMapping[face_eindex];

    	/* check for bottom element */
    	elem_t        *elemp;
    	int32_t	      node0;
    	node_t        *node0dat;
    	double        zo;

    	elemp        = &myMesh->elemTable[eindex];
    	node0        = elemp->lnid[0];
    	node0dat     = &myMesh->nodeTable[node0];
    	zo           = (node0dat->z)*(myMesh->ticksize);

    	if ( zo != theDRMdepth + thebase_zcoord ) {
    		DRM_ForcesinElement ( myMesh, mySolver, theK1, theK2, f_nodes, e_nodes, eindex, tt, 6, 2 );
    	} else {
    	    int  f_corner[1] = { 3 };
    	    int  e_corner[7] = { 0, 1, 2, 4, 5, 6, 7 };
    	    int  *fcorner_nodes, *ecorner_nodes;
    	    fcorner_nodes = &f_corner[0];
    	    ecorner_nodes = &e_corner[0];
    	    DRM_ForcesinElement ( myMesh, mySolver, theK1, theK2, fcorner_nodes, ecorner_nodes, eindex, tt, 7, 1 );

    	}
    } /* all elements in border3*/

    /* Loop over border4 elements */
    f_nodes = &f_nodes_border4[0];
    e_nodes = &e_nodes_border4[0];
    for ( face_eindex = 0; face_eindex < myDRM_Brd4 ; face_eindex++) {
    	eindex = myDRMBorder4ElementsMapping[face_eindex];

    	/* check for bottom element */
    	elem_t        *elemp;
    	int32_t	      node0;
    	node_t        *node0dat;
    	double        zo;

    	elemp        = &myMesh->elemTable[eindex];
    	node0        = elemp->lnid[0];
    	node0dat     = &myMesh->nodeTable[node0];
    	zo           = (node0dat->z)*(myMesh->ticksize);

    	if ( zo != theDRMdepth + thebase_zcoord ) {
    		DRM_ForcesinElement ( myMesh, mySolver, theK1, theK2, f_nodes, e_nodes, eindex, tt, 6, 2 );
    	} else {
    	    int  f_corner[1] = { 3 };
    	    int  e_corner[7] = { 0, 1, 2, 4, 5, 6, 7 };
    	    int  *fcorner_nodes, *ecorner_nodes;
    	    fcorner_nodes = &f_corner[0];
    	    ecorner_nodes = &e_corner[0];
    	    DRM_ForcesinElement ( myMesh, mySolver, theK1, theK2, fcorner_nodes, ecorner_nodes, eindex, tt, 7, 1 );

    	}
    } /* all elements in border4*/

    /* Loop over border5 elements */
    f_nodes = &f_nodes_border5[0];
    e_nodes = &e_nodes_border5[0];
    for ( face_eindex = 0; face_eindex < myDRM_Brd5 ; face_eindex++) {
    	eindex = myDRMBorder5ElementsMapping[face_eindex];
    	DRM_ForcesinElement ( myMesh, mySolver, theK1, theK2, f_nodes, e_nodes, eindex, tt, 6, 2 );
    } /* all elements in border5*/

    /* Loop over border6 elements */
    f_nodes = &f_nodes_border6[0];
    e_nodes = &e_nodes_border6[0];
    for ( face_eindex = 0; face_eindex < myDRM_Brd6 ; face_eindex++) {
    	eindex = myDRMBorder6ElementsMapping[face_eindex];
    	DRM_ForcesinElement ( myMesh, mySolver, theK1, theK2, f_nodes, e_nodes, eindex, tt, 6, 2 );
    } /* all elements in border6*/

    /* Loop over border7 elements */
    f_nodes = &f_nodes_border7[0];
    e_nodes = &e_nodes_border7[0];
    for ( face_eindex = 0; face_eindex < myDRM_Brd7 ; face_eindex++) {
    	eindex = myDRMBorder7ElementsMapping[face_eindex];
    	DRM_ForcesinElement ( myMesh, mySolver, theK1, theK2, f_nodes, e_nodes, eindex, tt, 6, 2 );
    } /* all elements in border7*/

    /* Loop over border8 elements */
    f_nodes = &f_nodes_border8[0];
    e_nodes = &e_nodes_border8[0];
    for ( face_eindex = 0; face_eindex < myDRM_Brd8 ; face_eindex++) {
    	eindex = myDRMBorder8ElementsMapping[face_eindex];
    	DRM_ForcesinElement ( myMesh, mySolver, theK1, theK2, f_nodes, e_nodes, eindex, tt, 6, 2 );
    } /* all elements in border7*/


    return;
}


void DRM_ForcesinElement ( mesh_t     *myMesh,
		mysolver_t *mySolver,
		fmatrix_t (*theK1)[8], fmatrix_t (*theK2)[8],
		int *f_nodes, int *e_nodes, int32_t   eindex, double tt, int Nnodes_e, int Nnodes_f )
{

    int       i, j;
    int  CoordArrX[8]      = { 0, 1, 0, 1, 0, 1, 0, 1 };
    int  CoordArrY[8]      = { 0, 0, 1, 1, 0, 0, 1, 1 };
    int  CoordArrZ[8]      = { 0, 0, 0, 0, 1, 1, 1, 1 };

    double thebase_zcoord = get_thebase_topo();

	fvector_t localForce[8];

	elem_t        *elemp;
	edata_t       *edata;
	node_t        *node0dat;
	double        xo, yo, zo;
	int32_t	      node0;
	e_t*          ep;

	int        aux;
	double 	   remainder;

	/* Capture the table of elements from the mesh and the size
	 * This is what gives me the connectivity to nodes */
	elemp        = &myMesh->elemTable[eindex];
	edata        = (edata_t *)elemp->data;
	node0        = elemp->lnid[0];
	node0dat     = &myMesh->nodeTable[node0];
	ep           = &mySolver->eTable[eindex];

	/* get coordinates of element zero node */
	xo = (node0dat->x)*(myMesh->ticksize);
	yo = (node0dat->y)*(myMesh->ticksize);
	zo = (node0dat->z)*(myMesh->ticksize) - thebase_zcoord;

	/*
	myDisp.f[0] = theUg_NS[aux] + ( theUg_NS[aux + 1] - theUg_NS[aux] ) * remainder / theUg_Dt;
	myDisp.f[1] = theUg_EW[aux] + ( theUg_EW[aux + 1] - theUg_EW[aux] ) * remainder / theUg_Dt;
	myDisp.f[2] = 0.0; */

	/* get material properties  */
	double h    = (double)edata->edgesize;

	/* Force contribution from external nodes */
	/* -------------------------------
	 * Ku DONE IN THE CONVENTIONAL WAY
	 * ------------------------------- */
	memset( localForce, 0, 8 * sizeof(fvector_t) );

	fvector_t myDisp;

	/* forces over f nodes */
	for (i = 0; i < Nnodes_f; i++) {

		int  nodef = *(f_nodes + i);
		fvector_t* toForce = &localForce[ nodef ];

		/* incoming displacements over e nodes */
		for (j = 0; j < Nnodes_e; j++) {

			int  nodee = *(e_nodes + j);

			double x_ne = xo + h * CoordArrX[ nodee ];   /* get xcoord */
			double y_ne = yo + h * CoordArrY[ nodee ];   /* get ycoord */
			double z_ne = zo + h * CoordArrZ[ nodee ];   /* get zcoord */

			//if ( theFncType == RICK )
			Incoming_inclinedPW (  &myDisp, x_ne - theXc ,  y_ne - theYc, z_ne, tt, edata->Vs, edata->Vp  );
			//else {
				//Ricker_inclinedPW (  &myDisp, x_ne - theXc ,  y_ne - theYc, z_ne, tt, edata->Vs, edata->Vp  );
	            /* fprintf(stderr,"Need to work on this \n");
	            MPI_Abort(MPI_COMM_WORLD, ERROR);
	            exit(1); */
	       // }

			MultAddMatVec( &theK1[ nodef ][ nodee ], &myDisp, -ep->c1, toForce );
			MultAddMatVec( &theK2[ nodef ][ nodee ], &myDisp, -ep->c2, toForce );
		}
	}

	/* forces over e nodes */
	for (i = 0; i < Nnodes_e; i++) {

		int  nodee = *(e_nodes + i);
		fvector_t* toForce = &localForce[ nodee ];

		/* incoming displacements over f nodes */
		for (j = 0; j < Nnodes_f; j++) {

			int  nodef = *(f_nodes + j);

			double x_nf = xo + h * CoordArrX[ nodef ];   /* get xcoord */
			double y_nf = yo + h * CoordArrY[ nodef ];   /* get ycoord */
			double z_nf = zo + h * CoordArrZ[ nodef ];   /* get zcoord */

			Incoming_inclinedPW (  &myDisp, x_nf - theXc ,  y_nf - theYc, z_nf, tt, edata->Vs, edata->Vp  );

			MultAddMatVec( &theK1[ nodee ][ nodef ], &myDisp, ep->c1, toForce );
			MultAddMatVec( &theK2[ nodee ][ nodef ], &myDisp, ep->c2, toForce );
		}
	}
	/* end Ku */

	/* Loop over the 8 element nodes:
	 * Add the contribution calculated above to the node
	 * forces carried from the source and stiffness.
	 */
	for (i = 0; i < 8; i++) {

		int32_t    lnid;
		fvector_t *nodalForce;

		lnid = elemp->lnid[i];

		nodalForce = mySolver->force + lnid;

		nodalForce->f[0] += localForce[i].f[0] ;
		nodalForce->f[1] += localForce[i].f[1] ;
		nodalForce->f[2] += localForce[i].f[2] ;

	} /* element nodes */

}

void getRicker ( fvector_t *myDisp, double zp, double t, double Vs ) {

	double Rz = Ricker_displ ( zp, theTs, t, thefc, Vs  ) ;

	if ( thePlaneWaveType == SV1 ) {
		myDisp->f[0] = Rz * theUo * cos (theplanewave_strike);
		myDisp->f[1] = Rz * theUo * sin (theplanewave_strike);
		myDisp->f[2] = 0.0;
	} else {
		myDisp->f[0] = 0.0;
		myDisp->f[1] = 0.0;
		myDisp->f[2] = Rz * theUo;
	}

}

double Ricker_displ ( double zp, double Ts, double t, double fc, double Vs  ) {

	double alfa1 = ( PI * fc ) * ( PI * fc ) * ( t - zp / Vs - Ts) * ( t - zp / Vs - Ts);
	double alfa2 = ( PI * fc ) * ( PI * fc ) * ( t + zp / Vs - Ts) * ( t + zp / Vs - Ts);

	double uo1 = ( 2.0 * alfa1 - 1.0 ) * exp(-alfa1);
	double uo2 = ( 2.0 * alfa2 - 1.0 ) * exp(-alfa2);

	return (uo1+uo2);
}


void Incoming_inclinedPW ( fvector_t *myDisp, double xp, double yp, double zp, double t, double Vs, double Vp  ) {

	// propagation vectors
	double c, f, e, A1, B1, t_inc, t_pref, t_sref, outcrop_fact;
	double Ug_inc, Ug_pref, Ug_sref, rem_tinc, rem_tpref, rem_tsref;

	int aux_tinc, aux_tpref, aux_tsref;

	double p_inc[3]  = {0.0}; // propagation vector of the incident wave
	double p_pref[3] = {0.0}; // propagation vector of the reflected p-wave
	double p_sref[3] = {0.0}; // propagation vector of the reflected s-wave

	double u_inc[3]  = {0.0}; // displ vector of the incident wave
	double u_pref[3] = {0.0}; // displ vector of the reflected p-wave
	double u_sref[3] = {0.0}; // displ vector of the reflected s-wave

	get_reflection_coeff ( &A1, &B1, Vs, Vp  );

	if ( thePlaneWaveType == SV1 ) {
		c = Vs;
		f = theplanewave_Zangle;
		e = asin( Vp / Vs * sin( f ) );

		p_inc[0] =  sin( f ) * cos (theplanewave_strike);
		p_inc[1] =  sin( f ) * sin (theplanewave_strike);
		p_inc[2] = -cos( f );

		p_pref[0] = sin( e ) * cos (theplanewave_strike);
		p_pref[1] = sin( e ) * sin (theplanewave_strike);
		p_pref[2] = cos( e );

		p_sref[0] = sin( f ) * cos (theplanewave_strike);
		p_sref[1] = sin( f ) * sin (theplanewave_strike);
		p_sref[2] = cos( f );

		u_inc[0] =  cos( f ) * cos (theplanewave_strike);
		u_inc[1] =  cos( f ) * sin (theplanewave_strike);
		u_inc[2] =  sin( f );

		u_pref[0] = sin( e ) * cos (theplanewave_strike);
		u_pref[1] = sin( e ) * sin (theplanewave_strike);
		u_pref[2] = cos( e );

		u_sref[0] = -cos( f ) * cos (theplanewave_strike);
		u_sref[1] = -cos( f ) * sin (theplanewave_strike);
		u_sref[2] =  sin( f );

		outcrop_fact = cos(f) + A1 * sin (e) - B1 * cos (f);

	} else {
		c = Vp;
		e = theplanewave_Zangle;
	    f = asin( Vs / Vp * sin( e ) );

		p_inc[0] =  sin( e ) * cos (theplanewave_strike);
		p_inc[1] =  sin( e ) * sin (theplanewave_strike);
		p_inc[2] = -cos( e );

		p_pref[0] = sin( e ) * cos (theplanewave_strike);
		p_pref[1] = sin( e ) * sin (theplanewave_strike);
		p_pref[2] = cos( e );

		p_sref[0] = sin( f ) * cos (theplanewave_strike);
		p_sref[1] = sin( f ) * sin (theplanewave_strike);
		p_sref[2] = cos( f );

		u_inc[0] =   sin( e ) * cos (theplanewave_strike);
		u_inc[1] =   sin( e ) * sin (theplanewave_strike);
		u_inc[2] =  -cos( e );

		u_pref[0] = sin( e ) * cos (theplanewave_strike);
		u_pref[1] = sin( e ) * sin (theplanewave_strike);
		u_pref[2] = cos( e );

		u_sref[0] = -cos( f ) * cos (theplanewave_strike);
		u_sref[1] = -cos( f ) * sin (theplanewave_strike);
		u_sref[2] =  sin( f );

		outcrop_fact = sin(e) + A1 * sin (e) - B1 * cos (f);
	}

	if ( theFncType == RICK ) {
		double alfa_inc  = ( PI * thefc ) * ( PI * thefc ) * ( t - ( xp*p_inc[0]  + yp*p_inc[1]  + zp*p_inc[2])/c   - theTs ) * ( t - ( xp*p_inc[0]  + yp*p_inc[1]  + zp*p_inc[2] )/c   - theTs ) ; // incident
		double alfa_pref = ( PI * thefc ) * ( PI * thefc ) * ( t - ( xp*p_pref[0] + yp*p_pref[1] + zp*p_pref[2])/Vp - theTs ) * ( t - ( xp*p_pref[0] + yp*p_pref[1] + zp*p_pref[2] )/Vp - theTs ) ; // p_reflected
		double alfa_sref = ( PI * thefc ) * ( PI * thefc ) * ( t - ( xp*p_sref[0] + yp*p_sref[1] + zp*p_sref[2])/Vs - theTs ) * ( t - ( xp*p_sref[0] + yp*p_sref[1] + zp*p_sref[2] )/Vs - theTs ) ; // s_reflected

		double Rick_inc  = ( 2.0 * alfa_inc  - 1.0 ) * exp(-alfa_inc);
		double Rick_pref = ( 2.0 * alfa_pref - 1.0 ) * exp(-alfa_pref);
		double Rick_sref = ( 2.0 * alfa_sref - 1.0 ) * exp(-alfa_sref);

		myDisp->f[0] = ( Rick_inc * u_inc[0] + A1 * Rick_pref * u_pref[0] + B1 * Rick_sref * u_sref[0] ) * theUo;
		myDisp->f[1] = ( Rick_inc * u_inc[1] + A1 * Rick_pref * u_pref[1] + B1 * Rick_sref * u_sref[1] ) * theUo;
		myDisp->f[2] = ( Rick_inc * u_inc[2] + A1 * Rick_pref * u_pref[2] + B1 * Rick_sref * u_sref[2] ) * theUo;
	} else {

		t_inc  = t - ( xp*p_inc[0]  + yp*p_inc[1]  + zp*p_inc[2]  ) / c  - theTs;
		t_pref = t - ( xp*p_pref[0] + yp*p_pref[1] + zp*p_pref[2] ) / Vp - theTs;
		t_sref = t - ( xp*p_sref[0] + yp*p_sref[1] + zp*p_sref[2] ) / Vs - theTs;

		aux_tinc    = (int)( t_inc / theUg_Dt);
		rem_tinc = t - aux_tinc * theUg_Dt;

		if ( aux_tinc < 0 || t_inc < 0.0 )
			Ug_inc = 0.0;
		else
			Ug_inc  = theUg_str[aux_tinc]  + ( theUg_str[aux_tinc  + 1 ] - theUg_str[aux_tinc]  ) * rem_tinc / theUg_Dt;

		int aux_tpref    = (int)( t_pref / theUg_Dt);
		double rem_tpref = t - aux_tpref * theUg_Dt;

		if ( aux_tpref < 0 || t_pref < 0.0 )
			Ug_pref = 0.0;
		else
			Ug_pref = theUg_str[aux_tpref] + ( theUg_str[aux_tpref + 1 ] - theUg_str[aux_tpref] ) * rem_tpref / theUg_Dt;

		int aux_tsref    = (int)( t_sref / theUg_Dt);
		double rem_tsref = t - aux_tsref * theUg_Dt;

		if ( aux_tpref < 0 || t_sref < 0.0 )
			Ug_sref = 0.0;
		else
			Ug_sref = theUg_str[aux_tsref] + ( theUg_str[aux_tsref + 1 ] - theUg_str[aux_tsref] ) * rem_tsref / theUg_Dt;

		myDisp->f[0] = ( Ug_inc * u_inc[0] + A1 * Ug_pref * u_pref[0] + B1 * Ug_sref * u_sref[0] ) / outcrop_fact;
		myDisp->f[1] = ( Ug_inc * u_inc[1] + A1 * Ug_pref * u_pref[1] + B1 * Ug_sref * u_sref[1] ) / outcrop_fact;
		myDisp->f[2] = ( Ug_inc * u_inc[2] + A1 * Ug_pref * u_pref[2] + B1 * Ug_sref * u_sref[2] ) / outcrop_fact;


		// compute the contribution of the normal component of ground motion
		if ( the_NoComp == 2 ) {
			if ( thePlaneWaveType == SV1 ) {
				c = Vs;
				f = theplanewave_Zangle;
				e = asin( Vp / Vs * sin( f ) );

				p_inc[0] =  sin( f ) * cos (theplanewave_strike + PI/2.0 );
				p_inc[1] =  sin( f ) * sin (theplanewave_strike + PI/2.0);
				p_inc[2] = -cos( f );

				p_pref[0] = sin( e ) * cos (theplanewave_strike + PI/2.0);
				p_pref[1] = sin( e ) * sin (theplanewave_strike + PI/2.0);
				p_pref[2] = cos( e );

				p_sref[0] = sin( f ) * cos (theplanewave_strike + PI/2.0);
				p_sref[1] = sin( f ) * sin (theplanewave_strike + PI/2.0);
				p_sref[2] = cos( f );

				u_inc[0] =  cos( f ) * cos (theplanewave_strike + PI/2.0);
				u_inc[1] =  cos( f ) * sin (theplanewave_strike + PI/2.0);
				u_inc[2] =  sin( f );

				u_pref[0] = sin( e ) * cos (theplanewave_strike + PI/2.0);
				u_pref[1] = sin( e ) * sin (theplanewave_strike + PI/2.0);
				u_pref[2] = cos( e );

				u_sref[0] = -cos( f ) * cos (theplanewave_strike + PI/2.0);
				u_sref[1] = -cos( f ) * sin (theplanewave_strike + PI/2.0);
				u_sref[2] =  sin( f );

				outcrop_fact = cos(f) + A1 * sin (e) - B1 * cos (f);

			} else {
				c = Vp;
				e = theplanewave_Zangle;
				f = asin( Vs / Vp * sin( e ) );

				p_inc[0] =  sin( e ) * cos (theplanewave_strike + PI/2.0);
				p_inc[1] =  sin( e ) * sin (theplanewave_strike + PI/2.0);
				p_inc[2] = -cos( e );

				p_pref[0] = sin( e ) * cos (theplanewave_strike + PI/2.0);
				p_pref[1] = sin( e ) * sin (theplanewave_strike + PI/2.0);
				p_pref[2] = cos( e );

				p_sref[0] = sin( f ) * cos (theplanewave_strike + PI/2.0);
				p_sref[1] = sin( f ) * sin (theplanewave_strike + PI/2.0);
				p_sref[2] = cos( f );

				u_inc[0] =   sin( e ) * cos (theplanewave_strike + PI/2.0);
				u_inc[1] =   sin( e ) * sin (theplanewave_strike + PI/2.0);
				u_inc[2] =  -cos( e );

				u_pref[0] = sin( e ) * cos (theplanewave_strike + PI/2.0);
				u_pref[1] = sin( e ) * sin (theplanewave_strike + PI/2.0);
				u_pref[2] = cos( e );

				u_sref[0] = -cos( f ) * cos (theplanewave_strike + PI/2.0);
				u_sref[1] = -cos( f ) * sin (theplanewave_strike + PI/2.0);
				u_sref[2] =  sin( f );

				outcrop_fact = sin(e) + A1 * sin (e) - B1 * cos (f);
			}

			t_inc  = t - ( xp*p_inc[0]  + yp*p_inc[1]  + zp*p_inc[2]  ) / c  - theTs;
			t_pref = t - ( xp*p_pref[0] + yp*p_pref[1] + zp*p_pref[2] ) / Vp - theTs;
			t_sref = t - ( xp*p_sref[0] + yp*p_sref[1] + zp*p_sref[2] ) / Vs - theTs;

			aux_tinc    = (int)( t_inc / theUg_Dt);
			rem_tinc = t - aux_tinc * theUg_Dt;

			if ( aux_tinc < 0 || t_inc < 0.0 )
				Ug_inc = 0.0;
			else
				Ug_inc  = theUg_nrm[aux_tinc]  + ( theUg_nrm[aux_tinc  + 1 ] - theUg_nrm[aux_tinc]  ) * rem_tinc / theUg_Dt;

			aux_tpref    = (int)( t_pref / theUg_Dt);
			rem_tpref = t - aux_tpref * theUg_Dt;

			if ( aux_tpref < 0 || t_pref < 0.0 )
				Ug_pref = 0.0;
			else
				Ug_pref = theUg_nrm[aux_tpref] + ( theUg_nrm[aux_tpref + 1 ] - theUg_nrm[aux_tpref] ) * rem_tpref / theUg_Dt;

			aux_tsref    = (int)( t_sref / theUg_Dt);
			rem_tsref = t - aux_tsref * theUg_Dt;

			if ( aux_tpref < 0 || t_sref < 0.0 )
				Ug_sref = 0.0;
			else
				Ug_sref = theUg_nrm[aux_tsref] + ( theUg_nrm[aux_tsref + 1 ] - theUg_nrm[aux_tsref] ) * rem_tsref / theUg_Dt;

			myDisp->f[0] += ( Ug_inc * u_inc[0] + A1 * Ug_pref * u_pref[0] + B1 * Ug_sref * u_sref[0] ) / outcrop_fact;
			myDisp->f[1] += ( Ug_inc * u_inc[1] + A1 * Ug_pref * u_pref[1] + B1 * Ug_sref * u_sref[1] ) / outcrop_fact;
			myDisp->f[2] += ( Ug_inc * u_inc[2] + A1 * Ug_pref * u_pref[2] + B1 * Ug_sref * u_sref[2] ) / outcrop_fact;
		}
	}
}


void get_reflection_coeff ( double *A1, double *B1, double Vs, double Vp  ) {

	double fcr, f, e;

	if ( thePlaneWaveType == SV1 ) {

		 fcr = asin(Vs/Vp);        // critical angle
		 f = theplanewave_Zangle;
		 e = asin( Vp / Vs * sin( f ) );

	    if ( theplanewave_Zangle > fcr ) {
	        fprintf(stderr, "Vertical angle greater than critical %f \n", theplanewave_Zangle);
	    	MPI_Abort(MPI_COMM_WORLD,ERROR);
	    	exit(1);
	    }

		*A1 = ( Vp / Vs ) * sin (4.0 * f) / ( sin ( 2.0 * e ) * sin ( 2.0 * f ) + ( Vp / Vs ) * ( Vp / Vs ) * cos ( 2.0 * f ) * cos ( 2.0 * f ) );
		*B1 = ( sin ( 2.0 * e ) * sin ( 2.0 * f ) - ( Vp / Vs ) * ( Vp / Vs ) * cos ( 2.0 * f ) * cos ( 2.0 * f ) ) / ( sin ( 2.0 * e ) * sin ( 2.0 * f ) + ( Vp / Vs ) * ( Vp / Vs ) * cos ( 2.0 * f ) * cos ( 2.0 * f ) );

	} else {
		 e = theplanewave_Zangle;
		 f = asin( Vs / Vp * sin( e ) );

		*A1 =  ( sin ( 2.0 * e ) * sin ( 2.0 * f ) - ( Vp / Vs ) * ( Vp / Vs ) * cos ( 2.0 * f ) * cos ( 2.0 * f ) ) / ( sin ( 2.0 * e ) * sin ( 2.0 * f ) + ( Vp / Vs ) * ( Vp / Vs ) * cos ( 2.0 * f ) * cos ( 2.0 * f ) );
		*B1 = - 2.0 * ( Vp / Vs ) * sin (2.0 * e) * cos (2.0 * f) / ( sin ( 2.0 * e ) * sin ( 2.0 * f ) + ( Vp / Vs ) * ( Vp / Vs ) * cos ( 2.0 * f ) * cos ( 2.0 * f ) );

	}

}

/*double time_shift ( double Vs, double Vp ) {

	int i, j;
	double p_inc[3]  = {0.0}; // propagation angle of the incident wave
	//double p_pref[3] = {0.0}; // propagation angle of the reflected p-wave
	//double p_sref[3] = {0.0}; // propagation angle of the reflected s-wave
	//double e, f, time_shft[8] = { 0.0 }, c;
	double time_shft[8] = { 0.0 }, c;

	double t_shft=0.0;

	double DRM_EW = theDRMBox_halfwidthElements_ew * thedrmbox_esize;
	double DRM_NS = theDRMBox_halfwidthElements_ns * thedrmbox_esize;
	double DRM_D  = theDRMBox_DepthElements * thedrmbox_esize;

	double drm_corners[3][8] = { { DRM_NS,  DRM_NS, -DRM_NS,  -DRM_NS,  DRM_NS,  DRM_NS, -DRM_NS,  -DRM_NS} , \
                                 {-DRM_EW,  DRM_EW,  DRM_EW,  -DRM_EW, -DRM_EW,  DRM_EW,  DRM_EW,  -DRM_EW} , \
                                 {    0.0,     0.0,     0.0,      0.0,   DRM_D,   DRM_D,   DRM_D,    DRM_D} };

	p_inc[0] =  sin( theplanewave_Zangle ) * cos (theplanewave_strike);
	p_inc[1] =  sin( theplanewave_Zangle ) * sin (theplanewave_strike);
	p_inc[2] = -cos( theplanewave_Zangle );


	if ( thePlaneWaveType == SV1 ) {
		c = Vs;
		//f = theplanewave_Zangle;
		//e = asin( Vp / Vs * sin( f ) );

		 p_inc[0] =  sin( f ) * cos (theplanewave_strike);
		p_inc[1] =  sin( f ) * sin (theplanewave_strike);
		p_inc[2] = -cos( f );

		 p_pref[0] = sin( e ) * cos (theplanewave_strike);
		p_pref[1] = sin( e ) * sin (theplanewave_strike);
		p_pref[2] = cos( e );

		p_sref[0] = sin( f ) * cos (theplanewave_strike);
		p_sref[0] = sin( f ) * sin (theplanewave_strike);
		p_sref[2] = cos( f );

		 for (i = 0; i < 8; i++) {
			for (j = 0; j < 3; j++) {
				time_shft[i] += p_inc[j] * drm_corners[j][i] / Vs;
			}
		}

	} else {
		c = Vp;
		 e = theplanewave_Zangle;
		//f = asin( Vs / Vp * sin( e ) );
		p_inc[0] =  sin( e ) * cos (theplanewave_strike);
		p_inc[1] =  sin( e ) * sin (theplanewave_strike);
		p_inc[2] = -cos( e );
	}

	for (i = 0; i < 8; i++) {
		for (j = 0; j < 3; j++) {
			time_shft[i] += p_inc[j] * drm_corners[j][i] / c;
		}
	}

	for (i = 0; i < 8; i++)
		t_shft = MIN( t_shft, time_shft[i] );


	return t_shft;

}*/





