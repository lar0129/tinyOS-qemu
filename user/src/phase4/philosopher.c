#include "philosopher.h"

// TODO: define some sem if you need

int phi_usem_id[PHI_NUM];
int max_eat_usem_id;

void init() {
  // init some sem if you need
  // // TODO();
  for(int i = 0; i < PHI_NUM; i++){
    phi_usem_id[i] = sem_open(1);
  }
  max_eat_usem_id = sem_open(PHI_NUM - 1);
}

void philosopher(int id) {
  // implement philosopher, remember to call `eat` and `think`
  while (1) {
  // // TODO();
    think(id);
    P(max_eat_usem_id);
    P(phi_usem_id[id]);
    P(phi_usem_id[(id + 1) % PHI_NUM]);
    eat(id);
    V(phi_usem_id[(id + 1) % PHI_NUM]);
    V(phi_usem_id[id]);
    V(max_eat_usem_id);
  }
}
