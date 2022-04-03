#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi/mpi.h>

/*int MPI_FlattreeColectiva(void *buf, int count, MPI_Datatype datatype, int root, MPI_Comm comm){

    int totalprocs, process, error;

    MPI_Status status;

    MPI_Comm_size(MPI_COMM_WORLD, &totalprocs); //Obtenemos el número de procesos
    MPI_Comm_rank(MPI_COMM_WORLD, &process); //Obtenemos el número de proceso

    if(process == root){ //Hacemos esto porque el proceso 0 no siempre es el proceso root

        for (int i = 0; i < totalprocs; ++i) { //Recorremos todos los procesos excepto el proceso root

            if (i != root){
                error = MPI_Send(buf, count, datatype, i, 0, comm); //El proceso root envía el mensaje a los otros procesos
                if (error != MPI_SUCCESS){
                    return error;
                }
            }
        }
    } else { //Los procesos que no son root reciben el mensaje del proceso root
        error = MPI_Recv(buf, count, datatype, root, 0, comm, &status);
        if (error != MPI_SUCCESS) {
            return error;
        }
    }
    return MPI_SUCCESS;
}*/

int MPI_FlattreeColectiva(void *sendbuf, void *recvbuff, int count, MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm){

    int totalprocs, process, error;
    MPI_Status status;
    double aux;

    MPI_Comm_size(MPI_COMM_WORLD, &totalprocs); //Obtenemos el número de procesos
    MPI_Comm_rank(MPI_COMM_WORLD, &process); //Obtenemos el número de proceso

    if(process == root){ //Hacemos esto porque el proceso 0 no siempre es el proceso root

        for (int i = 0; i < totalprocs; ++i) { //Recorremos todos los procesos excepto el proceso root

            if (i != root){
                error = MPI_Recv(&aux, count, datatype, i, 0, comm, &status); //El proceso root recibe el mensaje de los otros procesos
                if (error != MPI_SUCCESS){
                    return error;
                }
            }

            aux += aux;
        }

        //Falta poner el resultado de aux en recvbuff

    } else { //Los procesos que no son root envían el mensaje al proceso root
        error = MPI_Send(sendbuf, count, datatype, root, 0, comm);
        if (error != MPI_SUCCESS) {
            return error;
        }
    }
    return MPI_SUCCESS;
}




int main(int argc, char *argv[])
{
    int i, done = 0, n, count;
    double PI25DT = 3.141592653589793238462643;
    double pi, x, y, z;

    double aprox;
    int totalprocs, process;


    MPI_Init(&argc, &argv); //Inicializamos el MPI
    MPI_Comm_size(MPI_COMM_WORLD, &totalprocs); // Obtenemos el número de procesos
    MPI_Comm_rank(MPI_COMM_WORLD, &process); //Obtenemos el número de proceso


    while (!done)
    {
        if(process == 0){ //El proceso 0 pide los datos
            printf("Enter the number of points: (0 quits) \n");
            scanf("%d",&n);

        }

        //El proceso 0 envía el valor de n a los demás procesos y estos lo reciben.
        MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);


        if (n == 0) break; //El programa termina

        count = 0;

        for (i = process + 1; i <= n; i += totalprocs) {
            // Get the random numbers between 0 and 1
            x = ((double) rand()) / ((double) RAND_MAX);
            y = ((double) rand()) / ((double) RAND_MAX);

            // Calculate the square root of the squares
            z = sqrt((x*x)+(y*y));

            // Check whether z is within the circle
            if(z <= 1.0)
                count++;
        }
        pi = ((double) count/(double) n)*4.0;

        //En este punto todos los procesos tienen su valor local de PI

        //Hacemos la suma de todas las aproximaciones parciales de PI
        //MPI_Reduce(&pi, &aprox, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_FlattreeColectiva(&pi, &aprox, 1,MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

        if(process == 0){ //Solo imprime el proceso 0
            printf("pi is approx. %.16f, Error is %.16f\n", aprox, fabs(aprox - PI25DT));
        }
    }

    //Esperamos por todos los procesos y liberamos todos los recursos reservados.
    MPI_Finalize();
}