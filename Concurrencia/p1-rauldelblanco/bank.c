#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "options.h"

#define MAX_AMOUNT 20

struct bank {
    int              num_accounts; // number of accounts
    int*             accounts;     // balance array
    pthread_mutex_t* mutex;        // mutex
};

struct args {
    pthread_mutex_t *mutex;       // mutex for the iterations
    int              thread_num;  // application defined thread #
    int              delay;       // delay between operations
    int	             *iterations; // number of operations
    int              net_total;   // total amount deposited by this thread
    struct bank     *bank;        // pointer to the bank (shared with other threads)
};

struct thread_info {
    pthread_t    id;    // id returned by pthread_create()
    struct args *args;  // pointer to the arguments
};

struct auxiliar {
    pthread_t    id;    // id returned by pthread_create()
    int          delay; // delay between operations
    int          aux;   // end of transfer controller
    struct bank *bank;  // pointer to the bank (shared with other threads)
};

int down(int* iterations, pthread_mutex_t* mutex){
    int aux;
    pthread_mutex_lock(mutex);

    if(*iterations){
        aux = (*iterations)--;
    } else{
        aux = 0;
    }

    pthread_mutex_unlock(mutex);
    return aux;
}

int up(int* iterations, pthread_mutex_t* mutex){
    int aux;
    pthread_mutex_lock(mutex);
    aux = (*iterations)++;
    pthread_mutex_unlock(mutex);
    return aux;
}

// Threads run on this function
void *deposit(void *ptr)
{
    struct args *args =  ptr;
    int amount, account, balance;

    while(down(args->iterations, args->mutex)) {
        amount  = rand() % MAX_AMOUNT;
        account = rand() % args->bank->num_accounts;
        printf("Thread %d depositing %d on account %d\n",
            args->thread_num, amount, account);

        pthread_mutex_lock(&args->bank->mutex[account]);
        balance = args->bank->accounts[account];
        if(args->delay) usleep(args->delay); // Force a context switch

        balance += amount;
        if(args->delay) usleep(args->delay);

        args->bank->accounts[account] = balance;
        if(args->delay) usleep(args->delay);

        args->net_total += amount;
        pthread_mutex_unlock(&args->bank->mutex[account]);
    }
    return NULL;
}

void *transferencias(void *ptr){

    struct args *args = ptr;
    int amount, accountOrigen, accountDestino;

    while (down(args->iterations, args->mutex)){

        accountOrigen  = rand() % args->bank->num_accounts;
        accountDestino = rand() % args->bank->num_accounts;

        if(accountOrigen == accountDestino){
            up(args->iterations, args->mutex);
            continue;
        }

        while (1) {
            pthread_mutex_lock(&args->bank->mutex[accountOrigen]);
            if(pthread_mutex_trylock(&args->bank->mutex[accountDestino])){
                pthread_mutex_unlock(&args->bank->mutex[accountOrigen]);
                continue;
            }

            if (args->bank->accounts[accountOrigen]>0) {
                amount = rand() % args->bank->accounts[accountOrigen];
                if(args->delay) usleep(args->delay);
            } else {
                amount = 0;
            }

            if(amount != 0){
                printf("Thread %d: Account %d transferring %d to account %d\n", args->thread_num, accountOrigen, amount, accountDestino);

                args->bank->accounts[accountDestino] += amount;
                if(args->delay) usleep(args->delay);
                args->bank->accounts[accountOrigen] -= amount;
                if(args->delay) usleep(args->delay);
            }else{
                up(args->iterations, args->mutex);
            }

            pthread_mutex_unlock(&args->bank->mutex[accountOrigen]);
            pthread_mutex_unlock(&args->bank->mutex[accountDestino]);
            break;
        }
        args->net_total += amount;
    }

    return NULL;
}

// start opt.num_threads threads running on deposit.
struct thread_info *start_threads(struct options opt, struct bank *bank, void *(tipo) (void *))
{
    int i, *iterations;
    struct thread_info *threads;
    pthread_mutex_t *mutex;

    printf("\nCreating %d threads\n", opt.num_threads);
    threads = malloc(sizeof(struct thread_info) * opt.num_threads);

    if (threads == NULL) {
        printf("Not enough memory\n");
        exit(1);
    }

    mutex = malloc(sizeof (pthread_mutex_t));
    iterations= malloc(sizeof (int));
    pthread_mutex_init(mutex, NULL);
    *iterations = opt.iterations;

    // Create num_thread threads running swap()
    for (i = 0; i < opt.num_threads; i++) {
        threads[i].args = malloc(sizeof(struct args));

        threads[i].args -> thread_num = i;
        threads[i].args -> net_total  = 0;
        threads[i].args -> bank       = bank;
        threads[i].args -> delay      = opt.delay;
        threads[i].args -> iterations = iterations;
        threads[i].args->mutex          = mutex;

        if (0 != pthread_create(&threads[i].id, NULL, tipo, threads[i].args)) {
            printf("Could not create thread #%d", i);
            exit(1);
        }
    }
    return threads;
}

// Print the final balances of accounts and threads
void print_balances(struct bank *bank, struct thread_info *thrs, int num_threads) {
    int total_deposits=0, bank_total=0;

    for(int i=0; i < num_threads; i++) {
        printf("%d: %d\n", i, thrs[i].args->net_total);
        total_deposits += thrs[i].args->net_total;
    }
    printf("Total: %d\n", total_deposits);

    printf("\nAccount balance\n");
    for(int i=0; i < bank->num_accounts; i++) {
        pthread_mutex_lock(&bank->mutex[i]);
        printf("%d: %d\n", i, bank->accounts[i]);
        bank_total += bank->accounts[i];
        pthread_mutex_unlock(&bank->mutex[i]);
    }
    printf("Total: %d\n", bank_total);
}

void *print_total(void *ptr) {

    struct auxiliar *args = ptr;
    int total;

    while (args->aux){
        total = 0;
        for (int i = 0; i < args->bank->num_accounts; ++i) {
            pthread_mutex_lock(&args->bank->mutex[i]);
        }

        for (int i = 0; i < args->bank->num_accounts; ++i) {
            total += args->bank->accounts[i];
            if(args->delay) usleep(args->delay);
        }

        for (int i = 0; i < args->bank->num_accounts; ++i) {
            pthread_mutex_unlock(&args->bank->mutex[i]);
        }

        printf("Total: %d\n", total);
    }
    return NULL;
}

// wait for all threads to finish, print totals, and free memory
void wait(struct options opt, struct bank *bank, struct thread_info *threads) {
    // Wait for the threads to finish
    for (int i = 0; i < opt.num_threads; i++)
        pthread_join(threads[i].id, NULL);

    printf("\nNet deposits by thread\n");
    print_balances(bank, threads, opt.num_threads);

    // Solamente liberamos la memoria del thread 0 en estos tres casos porque la posicion de memoria
    // reservada para esos campos es la misma en todos los threads.
    free(threads[0].args->iterations);
    pthread_mutex_destroy(threads[0].args->mutex);
    free(threads[0].args->mutex);

    for (int i = 0; i < opt.num_threads; i++)
        free(threads[i].args);

    free(threads);
}

void wait2(struct options opt, struct bank *bank, struct thread_info *threads, struct auxiliar *args) {
    // Wait for the threads to finish
    for (int i = 0; i < opt.num_threads; i++)
        pthread_join(threads[i].id, NULL);

    args->aux = 0; //Hacemos que los threads paren de ejecutar la impresion del total
    usleep(args->delay);

    pthread_join(args->id, NULL); //Esperamos a que termine el thread de impresión
    printf("\nNet transfers by thread\n");
    print_balances(bank, threads, opt.num_threads);

    // Solamente liberamos la memoria del thread 0 en estos tres casos porque la posicion de memoria
    // reservada para esos campos es la misma en todos los threads.
    free(threads[0].args->iterations);
    pthread_mutex_destroy(threads[0].args->mutex);
    free(threads[0].args->mutex);

    for (int i = 0; i < opt.num_threads; i++)
        free(threads[i].args);

    free(threads);

    for (int i = 0; i < bank->num_accounts; ++i) {
        pthread_mutex_destroy(&bank->mutex[i]);
    }
    free(args);
    free(bank->accounts);
    free(bank->mutex);
}

// allocate memory, and set all accounts to 0
void init_accounts(struct bank *bank, int num_accounts) {
    bank->num_accounts = num_accounts;
    bank->accounts     = malloc(bank->num_accounts * sizeof(int));
    bank->mutex        = malloc((bank->num_accounts) * sizeof(pthread_mutex_t));

    for(int i=0; i < bank->num_accounts; i++) {
        bank->accounts[i] = 0;
        pthread_mutex_init(&bank->mutex[i], NULL);
    }
}
struct auxiliar *auxiliar(struct bank *bank, int delay){

    struct auxiliar *args = malloc(sizeof(struct auxiliar));
    args->bank            = bank;
    args->delay           = delay;
    args->aux             = 1;
    pthread_create(&args->id, NULL, print_total, args);

    return args;
}

int main (int argc, char **argv)
{
    struct options      opt;
    struct bank         bank;
    struct thread_info *thrs;
    struct thread_info *transfer_thrs;
    struct auxiliar    *args;

    srand(time(NULL));

    // Default values for the options
    opt.num_threads  = 5;
    opt.num_accounts = 10;
    opt.iterations   = 100;
    opt.delay        = 10;

    read_options(argc, argv, &opt);

    init_accounts(&bank, opt.num_accounts);

    thrs = start_threads(opt, &bank, deposit);
    wait(opt, &bank, thrs);

    transfer_thrs = start_threads(opt, &bank, transferencias);
    args = auxiliar(&bank, opt.delay);
    wait2(opt, &bank, transfer_thrs, args);

    return 0;
}