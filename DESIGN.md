(1)
allocated block: [header, 8] [payload, 16 aligned] [footer, 8]
free block: [header, 8] [prev, 8] [next, 8] [footer, 8]
#define MIN_BLOCK 32

(2)
powers of 2 classes, starting at 0-32, 33-64, 65-128...

#define NUM_CLASSES 9

int get_class_index(size_t size) {
  int idx = 0;
  size_t upper = MIN_BLOCK;
  while (size > upper && idx < NUM_CLASSES - 1) {
      upper <<= 1;
      idx++;
  }
  return idx;
}

(3)

insert_node(bp) {
  idx = get_class_index(GET_SIZE(HDRP(bp)));
  head = class_head[idx];

  class_head[idx] = bp;

  bp->next = head;
  bp->prev = NULL;

  if (head != NULL) 
    head->prev = bp;
    

  
}

remove_node(bp) {
  idx = get_class_size(GET_SIZE(HDRP(bp)));
  if (bp->prev == NULL) { // is head
    bp-> <-ptr
    class_head[idx] = bp->next;
    class_head[idx]->prev = NULL;
  } else {

    prev = bp->prev;
    next = bp->next;

    prev->next = next;
    next->prev = prev;
  }
}


find_fit(size_t asize) {
  for (idx = get_class_index(asize); idx < NUM_CLASSES; idx++) {
    for (void *bp = class_head[idx]; bp != NULL; bp = bp->next) {
      if (GET_SIZE(HDRP(bp)) >= asize)
        return bp;
    }
  }
  return NULL;
}

coalesce(bp) {



}
