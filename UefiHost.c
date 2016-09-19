#include <lkl_host.h>
#include <iomem.h>
#include <unistd.h>

#include <lk/kernel/semaphore.h>
#include <lk/kernel/mutex.h>
#include <lk/kernel/thread.h>

#include "LKL.h"

static void print(const char *str, int len)
{
	int ret __attribute__((unused));

	ret = write(STDOUT_FILENO, str, len);
}

struct lkl_mutex {
	mutex_t mutex;
};

struct lkl_sem {
	semaphore_t sem;
};

static struct lkl_sem *sem_alloc(int count)
{
	struct lkl_sem *sem;

	sem = AllocatePool(sizeof(*sem));
	if (!sem)
		return NULL;

	sem_init(&sem->sem, count);

	return sem;
}

static void sem_free(struct lkl_sem *sem)
{
	sem_destroy(&sem->sem);
	FreePool(sem);
}

static void sem_up(struct lkl_sem *sem)
{
    EFI_TPL OldTpl;
    OldTpl = gBS->RaiseTPL (TPL_HIGH_LEVEL);
    gBS->RestoreTPL (OldTpl);

	sem_post(&sem->sem, 1);
}

static void sem_down(struct lkl_sem *sem)
{
    EFI_TPL OldTpl;
    OldTpl = gBS->RaiseTPL (TPL_HIGH_LEVEL);
    gBS->RestoreTPL (OldTpl);

	int err;
	do {
        thread_yield();
		err = sem_wait(&sem->sem);
	} while (err < 0);
}

static struct lkl_mutex *mutex_alloc(void)
{
	struct lkl_mutex *_mutex = AllocatePool(sizeof(struct lkl_mutex));
	mutex_t *mutex = NULL;

	if (!_mutex)
		return NULL;

	mutex = &_mutex->mutex;

	mutex_init(mutex);

	return _mutex;
}

static void mutex_lock(struct lkl_mutex *mutex)
{
	mutex_acquire(&mutex->mutex);
}

static void mutex_unlock(struct lkl_mutex *_mutex)
{
	mutex_t *mutex = &_mutex->mutex;
	mutex_release(mutex);
}

static void mutex_free(struct lkl_mutex *_mutex)
{
	mutex_t *mutex = &_mutex->mutex;
	mutex_destroy(mutex);
	FreePool(_mutex);
}

#define MS2100N(x) ((x)*(1000000/100))
STATIC EFI_EVENT mTimerEvent;
STATIC volatile lk_time_t ticks = 0;

VOID
EFIAPI
TimerCallback (
  IN  EFI_EVENT   Event,
  IN  VOID        *Context
  )
{
    ticks += 10;

    // as long as linux doesn't do any long-running tasks this is totally fine
    thread_yield();
}
lk_time_t current_time(void)
{
    return ticks;
}

lk_bigtime_t current_time_hires(void)
{
    lk_bigtime_t time;

    time = (lk_bigtime_t)ticks * 1000;
    return time;
}

void lkl_thread_init(void)
{
    EFI_STATUS Status;

    thread_init_early();
    thread_init();
    thread_create_idle();
    thread_set_priority(DEFAULT_PRIORITY);

    Status = gBS->CreateEvent (EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_CALLBACK, TimerCallback, NULL, &mTimerEvent);
    ASSERT_EFI_ERROR (Status);

    Status = gBS->SetTimer (mTimerEvent, TimerPeriodic, MS2100N(10));
    ASSERT_EFI_ERROR (Status);
}

static lkl_thread_t lkl_thread_create(void (*fn)(void *), void *arg)
{
	thread_t* thread = thread_create("lkl", (int (*)(void *))fn, arg, DEFAULT_PRIORITY, 2*1024*1024);
	if (!thread)
		return 0;
	else {
        thread_resume(thread);
		return (lkl_thread_t) thread;
    }
}

static void lkl_thread_detach(void)
{
	thread_detach(get_current_thread());
}

static void lkl_thread_exit(void)
{
	thread_exit(0);
}

static int lkl_thread_join(lkl_thread_t tid)
{
	if (thread_join((thread_t*)tid, NULL, INFINITE_TIME))
		return -1;
	else
		return 0;
}

static unsigned long long time_ns(void)
{
	return current_time_hires();
}

typedef struct {
    EFI_EVENT event;
    void (*fn)(void *);
    void* arg;
} ltimer_t;

VOID
EFIAPI
LTimerCallback (
  IN  EFI_EVENT   Event,
  IN  VOID        *Context
  )
{
  ltimer_t* timer = Context;
  timer->fn(timer->arg);
}

static void *timer_alloc(void (*fn)(void *), void *arg)
{
    EFI_STATUS Status;

    ltimer_t* timer = AllocatePool(sizeof(ltimer_t));
    ASSERT(timer);
    timer->fn = fn;
    timer->arg = arg;

    Status = gBS->CreateEvent (EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_CALLBACK, LTimerCallback, timer, &timer->event);
    ASSERT_EFI_ERROR (Status);

    return timer;
}

static int timer_set_oneshot(void *_timer, unsigned long ns)
{
    EFI_STATUS Status;
    ltimer_t* timer = _timer;

    Status = gBS->SetTimer (timer->event, TimerRelative, ns/100);
    ASSERT_EFI_ERROR (Status);
    return 0;
}

static void timer_free(void *_timer)
{
    EFI_STATUS Status;
    ltimer_t* timer = _timer;

    Status = gBS->CloseEvent (timer->event);
    ASSERT_EFI_ERROR (Status);
}

static void lkl_panic(void)
{
	ASSERT(0);
}

static long _gettid(void)
{
	return (long)get_current_thread();
}

static void *lkl_mem_alloc(unsigned long size) {
    return AllocatePool(size);
}

struct lkl_host_operations lkl_host_ops = {
	.panic = lkl_panic,
	.thread_create = lkl_thread_create,
	.thread_detach = lkl_thread_detach,
	.thread_exit = lkl_thread_exit,
	.thread_join = lkl_thread_join,
	.sem_alloc = sem_alloc,
	.sem_free = sem_free,
	.sem_up = sem_up,
	.sem_down = sem_down,
	.mutex_alloc = mutex_alloc,
	.mutex_free = mutex_free,
	.mutex_lock = mutex_lock,
	.mutex_unlock = mutex_unlock,
	.time = time_ns,
	.timer_alloc = timer_alloc,
	.timer_set_oneshot = timer_set_oneshot,
	.timer_free = timer_free,
	.print = print,
	.mem_alloc = lkl_mem_alloc,
	.mem_free = FreePool,
	.ioremap = lkl_ioremap,
	.iomem_access = lkl_iomem_access,
	.virtio_devices = lkl_virtio_devs,
	.gettid = _gettid,
};

static int uefi_blk_get_capacity(struct lkl_disk disk, unsigned long long *res)
{
    LKL_VOLUME *Volume = disk.handle;

	*res = (unsigned long long)((Volume->BlockIo->Media->LastBlock+1) * Volume->BlockIo->Media->BlockSize);
	return 0;
}

static int do_rw(LKL_VOLUME *Volume, EFI_DISK_READ fn, struct lkl_disk disk, struct lkl_blk_req *req)
{
	INT64 off = req->sector * 512;
	void *addr;
	int len;
	int i;
	int ret = 0;
    EFI_STATUS Status;

	for (i = 0; i < req->count; i++) {

		addr = req->buf[i].iov_base;
		len = req->buf[i].iov_len;

		Status = fn(Volume->DiskIo, Volume->MediaId, off, len, addr);
		if (EFI_ERROR(Status)) {
			ret = -1;
			goto out;
		}

		addr += len;
		off += len;
	}

out:
	return ret;
}

static int uefi_blk_request(struct lkl_disk disk, struct lkl_blk_req *req)
{
    LKL_VOLUME *Volume = disk.handle;
	int err = 0;

	switch (req->type) {
	case LKL_DEV_BLK_TYPE_READ:
		err = do_rw(Volume, Volume->DiskIo->ReadDisk, disk, req);
		break;
	case LKL_DEV_BLK_TYPE_WRITE:
		err = do_rw(Volume, Volume->DiskIo->WriteDisk, disk, req);
		break;
	case LKL_DEV_BLK_TYPE_FLUSH:
	case LKL_DEV_BLK_TYPE_FLUSH_OUT:
        Volume->BlockIo->FlushBlocks(Volume->BlockIo);
		break;
	default:
		return LKL_DEV_BLK_STATUS_UNSUP;
	}

	if (err < 0)
		return LKL_DEV_BLK_STATUS_IOERR;

	return LKL_DEV_BLK_STATUS_OK;
}

struct lkl_dev_blk_ops lkl_dev_blk_ops = {
	.get_capacity = uefi_blk_get_capacity,
	.request = uefi_blk_request,
};
