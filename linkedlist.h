/**************************************************************************************************************************
			DO NOT CHANGE THE CONTENTS OF THIS FILE FOR YOUR COURSEWORK. ONLY WORK WITH THE OFFICIAL VERSION
**************************************************************************************************************************/

#include <stdio.h>

/*
* Definition of a node of the linked list
*/
struct element
{
	void * pData;
	struct element * pNext;
};

/**
* This function adds an element oTemp to the end of the linked list. 
*/
void addLast(void * oTemp, struct element ** pHead, struct element ** pTail);

/**
* This function adds an element oTemp to the beginning of the linked list. 
*/
void addFirst(void * oTemp, struct element ** pHead, struct element ** pTail);

/**
* This function removes the first element of the linked list  
*/
void * removeFirst(struct element ** pHead, struct element ** pTail);
