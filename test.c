#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define SIZE 10

typedef struct
{
	int lewy;
	int prawy;
} para;

typedef struct stos
{
	para *newP;
	struct stos *prev;
	struct stos *next;
} stosik;

stosik **moj;


void push(para *pakiet, int n)
{
	stosik *tmp = malloc(sizeof(stosik));
	tmp->newP = pakiet;

	tmp->next = 0;
	tmp->prev = 0;
	if (!moj[n])
		moj[n] = tmp;
	else
	{
		stosik *head = moj[n];
		while (head->next)
			head = head->next;
		tmp->prev = head;
		head->next = tmp;
	}
}

para *pop(int n)
{
	if (moj[n] == NULL)
	{
		return NULL;
	}
	else
	{
		para *tmp = moj[n]->newP;
		stosik *next = moj[n]->next;
		if (next)
			next->prev = 0;
		free(moj[n]);
		moj[n] = next;
		return tmp;
	}
}


int main(int argc, char **argv) {
	
	moj = malloc(sizeof(stosik *) * 8);
	memset(moj, 0, sizeof(stosik *) * 8);

	for(int i = 0; i < 100; i++) {
		para *tmp = (para *)malloc(sizeof(para));
		tmp->lewy = i;
		tmp->prawy = i*2;
		push(tmp, 0);
	}

	for(int i = 0; i < 12; i++) {
		para *tmp = (para *)malloc(sizeof(para));
		tmp->lewy = i;
		tmp->prawy = i*100;
		push(tmp, 1);
	}

	para *tmp_p=0;
	
	for (int i = 7; i >= 0; i--)
	{
		do
		{
			tmp_p = pop(i);
			if (tmp_p)
			{
				printf("%d // %d // %d\n", i, tmp_p->lewy, tmp_p->prawy);
				free(tmp_p);
			}
		} while (tmp_p);
	}
	free(moj);




	return 0;
}