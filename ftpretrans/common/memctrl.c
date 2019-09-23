#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

typedef struct _BUCKET_NODE_ {
  char* val_;
  char* next_;
} BUCKET_NODE;

typedef struct _INDEX_NODE_ {
  char* val_;
  char* next_;
  uint32_t itemused;
} INDEX_NODE;

static INDEX_NODE indexnode;
static BUCKET_NODE bucketnode;
#define DEFAULT_ITEM_NUM 1024
#define BYTE_PER_NUM (DEFAULT_ITEM_NUM / 8)
#define INT_PER_NUM (DEFAULT_ITEM_NUM / 32)
#define INT_PER_BYTE 4
static int itemsize = 0;
static pthread_mutex_t mutex;

static void BucketNodeInit(BUCKET_NODE* node) {
  node->val_ = 0;
  node->next_ = 0;
}

static void IndexNodeInit(INDEX_NODE* node) {
  node->val_ = 0;
  node->next_ = 0;
  node->itemused = 0;
}

static int GetSpareLoc(INDEX_NODE* node) {
  uint32_t* val;
  int index = 0;
  for (int num = 0; num < INT_PER_NUM; ++num) {
    val = (uint32_t*)(node->val_ + num * INT_PER_BYTE);
    index = __builtin_ffsl(*val);
    if (0 != index) {
      *val &= ~(1 << (index - 1));
      ++(node->itemused);
      return num * 32 + index;
    }
  }
  return 0;
}

static int ResetSpareLoc(INDEX_NODE* node, int index) {
  uint32_t* val = (uint32_t*)node->val_ + index / 32;
  int offset = index % 32;
  *val |= 1 << offset;
}

static int CreateNode(INDEX_NODE** inode, BUCKET_NODE** bnode) {
  int success = 0;
  while (1) {
    *inode = (INDEX_NODE*)malloc(sizeof(INDEX_NODE));
    if (0 == *inode) {
      printf("create index node failed.\n");
      break;
    }
    IndexNodeInit(*inode);
    (*inode)->val_ = (char*)malloc(BYTE_PER_NUM);
    if (0 == (*inode)->val_) {
      printf("create index node failed.\n");
      break;
    }
    *bnode = (BUCKET_NODE*)malloc(sizeof(BUCKET_NODE));
    if (0 == *bnode) {
      printf("create bucket node failed.\n");
      break;
    }
    BucketNodeInit(*bnode);
    (*bnode)->val_ = (char*)malloc(DEFAULT_ITEM_NUM * itemsize);
    if (0 == (*bnode)->val_) {
      printf("create bucket mem failed.\n");
      break;
    }
    success = 1;
    break;
  }
  if (1 == success) {
    memset((*inode)->val_, 0xFF, BYTE_PER_NUM);
    return 0;
  }

  if (*inode) {
    if ((*inode)->val_) free((*inode)->val_);
    free(*inode);
  }
  if (*bnode) {
    if ((*bnode)->val_) free((*bnode)->val_);
    free(*bnode);
  }
  return -1;
}

// every mem bucket create with 1024 items
int MemInit(int size) {
  itemsize = size;
  IndexNodeInit(&indexnode);
  BucketNodeInit(&bucketnode);
  indexnode.val_ = (char*)malloc(BYTE_PER_NUM);
  if (0 == indexnode.val_) {
    printf("create index mem failed.\n");
    return -1;
  }
  bucketnode.val_ = (char*)malloc(DEFAULT_ITEM_NUM * itemsize);
  if (0 == bucketnode.val_) {
    printf("create bucket mem failed.\n");
    free(indexnode.val_);
    indexnode.val_ = 0;
    return -1;
  }
  // spare index set to 1
  memset(indexnode.val_, 0xFF, BYTE_PER_NUM);
  pthread_mutex_init(&mutex, 0);
  return 0;
}

void MemDestroy() {
  pthread_mutex_destroy(&mutex);
  INDEX_NODE* p = indexnode.next_;
  INDEX_NODE* tmp = p;
  while (0 != p) {
    tmp = p;
    free(p->val_);
    p = p->next_;
    free(tmp);
  }
  if (indexnode.val_) free(indexnode.val_);
  BUCKET_NODE* bp = bucketnode.next_;
  BUCKET_NODE* btmp = p;
  while (0 != bp) {
    btmp = bp;
    free(bp->val_);
    bp = bp->next_;
    free(btmp);
  }
  if (bucketnode.val_) free(bucketnode.val_);
}

char* GetItemMem() {
  pthread_mutex_lock(&mutex);
  INDEX_NODE* p = &indexnode;
  INDEX_NODE* tmp = p;
  BUCKET_NODE* bp = &bucketnode;
  BUCKET_NODE* btmp = bp;
  while (0 != p) {
    if (p->itemused < DEFAULT_ITEM_NUM) break;
    tmp = p;
    btmp = bp;
    bp = bp->next_;
    p = p->next_;
  }
  if (0 != p) {
    int index = GetSpareLoc(p);
    if (0 == index) {
      printf("error happend, index not equal to cnt.\n");
      pthread_mutex_unlock(&mutex);
      return 0;
    }
    pthread_mutex_unlock(&mutex);
    return bp->val_ + itemsize * (index - 1);
  }
  INDEX_NODE* newinode = 0;
  BUCKET_NODE* newbnode = 0;
  int ret = CreateNode(&newinode, &newbnode);
  if (0 != ret) {
    pthread_mutex_unlock(&mutex);
    return 0;
  }
  tmp->next_ = newinode;
  btmp->next_ = newbnode;
  int index = GetSpareLoc(newinode);
  pthread_mutex_unlock(&mutex);
  return bp->val_ + itemsize * (index - 1);
}

void ResetItem(char* item) {
  INDEX_NODE* p = &indexnode;
  BUCKET_NODE* bp = &bucketnode;
  while (0 != bp) {
    if (item >= bp->val_ && item < bp->val_ + DEFAULT_ITEM_NUM * itemsize) {
      if ((item - bp->val_) % itemsize != 0) {
        printf("Reset addr error.\n");
        return;
      }
      pthread_mutex_lock(&mutex);
      ResetSpareLoc(p, (item - bp->val_) / itemsize);
      pthread_mutex_unlock(&mutex);
      break;
    }
    bp = bp->next_;
    p = p->next_;
  }
  return;
}
