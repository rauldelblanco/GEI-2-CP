#include <sys/types.h>
#include <openssl/md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define PASS_LEN 6
#define N_THREADS 10
#define PROGRESS_BAR "##################################################"

struct info {
    char           **hash;
    int              casos;
    int              found;
    pthread_mutex_t *mutex_found;
    pthread_mutex_t *mutex_casos;
};

struct args {
    int          thread_num;
    struct info *info;
};

struct thread_info {
    pthread_t       id;
    struct args    *args;
};

long ipow(long base, int exp)
{
    long res = 1;
    for (;;)
    {
        if (exp & 1)
            res *= base;
        exp >>= 1;
        if (!exp)
            break;
        base *= base;
    }

    return res;
}

long pass_to_long(char *str) {
    long res = 0;

    for(int i=0; i < PASS_LEN; i++)
        res = res * 26 + str[i]-'a';

    return res;
}

void long_to_pass(long n, unsigned char *str) {  // str should have size PASS_SIZE+1
    for(int i=PASS_LEN-1; i >= 0; i--) {
        str[i] = n % 26 + 'a';
        n /= 26;
    }
    str[PASS_LEN] = '\0';
}

int hex_value(char c) {
    if (c>='0' && c <='9')
        return c - '0';
    else if (c>= 'A' && c <='F')
        return c-'A'+10;
    else if (c>= 'a' && c <='f')
        return c-'a'+10;
    else return 0;
}

void hex_to_num(char *str, unsigned char *hex) {
    for(int i=0; i < MD5_DIGEST_LENGTH; i++)
        hex[i] = (hex_value(str[i*2]) << 4) + hex_value(str[i*2 + 1]);
}

void *break_pass(void *ptr) {
    struct args *args = ptr;
    int found;
    char *aux;
    unsigned char md5_num[MD5_DIGEST_LENGTH];
    unsigned char res[MD5_DIGEST_LENGTH];
    unsigned char *pass = malloc((PASS_LEN + 1) * sizeof(char));
    long bound = ipow(26, PASS_LEN); // we have passwords of PASS_LEN
                                     // lowercase chars =>
                                    //     26 ^ PASS_LEN  different cases

    for(long i=args->thread_num; i < bound; i += N_THREADS) {

        pthread_mutex_lock(args->info->mutex_found);
        found = args->info->found;
        pthread_mutex_unlock(args->info->mutex_found);

        if(found != 0){

            pthread_mutex_lock(args->info->mutex_casos);
            args->info->casos++;
            pthread_mutex_unlock(args->info->mutex_casos);

            long_to_pass(i, pass);

            MD5(pass, PASS_LEN, res);

            for (int j = 0; j < found; ++j) {
                hex_to_num(args->info->hash[j], md5_num);

                if(0 == memcmp(res, md5_num, MD5_DIGEST_LENGTH)){

                    printf("\r%s: %-60s\n", args->info->hash[j], pass);

                    for (int k = j; k < (found - 1); ++k) {
                        aux = args->info->hash[k];
                        args->info->hash[k] = args->info->hash[k + 1];
                        args->info->hash[k + 1] = aux;
                    }
                    pthread_mutex_lock(args->info->mutex_found);
                    args->info->found--;
                    pthread_mutex_unlock(args->info->mutex_found);
                    break; // Found it!
                }
            }

        }else {
            break;
        }
    }

    free(pass);
    return NULL;
}

void *progress(void *ptr) {
    struct args *args = ptr;
    double  attempts, porcentaje;
    long bound = ipow(26, PASS_LEN);
    int found = -1, print, t1, t2, t = 0;

    while (found != 0) {

        pthread_mutex_lock(args->info->mutex_casos);
        attempts = (double) args->info->casos;
        pthread_mutex_unlock(args->info->mutex_casos);

        porcentaje = 100 * (attempts / bound);
        print = (int) (porcentaje * 0.5);

        printf("\rSearching: [%.*s%*s] %.2f%%    Pass/sec: %d", print, PROGRESS_BAR, 50 - print, "", porcentaje, t);
        fflush(stdout);

        pthread_mutex_lock(args->info->mutex_casos);
        t1 = args->info->casos;
        pthread_mutex_unlock(args->info->mutex_casos);

        sleep(1);

        pthread_mutex_lock(args->info->mutex_casos);
        t2 = args->info->casos;
        pthread_mutex_unlock(args->info->mutex_casos);

        t = t2 - t1;

        pthread_mutex_lock(args->info->mutex_found);
        found = args->info->found;
        pthread_mutex_unlock(args->info->mutex_found);
    }

    puts("");

    return NULL;
}

struct thread_info *start_threads(struct info *info){

    struct thread_info *threads;

    threads = malloc(sizeof (struct thread_info)*(N_THREADS + 1));

    if (threads == NULL) {
        printf("Not enough memory\n");
        exit(1);
    }

    for (int i = 0; i < (N_THREADS + 1); ++i) {
        threads[i].args = malloc(sizeof (struct args));
        threads[i].args->thread_num = i;
        threads[i].args->info = info;

        if(i < N_THREADS){
            if (0 != pthread_create(&threads[i].id, NULL, break_pass, threads[i].args)) {
                printf("Could not create thread");
                exit(1);
            }
        } else {
            if (0 != pthread_create(&threads[i].id, NULL, progress, threads[i].args)) {
                printf("Could not create thread");
                exit(1);
            }
        }

    }

    return threads;
}

void init_data(struct info *info, int hashes, char *md5[]){

    info->mutex_casos = malloc(sizeof (pthread_mutex_t));
    info->mutex_found = malloc(sizeof (pthread_mutex_t));
    info->hash = malloc(sizeof (char *) * hashes);

    if (info->hash == NULL || info->mutex_casos == NULL || info->mutex_found == NULL){
        printf("Not enough memory\n");
        exit(1);
    }

    pthread_mutex_init(info->mutex_casos, NULL);
    pthread_mutex_init(info->mutex_found, NULL);

    for (int i = 0; i < hashes; ++i) {
        info->hash[i] = md5[i+1];
    }

    info->found = hashes;
    info->casos = 0;
}

void wait(struct thread_info *threads, struct info *info){

    for (int i = 0; i < N_THREADS + 1; ++i) {
        pthread_join(threads[i].id,NULL);
    }

    for (int i = 0; i < N_THREADS + 1; ++i) {
        free(threads[i].args);
    }

    pthread_mutex_destroy(info->mutex_casos);
    pthread_mutex_destroy(info->mutex_found);
    free(info->mutex_casos);
    free(info->mutex_found);

    free(info->hash);

    free(threads);
}

int main(int argc, char *argv[]) {

    struct info info;
    struct thread_info *thrs;

    if(argc < 2) {
        printf("Use: %s string\n", argv[0]);
        exit(0);
    }

    init_data(&info, --argc, argv);

    thrs = start_threads(&info);

    wait(thrs, &info);
    return 0;
}
