// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"


typedef struct bucket_t{
  // bucket num. Doing some data valid check.
  uint num;

  #define QUEUE_MAX_SZ (8)

  struct spinlock lock;

  // creating queue. -1 means slot is free
  uint queue_sz;
  uint devs[QUEUE_MAX_SZ];
  uint blocknos[QUEUE_MAX_SZ];

  // TODO: how to organize buf
  // Shall we put the most used buf at the head or tail?
  // 1. find the target buffer quickly.
  // 2. find the free buffer quickly.
  // so we put buffer at head when calling bget. Dose it make sense?
  // **dont change chain when calling brelse** or put it at tail?
  struct buf head;
} bucket_t;


// return value:
//   0: queue full, 1: queue not full
static int bucket_is_queue_full(struct bucket_t *bucket) {
  return bucket->queue_sz == QUEUE_MAX_SZ;
}

static int bucket_is_queue_empty(struct bucket_t *bucket) {
  return bucket->queue_sz == 0;
}

static void bucket_queue_add(struct bucket_t *bucket, uint dev, uint blockno) {
  if (bucket_is_queue_full(bucket)) {
    panic("bucket_queue_add: add element to a full queue");
  }
  bucket->devs[bucket->queue_sz] = dev;
  bucket->blocknos[bucket->queue_sz] = blockno;
  ++bucket->queue_sz;
}

static void bucket_queue_remove(struct bucket_t *bucket, uint idx) {
  if (bucket_is_queue_empty(bucket) || idx >= bucket->queue_sz) {
    panic("bucket_queue_remove: remove a invalid slot");
  }
  bucket->devs[idx] = bucket->devs[bucket->queue_sz - 1];
  bucket->blocknos[idx] = bucket->blocknos[bucket->queue_sz - 1];
  --bucket->queue_sz;
}

static int bucket_queue_find(struct bucket_t *bucket, uint dev, uint blockno) {
  for (uint i = 0; i < bucket->queue_sz; ++i) {
    if (bucket->devs[i] == dev && bucket->blocknos[i] == blockno) {
      return i;
    }
  } 
  return -1;
}

// add a new buffer to a bucket
static void bucket_add_buf(struct bucket_t *bucket, struct buf *buf, uint is_wait);

static struct buf* bucket_chain_get_buf(struct bucket_t *bucket, uint dev, uint blockno);

// find buffer from bucket
static struct buf* bucket_get_buf(struct bucket_t *bucket, uint dev, uint blockno);

// remove the buffer from bucket.
void bucket_release_buf(struct bucket_t *bucket, struct buf *buf);

// get and init a free buffer in bucket
static struct buf* bucket_get_free_buf(struct bucket_t *bucket);
  
#define BUCKET_TOT_NUM (13)

typedef struct bcache_t{
  struct buf buf[NBUF];
  struct bucket_t buckets[BUCKET_TOT_NUM];
}bcache_t;

static uint bucket_idx(uint dev, uint blockno) {
  return (dev * 10 + blockno) % BUCKET_TOT_NUM;
}

struct bcache_t my_bcache;
//------------------
// test
struct spinlock g_lock;
//---------------------


static struct buf* bucket_chain_get_buf(struct bucket_t *bucket, uint dev, uint blockno) {
  struct buf *b;
  for(b = bucket->head.next; b != &bucket->head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;

      // move buffer to head
      b->prev->next = b->next;
      b->next->prev = b->prev;

      b->next = bucket->head.next;
      b->prev = &bucket->head;
      bucket->head.next->prev = b;
      bucket->head.next = b;

      return b;
    }
  }
  return 0;
}

// add a new buffer to a bucket
static void bucket_add_buf(struct bucket_t *bucket, struct buf *buf, uint is_wait) {
  if (bucket->num != bucket_idx(buf->dev, buf->blockno)) {
    panic("buffer insert wrong bucket");
  }

  acquire(&bucket->lock);

  // add to head
  buf->prev = &bucket->head;
  buf->next = bucket->head.next;

  bucket->head.next->prev = buf;
  bucket->head.next = buf;

  if (is_wait) {
    int idx = bucket_queue_find(bucket, buf->dev, buf->blockno);
    if (idx == -1) {
      panic("bucket_add_buf: there should be a request");
    }

    // wake up thread which is 1: waiting for creating 2: waiting for slot
    wakeup(bucket->devs);
    if (bucket_is_queue_full(bucket)) {
      wakeup(&bucket->devs[QUEUE_MAX_SZ]);
    }
    bucket_queue_remove(bucket, idx);
  }

  release(&bucket->lock);
}

// find buffer from bucket
static struct buf* bucket_get_buf(struct bucket_t *bucket, uint dev, uint blockno) {
  struct buf *b;
  if (bucket_idx(dev, blockno) != bucket->num) {
    panic("bucket_get_buf: file is not in this bucket");
  }

  acquire(&bucket->lock);

  // 1. finding buffer in the chain
  b = bucket_chain_get_buf(bucket, dev, blockno);
  if (b) {
    release(&bucket->lock);
    return b;
  }

  // 2. buffer is not in the chain, find if it is being creating
  int idx = bucket_queue_find(bucket, dev, blockno);
  if (idx != -1) {
    // 2.1 buffer is being creating, just wait for it.
    while (bucket_queue_find(bucket, dev, blockno) != -1) {
      // TODO: sleep at devs, because the slot may changed
      sleep(&bucket->devs, &bucket->lock);
    }

    b = bucket_chain_get_buf(bucket, dev, blockno);
    if (b == 0) {
      panic("buffer should have be created");
    }

    release(&bucket->lock);
    return b;
  } 

  // 2.2 bufer is not being creating, ready to create a new one
  // but waiting for usable queue slot firstly
  while (bucket_is_queue_full(bucket)) {
    sleep(&bucket->devs[QUEUE_MAX_SZ], &bucket->lock);
  }
  bucket_queue_add(bucket, dev, blockno);

  release(&bucket->lock);
  return 0;
}

// remove the buffer from bucket.
void bucket_release_buf(struct bucket_t *bucket, struct buf *buf) {
  acquire(&bucket->lock);
  if (bucket->num != bucket_idx(buf->dev, buf->blockno)) {
    panic("bucket_release_buf: buf in wrong bucket");
  }

  if (buf->refcnt == 0) {
    panic("why release a buffer whose refcnt is zero");
  }


  --buf->refcnt;

  // printf("release buf:%d-%d:%d, cnt=%d\n", buf->dev, buf->blockno, buf - my_bcache.buf, buf->refcnt);

  release(&bucket->lock);
}

// get and init a free buffer in bucket
static struct buf* bucket_get_free_buf(struct bucket_t *bucket) {
  struct buf *b;

  acquire(&bucket->lock);

  // the tail buffer is the least recent used buffer.
  for(b = bucket->head.prev; b != &bucket->head; b = b->prev){
    if (b->refcnt == 0) {
      // remove buffer from bucket
      b->prev->next = b->next;
      b->next->prev = b->prev;

      b->prev = b;
      b->next = b;

      release(&bucket->lock);
      return b;
    }
  }

  release(&bucket->lock);

  return 0;
}

// struct {
//   struct spinlock lock;
//   struct buf buf[NBUF];

//   // Linked list of all buffers, through prev/next.
//   // Sorted by how recently the buffer was used.
//   // head.next is most recent, head.prev is least.
//   struct buf head;
// } bcache;

void
binit(void)
{
  char name[32];
  // init each bucket
  for (int i = 0; i < BUCKET_TOT_NUM; ++i) {
    snprintf(name, sizeof(name), "bucket-%d", i);
    my_bcache.buckets[i].num = i;
    initlock(&my_bcache.buckets[i].lock, name);

    my_bcache.buckets[i].head.prev = &my_bcache.buckets[i].head;
    my_bcache.buckets[i].head.next = &my_bcache.buckets[i].head;

    my_bcache.buckets[i].queue_sz = 0;
  }

  // init each buffer
  for (int i = 0; i < NBUF; ++i) {
    struct buf *buf = &my_bcache.buf[i];
    buf->prev = buf;
    buf->next = buf;

    snprintf(name, sizeof(name), "buffer-%d", i);
    initsleeplock(&buf->lock, name);

    // the initial location of buffer does matter.
    bucket_add_buf(&my_bcache.buckets[bucket_idx(buf->dev, buf->blockno)], buf, 0);
  }


  initlock(&g_lock, "g_lock");

  // ---------------------------------------------------------
  // struct buf *b;

  // initlock(&bcache.lock, "bcache");

  // // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  // for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   initsleeplock(&b->lock, "buffer");
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  // FIXME:some bad happened here when call bget concurrently
  // i dont know why free a block twice

  // FIXME:
  // one situation:
  // two process both want a block.Then the algorthm will have two buffer using same block.

  // find target buffer at specific bucket
  struct buf *b;
  uint id = bucket_idx(dev, blockno);
  b = bucket_get_buf(&my_bcache.buckets[id], dev, blockno);
  if (b) {
    acquiresleep(&b->lock);
    // printf("get buf:%d-%d:%d\n", dev, blockno, b - my_bcache.buf);

    return b;
  }

  // try to get free buf from another bucket
  for (int i = 0; i < BUCKET_TOT_NUM; ++i) {
    uint t_id = (id + i) % BUCKET_TOT_NUM;
    b = bucket_get_free_buf(&my_bcache.buckets[t_id]);
    if (b) {
      // no others can see a free buffer, so can modify it without locking.
      b->dev = dev;
      b->blockno = blockno;

      b->refcnt = 1;
      b->valid = 0;

      bucket_add_buf(&my_bcache.buckets[id], b, 1);

      acquiresleep(&b->lock);
      // printf("get free buf:%d-%d:%d\n", dev, blockno, b - my_bcache.buf);
      return b;
    }
  }

  panic("bget: no buffers");
//--------------------------------
  // struct buf *b;

  // acquire(&bcache.lock);

  // // Is the block already cached?
  // for(b = bcache.head.next; b != &bcache.head; b = b->next){
  //   if(b->dev == dev && b->blockno == blockno){
  //     b->refcnt++;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }

  // // Not cached.
  // // Recycle the least recently used (LRU) unused buffer.
  // for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //   if(b->refcnt == 0) {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
  // panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  //
  // decrease reference cnt of buf

  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint id = bucket_idx(b->dev, b->blockno);

  bucket_release_buf(&my_bcache.buckets[id], b);

  //-------------------------------------
  // if(!holdingsleep(&b->lock))
  //   panic("brelse");

  // releasesleep(&b->lock);

  // acquire(&bcache.lock);
  // b->refcnt--;
  // if (b->refcnt == 0) {
  //   // no one is waiting for it.
  //   b->next->prev = b->prev;
  //   b->prev->next = b->next;
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
  
  // release(&bcache.lock);
}

void
bpin(struct buf *b) {
  uint id = bucket_idx(b->dev, b->blockno);

  // FIXME: buffer may have been changed

  acquire(&my_bcache.buckets[id].lock);
  if (b->refcnt == 0) {
    panic("buf may have been used by another process");
  }

  ++b->refcnt;

  // printf("pin buf:%d-%d:%d, cnt=%d\n", b->dev, b->blockno, b - my_bcache.buf, b->refcnt);

  release(&my_bcache.buckets[id].lock);

  // acquire(&bcache.lock);
  // b->refcnt++;
  // release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  uint id = bucket_idx(b->dev, b->blockno);

  acquire(&my_bcache.buckets[id].lock);

  // printf("unpin buf:%d-%d:%d, cnt=%d\n", b->dev, b->blockno, b - my_bcache.buf, b->refcnt);

  b->refcnt--;
  release(&my_bcache.buckets[id].lock);

  // acquire(&bcache.lock);
  // b->refcnt--;
  // release(&bcache.lock);
}


