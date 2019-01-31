#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <signal.h>

#define SHAREDMEM_FILENAME	"/tmp/shared_mem"

static int is_main_exit = 0;


typedef struct {
    pthread_mutex_t rw_mutex;
    pthread_cond_t rw_cond;
    int rw_mutex_init;
    int read_flag;
    int write_flag;
    int data;
} shared_data_t;

static void sig_handler(int signum)
{
    printf("Signal %d received.\n", signum);
    if (signum == SIGINT)
    {
        is_main_exit = 1;
        printf("Signal SIGINT received, exit program...\n");
    }
}

void read_start(shared_data_t *p_data)
{   
    while (1)
    {
        pthread_mutex_lock(&p_data->rw_mutex);
        if (p_data->write_flag <= 0)
        {
            p_data->read_flag++;
            pthread_mutex_unlock(&p_data->rw_mutex);
            break;
        }
        else
        {
            pthread_cond_wait(&p_data->rw_cond, &p_data->rw_mutex);
            printf("received condition signal.\n");
            printf("writer flag is %d, reader flag is %d\n", p_data->write_flag, p_data->read_flag);
            pthread_mutex_unlock(&p_data->rw_mutex);
            //pthread_mutex_unlock(&p_data->rw_mutex);
            //usleep(1);
        }
    }
}

void read_finish(shared_data_t *p_data)
{
    pthread_mutex_lock(&p_data->rw_mutex);
    p_data->read_flag--;
    if (p_data->read_flag <= 0)
    {
        pthread_cond_broadcast(&p_data->rw_cond);
    }
    pthread_mutex_unlock(&p_data->rw_mutex);
}

void write_start(shared_data_t *p_data)
{
    while (1)
    {
        pthread_mutex_lock(&p_data->rw_mutex);
        if (p_data->read_flag <= 0 && p_data->write_flag <= 0)
        {
            printf("set write flag to 1\n");
            p_data->write_flag = 1;
            pthread_mutex_unlock(&p_data->rw_mutex);
            break;
        }
        else
        {
            pthread_cond_wait(&p_data->rw_cond, &p_data->rw_mutex);
            printf("received condition signal.\n");
            printf("writer flag is %d, reader flag is %d\n", p_data->write_flag, p_data->read_flag);
            pthread_mutex_unlock(&p_data->rw_mutex);
            //pthread_mutex_unlock(&p_data->rw_mutex);
            //usleep(1);
        }
    }
}

void write_finish(shared_data_t *p_data)
{
    pthread_mutex_lock(&p_data->rw_mutex);
    p_data->write_flag = 0;
    pthread_cond_broadcast(&p_data->rw_cond);
    pthread_mutex_unlock(&p_data->rw_mutex);
}

int main(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = sig_handler;
    sa.sa_flags = 0;// not SA_RESTART!;
    sigaction(SIGINT, &sa, NULL);

    int fd = open(SHAREDMEM_FILENAME, O_RDWR);
    printf("shared memory opened.\n");

    shared_data_t* sdata = (shared_data_t*)mmap(0, sizeof(shared_data_t), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    printf("shared memory mapped.\n");
    close(fd);

    if (sdata == NULL)
    {
	printf("failed to map to shared memory.\n");
	return -1;
    }

    printf("reader/writer mutex init flag is %d\n", sdata->rw_mutex_init);

    while (!is_main_exit)
    {
	printf("waiting for shared memory readable...\n");
	read_start(sdata);
	printf("read from shared memory...\n");
        printf("press any key to finish reading...\n");
        getchar();
	printf("data is %d\n", sdata->data);
	sdata->data = 0;
	read_finish(sdata);
	printf("press any key to start read again...\n");
	getchar();
    }

    munmap(sdata, sizeof(shared_data_t));
    return 0;
}
