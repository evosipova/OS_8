#include "common.h"
#include <semaphore.h>
#include <signal.h>


const char *writer_sem_name = "/writer-semaphore";
sem_t *writer;
const char *first_writer_sem_name = "/first_writer-semaphore";
sem_t *first_writer;

void sigfunc(int sig) {
  if (sig != SIGINT && sig != SIGTERM) {
    return;
  }
  if (sig == SIGINT) {
    kill(buffer->reader_pid, SIGTERM);
    printf("Writer(SIGINT) ---> Reader(SIGTERM)\n");
  } else if (sig == SIGTERM) {
    printf("Writer(SIGTERM) <--- Reader(SIGINT)\n");
  }
  if (sem_close(writer) == -1) {
    perror("sem_close: Incorrect close of writer semaphore");
    exit(-1);
  };
  if (sem_close(first_writer) == -1) {
    perror("sem_close: Incorrect close of first_writer semaphore");
    exit(-1);
  };
  close_common_semaphores();

  if (sem_unlink(writer_sem_name) == -1) {
    perror("sem_unlink: Incorrect unlink of writer semaphore");
  };
  if (sem_unlink(first_writer_sem_name) == -1) {
    perror("sem_unlink: Incorrect unlink of first_writer semaphore");
  };
  unlink_all();
  printf("Writer: bye!!!\n");
  exit(10);
}

int main() {
  signal(SIGINT, sigfunc);
  signal(SIGTERM, sigfunc);

  srand(time(0));
  init();

  if ((buf_id = shm_open(shar_object, O_CREAT | O_RDWR, 0666)) == -1) {
    perror("shm_open");
    exit(-1);
  } else {
    printf("Object is open: name = %s, id = 0x%x\n", shar_object, buf_id);
  }
  if (ftruncate(buf_id, sizeof(shared_memory)) == -1) {
    perror("ftruncate");
    exit(-1);
  } else {
    printf("Memory size set and = %lu\n", sizeof(shared_memory));
  }
  buffer = mmap(0, sizeof(shared_memory), PROT_WRITE | PROT_READ, MAP_SHARED, buf_id, 0);
  if (buffer == (shared_memory *)-1) {
    perror("writer: mmap");
    exit(-1);
  }
  printf("mmap checkout\n");

  if ((writer = sem_open(writer_sem_name, O_CREAT, 0666, 1)) == 0) {
    perror("sem_open: Can not create writer semaphore");
    exit(-1);
  };
  if ((first_writer = sem_open(first_writer_sem_name, O_CREAT, 0666, 1)) == 0) {
    perror("sem_open: Can not create first_writer semaphore");
    exit(-1);
  };
  if (sem_wait(writer) == -1) {
    perror("sem_wait: Incorrect wait of writer semaphore");
    exit(-1);
  };

  int writer_number = 0;
  sem_getvalue(first_writer, &writer_number);
  printf("checking: writer_number = %d\n", writer_number);
  if (writer_number == 0) {
    printf("Writer %d: I have lost this work :(\n", getpid());
    if (sem_post(writer) == -1) {
      perror("sem_post: Consumer can not increment writer semaphore");
      exit(-1);
    }
    exit(13);
  }
  if (sem_wait(first_writer) == -1) {
    perror("sem_wait: Incorrect wait of first_writer semaphore");
    exit(-1);
  };
  if (sem_post(writer) == -1) {
    perror("sem_post: Consumer can not increment writer semaphore");
    exit(-1);
  }
  buffer->writer_pid = getpid();
  printf("Writer %d: I am first for this work! :)\n", getpid());

  for (int i = 0; i < BUF_SIZE; ++i) {
    buffer->store[i] = -1;
  }
  buffer->have_reader = 0;
  // buffer->have_writer = 0;


  int is_writers = 0;
  sem_getvalue(admin, &is_writers);
  if (is_writers == 0) {
    if (sem_post(admin) == -1) {
      perror("sem_post: Can not increment admin semaphore");
      exit(-1);
    }
  }

  while (1) {
    if (sem_wait(empty) == -1) {
      perror("sem_wait: Incorrect wait of empty semaphore");
      exit(-1);
    };
    if (sem_wait(mutex) == -1) {
      perror("sem_wait: Incorrect wait of mutex");
      exit(-1);
    };
    int cell = getCell();

    printf("Writer %d: I am writing to cell [%d]", getpid(), cell);

    buffer->store[cell] = rand() % 11;
    if (sem_post(full) == -1) {
      perror("sem_post: Incorrect post of full semaphore");
      exit(-1);
    };
    pid_t pid = getpid();
    printf("Producer %d writes value = %d to cell [%d]\n", pid, buffer->store[cell], cell);
    if (sem_post(mutex) == -1) {
      perror("sem_post: Incorrect post of mutex semaphore");
      exit(-1);
    };
    sleep(rand() % 3 + 1);
  }
  return 0;
}