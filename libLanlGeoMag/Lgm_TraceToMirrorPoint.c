/*! \file Lgm_TraceToMirrorPoint.c
 *
 *  \brief Routines to find the mirror point location along a field line (for a given ptich angle).
 *
 *
 *
 *  \author M.G. Henderson
 *  \date   2004
 *
 *
 *
 */


/* TraceToMirrorPoint, Copyright (c) 2004 Michael G. Henderson <mghenderson@lanl.gov>
 *
 *    - Starting at Minimum-B point, and given a value of mu_eq (mu = cos(pitch angle)),
 *	trace up field line to the particle's mirror point. The particle will mirror
 *     	when B = Beq/(1-mu_eq^2)
 *
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 *
 * Feb 6, 2012 -- took out the eps parameter in MagStep (now fed by vals in
 * structure). But in this routine we had eps = 1e-10 hardcoded). Check to see it still works.
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "Lgm/Lgm_MagModelInfo.h"
#if USE_OPENMP
#include <omp.h>
#endif

#define LC_TOL  0.99    // Allow height to be as low as .99*Lgm_LossConeHeight, before we call it "in the loss cone"


double mpFunc( Lgm_Vector *P, double Bm, Lgm_MagModelInfo *Info ){

    Lgm_Vector  Bvec;
    double      F;

    Info->Bfield( P, &Bvec, Info );
    F = Lgm_Magnitude( &Bvec ) - Bm;
    return( F );

}

int Lgm_TraceToMirrorPoint( Lgm_Vector *u, Lgm_Vector *v, double *Sm, double Bm, double sgn, double tol, Lgm_MagModelInfo *Info ) {

    Lgm_Vector	u_scale;
    double	    Htry, Hdid, Hnext, Hmin, Hmax, s;
    double	    Sa=0.0, Sb=0.0, Smin, d;
    double	    Rlc, R, Fa, Fb, F, Fmin, B, Fs, Fn;
    double	    Ra, Rb, Height;
    Lgm_Vector	w, Pa, Pb, P, Bvec, Pmin;
    int		    done, FoundBracket, reset, nIts, nSteps;
    double      MinValidHeight;

    reset = TRUE;
    Fmin = 9e99;
    MinValidHeight = ( Info->Lgm_LossConeHeight < 0.0 ) ? Info->Lgm_LossConeHeight : 0.0;

    if ( Info->VerbosityLevel > 4 ) {
        printf( "\n\n**************** Start Detailed Output From TraceToMirrorPoint (VerbosityLevel = %d) ******************\n", Info->VerbosityLevel );
    }


    *Sm = 0.0;

    /*
     *  If particle mirrors below Info->Lgm_LossConeHeight, we are in the loss cone.
     */
    Lgm_Convert_Coords( u, &w, GSM_TO_WGS84, Info->c );
    Lgm_WGS84_to_GeodHeight( &w, &Height );
    if ( Height < LC_TOL*Info->Lgm_LossConeHeight ) {
        if ( Info->VerbosityLevel > 1 )
        printf("    Lgm_TraceToMirrorPoint: Initial Height is below specified loss cone height of %g km. In Loss Cone. (Height = %g) \n", Info->Lgm_LossConeHeight, Height );
        return(-1); // below loss cone height -> particle is in loss cone!
    }


    /*
     * Put in some logic to bail if the initial point is below Bm (i.e. if B is
     * significantly higher than Bm). This ammounts to requiring that the input
     * point is at least on a part of the F.L. accessible to bouncing particles
     * (that have Bm as specified.)
     */





    Hmax = Info->Hmax;
    Hmin = 1e-10;
    u_scale.x = u_scale.y = u_scale.z = 1.0;
    R = Ra = Rb = 0.0;
    F = Fa = Fb = 0.0;




    /*
     *  Bracket the root first.
     *  We are attempting to find a root in the direction given to us (via ithe
     *  sgn value).  We want to find where F=0 (i.e. where B = Bm).  Thus we
     *  need a bracket [Pa, Pb] such that Fa is negative and Pb is positive.
     *  Ideally, we would like to start at Bmin and trace up or down to get the
     *  mirror points. However, we could easily have a situation where the
     *  input point is already at (or very close) to one of the mirror points.
     *  For example, if we put in the S/C location, locally mirroring particles
     *  (PA=90) will have a mirror point at exactly that location and another
     *  one at some point beyond the Bmin point. To find the other root, we
     *  first need to get a point Pa between them (so that Fa<0). When the
     *  input point is numerically very close to a mirror point, F will be +/-
     *  a very small number, so it cant be used as the first point of the
     *  bracket. Instead, we need to step a tiny bit to get the first bracket.
     */
    Info->Bfield( u, &Bvec, Info );
    Pa = *u;
    Pb.x = Pb.y = Pb.z = 0.0;
    Ra = Lgm_Magnitude( &Pa );
    Fa = Lgm_Magnitude( &Bvec ) - Bm;
    Sa = 0.0;
    if ( Info->VerbosityLevel > 4 )  {
        printf("    TraceToMirrorPoint: Starting Point: %15g %15g %15g   R, F (=B-Bm) = %g %g   B, Bm = %g %g\n\n", u->x, u->y, u->z, Ra, Fa, Lgm_Magnitude( &Bvec ), Bm );
    }




    /*
     *  To proceed, we need to find a point to start the bracket on (i.e. a
     *  point where B-Bm < 0).  Then we need to trace until the sign changes
     *  (or we bail due to hitting earth or because its open or whatever.) Note
     *  that B-Bm may not be negative if we are at or above a mirror point
     *  already. If Fa is not < 0, we need to try and step a bit to see if we
     *  can get such a point.
     *
     */
    if ( Fa > 0.0 ) {

        // First, try a moderately small step in the user-supplied direction.
        Htry = 0.1; // Some mirror point pairs may be closer thogether than this. If we fail, we need to try with a smaller value here.
        P = *u;
        if ( Lgm_MagStep( &P, &u_scale, Htry, &Hdid, &Hnext, sgn, &s, &reset, Info->Bfield, Info ) < 0 ) return( LGM_BAD_TRACE );
        Info->Bfield( &P, &Bvec, Info );
        B = Lgm_Magnitude( &Bvec );
        F = B-Bm;
        if (fabs(F)<Fmin) { Fmin = fabs(F); Pmin = P; }

        if ( F < 0.0 ) {

            /*
             *  If F < 0.0, then it means |B| is still less than Bm in this
             *  direction.  Save this point as the start of a potential bracket.
             *  Then we can continue tracing in this direction to getm the
             *  other side of the bracket.
             */
            Pa  = P;
            Ra  = Lgm_Magnitude( &P );
            Sa  = Hdid;
            Fa  = F;

        } else {

            /*
             *  We tried a moderately small step size and didnt find a decreasing
             *  |B|. It could be that we need a very small step size to split mirror
             *  points that a very close together.
             */
            Htry = 1e-6; // we probably dont ever need to split the mirror points to any finer precision than this(?).
            P    = *u;
            if ( Lgm_MagStep( &P, &u_scale, Htry, &Hdid, &Hnext, sgn, &s, &reset, Info->Bfield, Info ) < 0 ) return( LGM_BAD_TRACE );
            Info->Bfield( &P, &Bvec, Info );
            B = Lgm_Magnitude( &Bvec );
            F = B-Bm;
            if (fabs(F)<Fmin) { Fmin = fabs(F); Pmin = P; }

            if ( F < 0.0 ) {

                /*
                 *  If F < 0.0, then it means |B| is still less than Bm in this
                 *  direction.  Save this point as the start of a potential bracket.
                 *  Then continue tracing in this direction.
                 */
                Pa  = P;
                Ra  = Lgm_Magnitude( &P );
                Sa  = Hdid;
                Fa  = F;

            } else {

                /*
                 * Even with a small step size in this direction, |B| was not found to
                 * be less than Bm. This could mean that we are very close to the Pmin
                 * point. Since we are trusting the user that this really is the
                 * direction that the other root should have been found in, we must
                 * conclude that the roots are very closely separated around Pmin. Just
                 * return the input point back as the output point.
                 */
                 *v  = *u;
                 *Sm = 0.0;
                 return( 1 );


            }

        }
    }




    // Allow larger step sizes. Also, we can go beyond Bm to get the bracket.
    Hmax = 0.5;
    Htry = 0.25;
    P    = Pa;

    /*
     *  To begin with, B - Bm will be negative (we wouldnt be here otherwise). So all we need to do is
     *  trace along the F.L. until B - Bm changes sign. That will give us the far side of the bracket.
     *  But dont go below surface of the Earth, etc.
     */
    done         = FALSE;
    FoundBracket = FALSE;
    nSteps       = 0;
    while ( !done ) {

//printf("P = %g %g %g   u_scale=%g %g %g\n", P.x, P.y, P.z, u_scale.x, u_scale.y, u_scale.z);
        if ( Info->VerbosityLevel > 4 ) {
            printf("    TraceToMirrorPoint, Finding Bracket: Starting P: %15g %15g %15g\n", P.x, P.y, P.z );
        }

        /*
         *  If user doesnt want large steps, limit Htry ...
         */
        if (Htry > Hmax) Htry = Hmax;

        if ( Lgm_MagStep( &P, &u_scale, Htry, &Hdid, &Hnext, sgn, &s, &reset, Info->Bfield, Info ) < 0 ) return(-1);


        /*
         *  Get value of quantity we want to minimize
         */
        Info->Bfield( &P, &Bvec, Info );
//printf("P = %.15lf %.15lf %.15lf    Bvec = %g %g %g \n", P.x, P.y, P.z, Bvec.x, Bvec.y, Bvec.z );
        R = Lgm_Magnitude( &P );
        F = Lgm_Magnitude( &Bvec ) - Bm;
        if (fabs(F)<Fmin) { Fmin = fabs(F); Pmin = P; }
        Lgm_Convert_Coords( &P, &w, GSM_TO_WGS84, Info->c );
        Lgm_WGS84_to_GeodHeight( &w, &Height );


        if (   (P.x > Info->OpenLimit_xMax) || (P.x < Info->OpenLimit_xMin) || (P.y > Info->OpenLimit_yMax) || (P.y < Info->OpenLimit_yMin)
                || (P.z > Info->OpenLimit_zMax) || (P.z < Info->OpenLimit_zMin) || ( Sa > 300.0 ) ) {
            /*
             *  Open FL!
             */
            v->x = v->y = v->z = 0.0;
            if ( Info->VerbosityLevel > 1 ) {
                printf("    Lgm_TraceToMirrorPoint(): FL OPEN\n");
            }
            if ( Info->VerbosityLevel > 4 ) {
                printf( "**************** End Detailed Output From TraceToMirrorPoint (VerbosityLevel = %d) ******************\n\n\n", Info->VerbosityLevel );
            }
            return(-2);

        } else if ( F > 0.0 ) { /* not >= because we want to explore at least a step beyond */

            done = TRUE;
            FoundBracket = TRUE;
            Pb = P;
            Rb = R;
            Fb = F;
            Sb = Sa + Hdid;

        } else {

            Pa = P;
            Ra = R;
            Fa = F;
            Sa += Hdid;
//printf("Sa = %g   Fa = %g\n", Sa, Fa);

        }



        // Set Htry adaptively. But make sure we wont descend below the Earth's surface.
        // WARNING: This will be bad for Shabansky type situations where we have a double minimum....
        // Perhaps whether to adaptively do this should be controlable. If we have trouble, we can restrict the stepsize...
        Htry = Hnext;
if (Htry > 0.1) Htry = 0.1;
double Rvalidmin = 1.0 + MinValidHeight/Re;
        //if (Htry > (R-1.0)) Htry = 0.95*(R-1.0);
        if (Htry > (R-Rvalidmin)) Htry = 0.95*(R-Rvalidmin);
if (Htry < 0.01) Htry = 0.01;


        // If Htry gets to be too small, there is probably something wrong.
        if (Htry < 1e-12) done = TRUE;


        if ( Info->VerbosityLevel > 4 ) {
            printf("                                             Got To: %15g %15g %15g     with Htry of: %g\n", P.x, P.y, P.z, Htry );
            printf("        Pa, Ra, Fa, Sa = (%15g, %15g, %15g) %15g %15g %15g\n", Pa.x, Pa.y, Pa.z, Ra, Fa, Sa  );
            printf("        Pb, Rb, Fb, Sb = (%15g, %15g, %15g) %15g %15g %15g\n", Pb.x, Pb.y, Pb.z, Rb, Fb, Sb  );
            printf("        F = %g, |B| Bm = %g %g FoundBracket = %d  done = %d    Current Height = %g Htry = %g\n\n", F, Lgm_Magnitude( &Bvec ), Bm, FoundBracket, done, Height, Htry );
        }

        //if ( Height < 0.0 ) {
        if ( Height < MinValidHeight ) {

            if ( Info->VerbosityLevel > 1 ) {
                //printf("    Lgm_TraceToMirrorPoint: Current Height is below surface of the Earth while finding bracket. (Height = %g) \n", Height );
                printf("    Lgm_TraceToMirrorPoint: Current Height is below surface of the Earth while finding bracket. (Height = %g) \n", Height );
            }
            if ( Info->VerbosityLevel > 4 ) {
                printf( "**************** End Detailed Output From TraceToMirrorPoint (VerbosityLevel = %d) ******************\n\n\n", Info->VerbosityLevel );
            }
            return(-1); /* dropped below surface of the Earth */
        }

        ++nSteps;

        if ( nSteps > 1000 ) {
            if ( Info->VerbosityLevel > 1 ) {
                printf("    Lgm_TraceToMirrorPoint: Took too many steps to find bracket. (nSteps = %d) \n", nSteps );
            }
            return(-3);
        }

    }


//printf("TRACE: Fa, Fb = %g %g\n", Fa, Fb);

    /*
     * We have found a potential bracket, but lets just be sure.
     */
//printf( "FoundBracket = %d\n", FoundBracket);
//printf( "Fa, Fb = %g %g\n", Fa, Fb );
    if ( FoundBracket ) {
        FoundBracket = ( (Fa>=0.0)&&(Fb<=0.0) || (Fa<=0.0)&&(Fb>=0.0) ) ? TRUE : FALSE ;
    }

//printf("FoundBracket = %d\n", FoundBracket);
//printf("Fmin = %g\n", Fmin);
    if ( !FoundBracket && ( fabs(Fmin) > 1e-4) ) {

        if ( Info->VerbosityLevel > 1 ) {
            printf("    Lgm_TraceToMirrorPoint: Bracket not found.\n");
            printf("Pmin = %.15lf, %.15lf, %.15lf,     Fmin = %.15lf\n", Pmin.x, Pmin.y, Pmin.z, Fmin);
            printf("Pa = %.15lf, %.15lf, %.15lf,     Fa = %.15lf\n", Pa.x, Pa.y, Pa.z, Fa);
            printf("Pb = %.15lf, %.15lf, %.15lf,     Fb = %.15lf\n", Pb.x, Pb.y, Pb.z, Fb);
        }
        if ( Info->VerbosityLevel > 4 ) {
            printf( "**************** End Detailed Output From TraceToMirrorPoint (VerbosityLevel = %d) ******************\n\n\n", Info->VerbosityLevel );
        }
        return( -3 ); /* We have gone as far as we could without finding a bracket. Bail out. */

    } else if ( !FoundBracket && ( fabs(Fmin) <= 1e-4) ) {

        Fb = Fmin;
        Sb = Smin;
        Pb = Pmin;

    } else {



        /*
         *  We have a bracket. Now go in for the kill using bisection.
         *
         *  (Note: If all we wanted to know is whether or not the line hits the
         *  Earth, we could stop here: it must hit the Earth or we wouldnt
         *  have a minimum bracketed.)
         *
         */
    if (1==1){
        done  = FALSE;
        //reset = TRUE;
        if ( Info->VerbosityLevel > 4 ) nIts = 0;
        while (!done) {

            if ( Info->VerbosityLevel > 4 ) {
                printf("    \tTraceToMirrorPoint, Finding Root: Starting P: %15g %15g %15g  ...\n", P.x, P.y, P.z );
            }

            d = Sb - Sa;

            if ( (fabs(d) < tol)  ) {
                done = TRUE;
            } else {

                //P = Pa; Htry = 0.5*d;
                P = Pa; Htry = LGM_1_OVER_GOLD*d;
                if ( Lgm_MagStep( &P, &u_scale, Htry, &Hdid, &Hnext, sgn, &s, &reset, Info->Bfield, Info ) < 0 ) return(-1);
                Info->Bfield( &P, &Bvec, Info );
                F = Lgm_Magnitude( &Bvec ) - Bm;
                if ( F >= 0.0 ) {
                    Pb = P; Fb = F; Sb = Sa + Hdid;
                } else {
                    Pa = P; Fa = F; Sa += Hdid;
                }

            }

            if ( Info->VerbosityLevel > 4 ) {
                printf("                                             Got To: %15g %15g %15g     with Htry of: %g\n", P.x, P.y, P.z, Htry );
                printf("        \tPa, Ra, Fa, Sa = (%15g, %15g, %15g) %15g %15g %15g\n", Pa.x, Pa.y, Pa.z, Ra, Fa, Sa  );
                printf("        \tPb, Rb, Fb, Sb = (%15g, %15g, %15g) %15g %15g %15g\n", Pb.x, Pb.y, Pb.z, Rb, Fb, Sb  );
                printf("        \tF = %g, |B| Bm = %g %g FoundBracket = %d  done = %d    Current Height = %g\n\n", F, Lgm_Magnitude( &Bvec ), Bm, FoundBracket, done, Height );
            }
            ++nIts;

        }
    }


    //reset = TRUE;



    if (0==1){

        /*
         *  Use Brent's method
         */
        //printf("TRACETOMIRROR: Sa, Sb = %g %g  Fa, Fb = %g %g   tol = %g\n", Sa, Sb, Fa, Fb, tol);
        double      Sz, Fz;
        Lgm_Vector  Pz;
        BrentFuncInfoP    f;

        f.u_scale = u_scale;
        f.Htry    = Htry;
        f.sgn     = sgn;
        f.reset   = reset;
        f.Info    = Info;
        f.func    = &mpFunc;
        f.Val     = Bm;
tol = 1e-10;
        Lgm_zBrentP( Sa, Sb, Fa, Fb, Pa, Pb, &f, tol, &Sz, &Fz, &Pz );
        Fb = Fz; Sb = Sz; Pb = Pz;
        printf("TRACETOMIRROR: Sa, Sb = %g %g  Fa, Fb = %g %g   tol = %g\n", Sa, Sb, Fa, Fb, tol);
    }

    }



    /*
     *  Take Pb as endpoint
     */
    *v = Pb; *Sm = Sb;


    /*
     * Do a final check on the Height to see if we are in loss cone or not.
     */
    Lgm_Convert_Coords( v, &w, GSM_TO_WGS84, Info->c );
    Lgm_WGS84_to_GeodHeight( &w, &Height );

	if ( Height < LC_TOL*Info->Lgm_LossConeHeight ) {
        // dropped below loss cone height -> particle is in loss cone!
        if ( Info->VerbosityLevel > 1 ) {
            printf("    v_gsm = %g %g %g\n", v->x, v->y, v->z);
            printf("    v_wgs84 = %g %g %g   Height = %g\n", w.x, w.y, w.z, Height);
            printf("    Lgm_TraceToMirrorPoint: Current Height is below specified loss cone height of %g km. In Loss Cone. (Height = %g) \n", Info->Lgm_LossConeHeight, Height );
        }
        if ( Info->VerbosityLevel > 4 ) {
            printf( "**************** End Detailed Output From TraceToMirrorPoint (VerbosityLevel = %d) ******************\n\n\n", Info->VerbosityLevel );
        }
	    return(-1);
	}


    if ( Info->VerbosityLevel > 4 ) {
        printf("    =============================================================================================================\n" );
        printf("    |                                                                                                           |\n" );
        printf("    |    Total iterations to find root: %3d                                                                     |\n", nIts );
        printf("    |    Total Bfield evaluations find root: %4ld                                                                 |\n", Info->Lgm_nMagEvals );
        printf("    |    TraceToMirrorPoint, Final Result: %15g %15g %15g     Sm = %g    |\n", v->x, v->y, v->z, *Sm );
        printf("    |                                                                                                           |\n" );
        printf("    =============================================================================================================\n" );
        printf( "**************** End Detailed Output From TraceToMirrorPoint (VerbosityLevel = %d) ******************\n\n\n", Info->VerbosityLevel );
    }

    if ( Info->VerbosityLevel > 2 ) printf("Lgm_TraceToMirrorPoint(): Number of Bfield evaluations = %ld\n", Info->Lgm_nMagEvals );

    return( 1 );

}

