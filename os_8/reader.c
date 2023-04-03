#include "common.h"
#include <unistd.h>


const char *reader_sem_name = "/reader-semaphore";
sem_t *reader;

void sigfunc(int sig) {
  if (sig != SIGINT && sig != SIGTERM) {
    return;
  }
  if (sig == SIGINT) {
    kill(buffer->writer_pid, SIGTERM);
    printf("Reader(SIGINT) ---> Writer(SIGTERM)\n");
  } else if (sig == SIGTERM) {
    printf("Reader(SIGTERM) <--- Writer(SIGINT)\n");
  }
  if (sem_close(reader) == -1) {
    perror("sem_close: Incorrect close of reader semaphore");
    exit(-1);
  };
  if (sem_unlink(reader_sem_name) == -1) {
    perror("sem_unlink: Incorrect unlink of reader semaphore");
    // exit(-1);
  };
  printf("Reader: bye!!!\n");
  exit(10);
}

int factorial(int n) {
  int p = 1;
  for (int i = 1; i <= n; ++i) {
    p *= i;
  }
  return p;
}

int main() {
  signal(SIGINT, sigfunc);
  signal(SIGTERM, sigfunc);

  srand(time(0));
  init();

  if (sem_wait(admin) == -1) {
    perror("sem_wait: Incorrect wait of admin semaphore");
    exit(-1);
  };
  printf("Consumer %d started\n", getpid());
  if (sem_post(admin) == -1) {
    perror("sem_post: Consumer can not increment admin semaphore");
    exit(-1);
  }

  if ((buf_id = shm_open(shar_object, O_RDWR, 0666)) == -1) {
    perror("shm_open: Incorrect reader access");
    exit(-1);
  } else {
    printf("Memory object is opened: name = %s, id = 0x%x\n", shar_object, buf_id);
  }
  if (ftruncate(buf_id, sizeof(shared_memory)) == -1) {
    perror("ftruncate");
    exit(-1);
  } else {
    printf("Memory size set and = %lu\n", sizeof(shared_memory));
  }

  buffer = mmap(0, sizeof(shared_memory), PROT_WRITE | PROT_READ, MAP_SHARED, buf_id, 0);
  if (buffer == (shared_memory *)-1) {
    perror("reader: mmap");
    exit(-1);
  }

  if ((reader = sem_open(reader_sem_name, O_CREAT, 0666, 1)) == 0) {
    perror("sem_open: Can not create reader semaphore");
    exit(-1);
  };
  if (sem_wait(reader) == -1) {  // ожидание когда запустится писатель
    perror("sem_wait: Incorrect wait of reader semaphore");
    exit(-1);
  };
  if (buffer->have_reader) {
    printf("Reader %d: I have lost this work :(\n", getpid());
    if (sem_post(reader) == -1) {
      perror("sem_post: Consumer can not increment reader semaphore");
      exit(-1);
    }
    exit(13);
  }
  buffer->have_reader = 1;
  if (sem_post(reader) == -1) {
    perror("sem_post: Consumer can not increment reader semaphore");
    exit(-1);
  }
  buffer->reader_pid = getpid();

  while (1) {
    sleep(rand() % 3 + 1);
    if (sem_wait(full) == -1) {
      perror("sem_wait: Incorrect wait of full semaphore");
      exit(-1);
    };
    if (sem_wait(mutex) == -1) {
      perror("sem_wait: Incorrect wait of busy semaphore");
      exit(-1);
    };
    int cell = getCell();

    printf("Consumer %d: Reads value = %d from cell [%d]\n", getpid(), buffer->store[cell],
           cell);

    int result = buffer->store[cell];
    buffer->store[cell] = -1;
    int f = factorial(result);
    if (sem_post(empty) == -1) {
      perror("sem_post: Incorrect post of free semaphore");
      exit(-1);
    };
    pid_t pid = getpid();
    printf("Consumer %d: Reads value = %d from cell [%d], factorial = %d\n", pid, result, cell,
           f);
    if (sem_post(mutex) == -1) {
      perror("sem_post: Incorrect post of mutex semaphore");
      exit(-1);
    };
  }
}