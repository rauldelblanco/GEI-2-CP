#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi/mpi.h>

int MPI_FlattreeColectiva(void *sendbuf, void *recvbuff, int count, MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm){

    //Realizamos el control de errores

    if(op != MPI_SUM){
        return MPI_ERR_OP;
    }

    if(datatype != MPI_DOUBLE){
        return MPI_ERR_TYPE;
    }

    if(comm != MPI_COMM_WORLD){
        return MPI_ERR_COMM;
    }

    if(count == 0){
        return MPI_ERR_COUNT;
    }

    if(sendbuf == NULL){
        return MPI_ERR_BUFFER;
    }

    int totalprocs, process, error;
    double *salida  = (double *) recvbuff; //Hacemos un casteo para obtener el valor del buffer para recibir
    double *entrada = (double *) sendbuf;  //Hacemos un casteo para obtener el valor del buffer para enviar
    double *aux;

    MPI_Status status;

    MPI_Comm_size(MPI_COMM_WORLD, &totalprocs); //Obtenemos el número de procesos
    MPI_Comm_rank(MPI_COMM_WORLD, &process);   //Obtenemos el número de proceso

    if(process == root){ //Hacemos esto porque el proceso 0 no siempre es el proceso root

        aux = malloc(sizeof (double) * count);

        for (int i = 0; i < count; ++i) { //Cogemos todos los valores del buffer de entrada y los introducimos en el de salida
            salida[i] = entrada[i];
        }

        for (int i = 0; i < totalprocs; ++i) { //Recorremos todos los procesos excepto el proceso root

            if (i != root){
                error = MPI_Recv(aux, count, datatype, i, 0, comm, &status); //El proceso root recibe el mensaje de los otros procesos

                if (error != MPI_SUCCESS){
                    free(aux);
                    return error;
                }

                for (int j = 0; j < count; ++j) { //En caso de que haya más de un elemento en el buffer.
                    salida[j] += aux[j];
                }
            }

        }
        free(aux);
    } else { //Los procesos que no son root envían el mensaje al proceso root
        error = MPI_Send(sendbuf, count, datatype, root, 0, comm);
        if (error != MPI_SUCCESS) {
            return error;
        }
    }
    
    return MPI_SUCCESS;
}

int MPI_BinomialColectiva(void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm){

    if(comm != MPI_COMM_WORLD){
        return MPI_ERR_COMM;
    }

    if(count == 0){
        return MPI_ERR_COUNT;
    }

    if(buffer == NULL){
        return MPI_ERR_BUFFER;
    }

    if(root != 0) {
        return MPI_ERR_ROOT;
    }

    int totalprocs, process, error, destino, origen;

    MPI_Status status;

    MPI_Comm_size(MPI_COMM_WORLD, &totalprocs); //Obtenemos el número de procesos
    MPI_Comm_rank(MPI_COMM_WORLD, &process);   //Obtenemos el número de proceso

    // El número total de iteraciones del bucle es el número entero más alto de realizar el logaritmo de todos los procesos.
    for (int i = 1; i <= ceil(log2(totalprocs)); ++i) {

        // Operación del enunciado
        if(process < pow(2, i-1)){

            //Calculamos el destino del mensaje
            destino = process + pow(2, i-1);

            //Comprobamos que el proceso que va a recibir el mensaje está dentro del rango total de procesos
            if(destino < totalprocs){
                error = MPI_Send(buffer, count, datatype, destino, 0, comm);
                if(error != MPI_SUCCESS){
                    return error;
                }
            }
        } else {

            // Comprobamos que el proceso que llega es un posible receptor.
            if(process < pow(2, i)){

                //Calculamos el proceso origen y recibimos el mensaje de ese proceso
                origen = process - pow(2, i-1);
                error = MPI_Recv(buffer, count, datatype, origen, 0, comm, &status);
                if(error != MPI_SUCCESS){
                    return error;
                }
            }
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
        //MPI_BinomialColectiva(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

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
        MPI_Reduce(&pi, &aprox, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
        //MPI_FlattreeColectiva(&pi, &aprox, 1,MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

        if(process == 0){ //Solo imprime el proceso 0
            printf("pi is approx. %.16f, Error is %.16f\n", aprox, fabs(aprox - PI25DT));
        }
    }

    //Esperamos por todos los procesos y liberamos todos los recursos reservados.
    MPI_Finalize();
}