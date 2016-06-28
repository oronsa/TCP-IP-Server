
//===========Author: Oron Sason=============//
//===========I.D: 303038129================//

#include <stdio.h>
#include <stdlib.h>
#include "threadpool.h"


threadpool* create_threadpool(int num_threads_in_pool)
{
	//not a valid size 
	if(num_threads_in_pool<=0 || num_threads_in_pool>MAXT_IN_POOL)
	{
		return NULL;
	}	
	//create the pool
	threadpool *t=(threadpool*)calloc(1,sizeof(threadpool));
	if(!t)
	{
		perror("allocate fail");
		return NULL;
	}

	t->threads=(pthread_t *)calloc(num_threads_in_pool,sizeof(pthread_t)*num_threads_in_pool);
	if(!t->threads)
	{
		perror("allocate fail");
		free(t);
		return NULL;
	}
	//init
	t->num_threads=num_threads_in_pool;
	t->qhead=NULL;
	t->qtail=NULL;
	t->qsize=0;
	t->dont_accept=0;
	t->shutdown=0;
	
	if(pthread_mutex_init(&(t->qlock),NULL)!=0)
	{
		if(t)
			free(t);
		if(t->threads)
			free(t->threads);
		fprintf(stderr,"mutex initiation  fail");
		return NULL;	
	}
	if(pthread_cond_init(&(t->q_empty),NULL)!=0)
	{
		if(t)
			free(t);
		if(t->threads)
			free(t->threads);
		fprintf(stderr,"the initiation fail");	
		return NULL;
	}	
	if(pthread_cond_init(&(t->q_not_empty),NULL)) 
	{
		if(t)
			free(t);
		if(t->threads)
			free(t->threads);
    		fprintf(stderr, "the initiation fail!\n");	
		return NULL;
 	}
 	//loop create 
  	int i=0;
	while(i<t->num_threads)
	{
		if((pthread_create(&(t->threads[i]),NULL ,do_work,t))!=0)
		{
			printf("Thread initiation error!!!\n");
			return NULL;	
		}
		i++;
	}
     
    return t; 
}
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg)
{
	
	if(!from_me || !dispatch_to_here)
		return;

	threadpool *t= from_me;
	if(t->dont_accept) //destroy function has begun
	{
		return;
	}
	pthread_mutex_lock(&t->qlock);//the thread in control
	work_t *work=(work_t*)calloc(1,sizeof(work_t));
	if(!work)
	{
		perror("allocate fail");
		return ;
	}
	work->routine=dispatch_to_here; //work function
	work->arg=arg;		//the argument of the function
	work->next=NULL;	//null for next work 
	
	if(t->qsize==0)
	{
		t->qhead=work;
		t->qtail=work;
	}
	else
	{
		t->qtail->next=work;
		t->qtail=work;
		
	}
	t->qsize++;	//any case the thread get into the pool update size 
	pthread_cond_signal(&(t->q_not_empty));//the pool is not empty	
	pthread_mutex_unlock(&(t->qlock));  //unlock the queue	

	return;
}
void* do_work(void* p)
{
	if(p==NULL)
		return NULL;
	threadpool *t=p;
	work_t *work;
	while(1)//run an endless loop
	{
		pthread_mutex_lock(&(t->qlock)); 

			if(t->shutdown) //If destruction process has begun
			{
				pthread_mutex_unlock(&(t->qlock));
				return 0;
			}
			//the queue is empty
			while(t->qsize==0 && t->shutdown==0)
			{
				//pthread_mutex_unlock(&(t->qlock));  //get the qlock.
				pthread_cond_wait(&(t->q_not_empty),&(t->qlock));
			}
			//Check again destruction flag
			if(t->shutdown) 
			{
				pthread_mutex_unlock(&(t->qlock));
				return 0;													       
	
			}

		work=t->qhead; //take the first elemnent from the queue
		t->qsize--; 	//update pool size 
		if(t->qsize==0)	//the queue becomes empty
		{
			t->qtail=NULL;
			t->qhead=NULL;
			pthread_cond_signal(&(t->q_empty));//signal destruction process.
		}
		else	//there is more works
		{
			t->qhead=work->next;
		}
		if(t->qsize==0 && t->dont_accept)//the q empty again
		{
			pthread_cond_signal(&(t->q_empty));//signal destruction process

		}		
		pthread_mutex_unlock(&(t->qlock));
		work->routine(work->arg); //do the work
		free(work);// to free the work after finish
	}
	return 0;
}
void destroy_threadpool(threadpool *destroyme)
{

	
	if(destroyme==NULL)
		return;
	threadpool *t=destroyme;

	pthread_mutex_lock(&(t->qlock));
	
	t->dont_accept = 1; //destroy function has begun flag
	
	while(t->qsize != 0) //Wait for queue to become empty
	{
		pthread_cond_wait(&(t->q_empty),&(t->qlock)); 
	}
	
	t->shutdown=1;
	pthread_cond_broadcast(&(t->q_not_empty));  //allow code to return NULL;
	pthread_mutex_unlock(&(t->qlock));
	int i=0;
	while(i<t->num_threads)
	{
		pthread_join(t->threads[i],NULL);
		i++;
	}
	free(t->threads);	//destroy threads in pool
	
	//======destroy mutex and cond==========//

	pthread_mutex_destroy(&(t->qlock));
	pthread_cond_destroy(&(t->q_empty));
	pthread_cond_destroy(&(t->q_not_empty));
	free(t);		//destroy pool	
}
