#include "my_thread_pool.h"

/**
 * @function:线程清理函数，防止线程带锁退出
 */
void handler(void *arg){
	pthread_mutex_unlock((pthread_mutex_t *)arg);//解锁
}

/**
 * @function:任务执行函数
 * @param arg:函数参数
 */
void *routine(void *arg){
	#ifdef DEBUG
	printf("[%u] is started.\n",
		(unsigned)pthread_self());
	#endif

	thread_pool *pool = (thread_pool *)arg;//参数其实是个线程池头结点
	struct task *p=NULL;

	while(1)
	{
		/**************************************************/
        //注册一个线程清理函数，防止死锁
		pthread_cleanup_push(handler, (void *)&pool->lock);
		pthread_mutex_lock(&pool->lock);
		/**************************************************/

		// 1, 没有任务，并且shutdown标志没有打开,就等
		while(pool->waiting_tasks == 0 && !pool->shutdown)
		{
			pthread_cond_wait(&pool->cond, &pool->lock);
            //没任务但线程池没有关的时候会阻塞在这里，唤醒的地方有两个
			//一个是在添加任务时，另一个是在销毁线程池时
		}

		// 2, 没有任务, 并且shutdown标志打开了，就释放锁并退出
		if(pool->waiting_tasks == 0 && pool->shutdown == true)
		{
			pthread_mutex_unlock(&pool->lock);
			pthread_exit(NULL);
		}

		// 3, 有任务,就执行任务
		p = pool->task_list->next;//第一个任务结点并没有赋值，所以可以跳过它
		pool->task_list->next = p->next;//更新pool->task_list->next
		pool->waiting_tasks--;

		/**************************************************/
		pthread_mutex_unlock(&pool->lock);
		pthread_cleanup_pop(0);
		/**************************************************/

		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
		(p->do_task)(p->arg);//执行任务
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        if(p != NULL){
		    free(p);//释放掉以被执行的任务 结点的空间
            p=NULL;
        }
	}

	pthread_exit(NULL);
}

/**
 * @function:初始化线程池链表，并一个创建线程来运行线程池管理函数
 * @param pool:指向要初始化的线程池
 * @param threads_number:线程里常驻线程数
 */
bool init_pool(thread_pool *pool, unsigned int threads_number){
	pthread_mutex_init(&pool->lock, NULL);
	pthread_cond_init(&pool->cond, NULL);

	pool->shutdown = false;//开机
	pool->task_list = malloc(sizeof(struct task));//创建第一个任务结点并用task_list指向它，注意第一个任务结点并没有赋值，why?
	pool->tids = malloc(sizeof(pthread_t) * MAX_ACTIVE_THREADS);

	if(pool->task_list == NULL || pool->tids == NULL)
	{
		perror("allocate memory error");
		return false;
	}

	pool->task_list->next = NULL;

	pool->max_waiting_tasks = MAX_WAITING_TASKS;
	pool->waiting_tasks = 0;
	pool->active_threads = threads_number;

	int i;
	for(i=0; i < pool->active_threads; i++){//创建active_threads个线程来运行任务执行函数
		if(pthread_create(&((pool->tids)[i]), NULL,
					routine, (void *)pool) != 0){
			perror("create threads error");
			return false;
		}

		#ifdef DEBUG
		printf("[%u]:[%s] ==> tids[%d]: [%u] is created.\n",
			(unsigned)pthread_self(), __FUNCTION__,
			i, (unsigned)pool->tids[i]);
		#endif
	}

	return true;
}
/**
 * @function:添加任务
 * @param pool:线程池结构地址
 * @param do_task():函数指针
 */
bool add_task(thread_pool *pool,void *(*do_task)(void *arg), void *arg){
	struct task *new_task = malloc(sizeof(struct task));//创建一个新的任务结点
	if(new_task == NULL){
		perror("allocate memory error");
		return false;
	}
	new_task->do_task = do_task;  //给新的任务结点赋值
	new_task->arg = arg;
	new_task->next = NULL;

	//============ LOCK =============//
	pthread_mutex_lock(&pool->lock);
	//===============================//

	if(pool->waiting_tasks >= MAX_WAITING_TASKS)//如果等待任务数量达到上限
	{
		pthread_mutex_unlock(&pool->lock);

		fprintf(stderr, "too many tasks.\n");
		free(new_task);

		return false;//放弃添加，结束函数
	}
	
	struct task *tmp = pool->task_list;
	while(tmp->next != NULL)
	{
		tmp = tmp->next;//找到最后一个结点
	}

	tmp->next = new_task;
	pool->waiting_tasks++;

	//=========== UNLOCK ============//
	pthread_mutex_unlock(&pool->lock);
	//===============================//

	#ifdef DEBUG
	printf("[%u][%s] ==> a new task has been added.\n",
		(unsigned)pthread_self(), __FUNCTION__);
	#endif

	pthread_cond_signal(&pool->cond);//唤醒条件变量
	return true;
}

/**
 * @function:创建活动线程
 * @param pool:线程池结构地址
 * @param additional_threads:线程的个数
 */
int add_thread(thread_pool *pool, unsigned additional_threads){
	if(additional_threads == 0)
		return 0;

	unsigned total_threads =
			pool->active_threads + additional_threads;

	int i, actual_increment = 0;
	for(i = pool->active_threads;i<total_threads && i < MAX_ACTIVE_THREADS;i++){
		if(pthread_create(&((pool->tids)[i]),NULL, routine, (void *)pool) != 0){
			perror("add threads error");
			if(actual_increment == 0)
				return -1;
			break;
		}
		actual_increment++; 

		#ifdef DEBUG
		printf("[%u]:[%s] ==> tids[%d]: [%u] is created.\n",
			(unsigned)pthread_self(), __FUNCTION__,
			i, (unsigned)pool->tids[i]);
		#endif
	}

	pool->active_threads += actual_increment;
	return actual_increment;
}

/**
 * @function:删除线程
 * @param pool:线程池结构地址
 * @param removing_threads:线程的个数
 */
int remove_thread(thread_pool *pool, unsigned int removing_threads)
{
	if(removing_threads == 0)
		return pool->active_threads;

	int remaining_threads = pool->active_threads - removing_threads;
	remaining_threads = remaining_threads > 0 ? remaining_threads : 1;//为什么要留1，why?

	int i;
	for(i = pool->active_threads - 1; i > remaining_threads - 1; i--)//我们的目的就缓解硬件资源紧张而不是结束任务
	{
		errno = pthread_cancel(pool->tids[i]);

		if(errno != 0)
			break;

		#ifdef DEBUG
		printf("[%u]:[%s] ==> cancelling tids[%d]: [%u]...\n",
			(unsigned)pthread_self(), __FUNCTION__,
			i, (unsigned)pool->tids[i]);
		#endif
	}

	if(i == pool->active_threads-1)
		return -1;
	else
	{
		pool->active_threads = i+1;
		return i+1;
	}
}
/**
 * @function:销毁线程池
 */
bool destroy_pool(thread_pool *pool)
{
	// 1, activate all threads
	pool->shutdown = true;
	pthread_cond_broadcast(&pool->cond);

	// 2, wait for their exiting
	int i;
	for(i=0; i < pool->active_threads; i++)
	{
		errno = pthread_join(pool->tids[i], NULL);
		if(errno != 0)
		{
			printf("join tids[%d] error: %s\n",
					i, strerror(errno));
		}
	}

	// 3, free memories
	free(pool->task_list);
	free(pool->tids);
	free(pool);

	return true;
}
void *print(void *arg){
	printf("\rserv$ %s\n", (char *)arg);
    return ((void *)0);
}









