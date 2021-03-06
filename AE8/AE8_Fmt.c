/*
 * We are reading in the AE8 models and dumping them as static vars in .h files
 * for inclusion into LanlGeoMag
 */
#include <stdio.h>
#include <string.h>

static char *ModelDataFiles[] = { "ae8min.asc", "ae8max.asc", "ap8min.asc", "ap8max.asc" };
static char *ModelNames[]     = { "AE8MIN", "AE8MAX", "AP8MIN", "AP8MAX" };


main(){

    int         i, j=0, k, n=0;
    char        s1[8], s2[8], s3[8], s4[8], s5[8], s6[8], s7[8], s8[8], s9[8], s10[8], s11[8], s12[8];
    long int    v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v[30000];
    char        Filename[256];
    char        Line[256];
    FILE        *fp, *fp_out;


    sprintf( Filename, "Lgm_AE8_AP8.h", "w");
    fp_out = fopen( Filename , "w");


    fprintf( fp_out, "#ifndef LGM_AE8_H\n");
    fprintf( fp_out, "#define LGM_AE8_H\n\n");
    fprintf( fp_out, "/*\n");
    fprintf( fp_out, " *  Lgm_AE8.h\n");
    fprintf( fp_out, " *\n");
    fprintf( fp_out, " *  These are static arrays of the data \"MAPS\" that are used in the AE8/AP8\n");
    fprintf( fp_out, " *  radiation belt flux models. This file was computer generated by AE8_Fmt.c\n");
    fprintf( fp_out, " *  which reads in the raw MAPS used in the fortran code and dumps them in a\n");
    fprintf( fp_out, " *  more readable format. Note: each array has a leading zero added. This is to\n");
    fprintf( fp_out, " *  make the arrays compatible with the fortran-stlye arrays starting at index\n");
    fprintf( fp_out, " *  1 instead, of zero.\n");
    fprintf( fp_out, " *\n");
    fprintf( fp_out, " */\n\n");

    fprintf( fp_out, "#define LGM_AP8MAX 1\n");
    fprintf( fp_out, "#define LGM_AP8MIN 2\n");
    fprintf( fp_out, "#define LGM_AE8MAX 7\n");
    fprintf( fp_out, "#define LGM_AE8MIN 8\n");
    fprintf( fp_out, "#define LGM_INTEGRAL_FLUX     1\n");
    fprintf( fp_out, "#define LGM_DIFFERENTIAL_FLUX 2\n\n");

    for ( j=0; j<4; ++j ) {

        printf("Processing: %s\n", ModelNames[j]);

        fp = fopen( ModelDataFiles[j], "r");
        fgets( Line, 250, fp );
        Line[strlen(Line)-1] = '\0';
        // since the numbers can run together and blanks are not automatically interpreted to be leading zeros,
        // we need to change all of the blanks to 0's
        for (i=0;i<strlen(Line);i++) if (Line[i] == ' ') Line[i] = '0';
        sscanf( Line+1, "%6ld%6ld%6ld%6ld%6ld%6ld%6ld%6ld", &v1, &v2, &v3, &v4, &v5, &v6, &v7, &v8);
        fprintf( fp_out, "static int %s_IHEAD[] = { 0, %6ld, %6ld, %6ld, %6ld, %6ld, %6ld, %6ld, %6ld };\n", ModelNames[j], v1, v2, v3, v4, v5, v6, v7, v8);
        fprintf( fp_out, "static int %s_MAP[] = { 0,\n", ModelNames[j] );


        n = 0;
        while( fgets( Line, 250, fp ) != NULL ){

            Line[strlen(Line)-1] = '\0';
            for (i=0;i<strlen(Line);i++) if (Line[i] == ' ') Line[i] = '0';
            v1 = v2 = v3 = v4 = v5 = v6 = v7 = v8 = v9 = v10 = v11 = v12 = 0.0;
            sscanf( Line+1, "%6ld%6ld%6ld%6ld%6ld%6ld%6ld%6ld%6ld%6ld%6ld%6ld", &v1, &v2, &v3, &v4, &v5, &v6, &v7, &v8, &v9, &v10, &v11, &v12);
            v[n++] = v1; v[n++] = v2; v[n++] = v3; v[n++] = v4; v[n++] = v5; v[n++] = v6; v[n++] = v7; v[n++] = v8; v[n++] = v9; v[n++] = v10; v[n++] = v11; v[n++] = v12;

        }
        fclose(fp);

        k = 0;
        for (i=0; i<n; ++i){
            if ( k>= 12 ) {
                fprintf(fp_out, "\n");
                k = 0;
            }
            if ( i == (n-1) ) fprintf(fp_out, " %6ld", v[i] );
            else              fprintf(fp_out, " %6ld,", v[i] );
            ++k;
        }



        fprintf( fp_out, "};\n" );


    }
    fprintf( fp_out, "\n\nvoid TRARA1( int DESCR[], int MAP[], double FL, double BB0, double E[], double F[], int N );\n");
    fprintf( fp_out, "double  TRARA2( int MAP[], int IL, int IB, double FISTEP );\n");
    fprintf( fp_out, "double  AE8_AP8_Flux( double L, double BB0, int MODEL, int FLUXTYPE, double E1, double E2 );\n\n");
    fprintf( fp_out, "\n\n#endif\n");
    fclose(fp_out);


}


    
