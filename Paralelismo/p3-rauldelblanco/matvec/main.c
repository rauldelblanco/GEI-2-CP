#include <stdio.h>
#include <sys/time.h>
#include <mpi/mpi.h>

#define DEBUG 1

#define N 1024

int main(int argc, char *argv[] ) {

    int i, j, totalprocs, process;
    float matrix[N][N];
    float vector[N];
    float result[N];
    struct timeval  tv1, tv2;

    MPI_Init(&argc, &argv); //Inicializamos el MPI
    MPI_Comm_size(MPI_COMM_WORLD, &totalprocs); // Obtenemos el número de procesos
    MPI_Comm_rank(MPI_COMM_WORLD, &process); //Obtenemos el número de proceso


    /* Initialize Matrix and Vector */

    if(process == 0){
        for(i=0;i<N;i++) {
            vector[i] = i;
            for(j=0;j<N;j++) {
                matrix[i][j] = i+j;
            }
        }
    }

    /* Obtenemos el primer tiempo */
    gettimeofday(&tv1, NULL);

    for(i=0;i<N;i++) {
        result[i]=0;
        for(j=0;j<N;j++) {
            result[i] += matrix[i][j]*vector[j];
        }
    }

    gettimeofday(&tv2, NULL);

    /* Obtenemos el segundo tiempo */
    int microseconds = (tv2.tv_usec - tv1.tv_usec) + 1000000 * (tv2.tv_sec - tv1.tv_sec);

    /* Display result */
    if (DEBUG && process == 0){
        for(i=0;i<N;i++) {
            printf(" %f \t ",result[i]);
        }
    } else {
        printf ("Time (seconds) = %lf\n", (double) microseconds/1E6);
    }


    MPI_Finalize();

    return 0;
}