/******************************************************************************/
/* Important CSCI 402 usage information:                                      */
/*                                                                            */
/* This fils is part of CSCI 402 kernel programming assignments at USC.       */
/*         53616c7465645f5f2e8d450c0c5851acd538befe33744efca0f1c4f9fb5f       */
/*         3c8feabc561a99e53d4d21951738da923cd1c7bbd11b30a1afb11172f80b       */
/*         984b1acfbbf8fae6ea57e0583d2610a618379293cb1de8e1e9d07e6287e8       */
/*         de7e82f3d48866aa2009b599e92c852f7dbf7a6e573f1c7228ca34b9f368       */
/*         faaef0c0fcf294cb                                                   */
/* Please understand that you are NOT permitted to distribute or publically   */
/*         display a copy of this file (or ANY PART of it) for any reason.    */
/* If anyone (including your prospective employer) asks you to post the code, */
/*         you must inform them that you do NOT have permissions to do so.    */
/* You are also NOT permitted to remove or alter this comment block.          */
/* If this comment block is removed or altered in a submitted file, 20 points */
/*         will be deducted.                                                  */
/******************************************************************************/

/*
 * Author:      William Chia-Wei Cheng (bill.cheng@acm.org)
 *
 * @(#)$Id: listtest.c,v 1.2 2015/12/30 01:30:27 william Exp $
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include "cs402.h"

#include "my402list.h"

int  My402ListLength(My402List* elem) {

   if(elem==NULL || ((*elem).anchor.next == &((*elem).anchor))){
	return 0;
   }
   
   return (*elem).num_members;
   
}

int  My402ListEmpty(My402List* elem) {

   if(elem==NULL || ((*elem).anchor.next == &((*elem).anchor))){
	return TRUE;
   }
   return FALSE;
}

 int  My402ListAppend(My402List* elem, void* obj) {
   if(elem!=NULL && (My402ListEmpty(elem))){
	My402ListElem *newElement = (My402ListElem *) malloc(sizeof(My402ListElem));
	if(newElement!=NULL) {
	    (*newElement).obj = obj;
	    (*newElement).next = &((*elem).anchor);
	    (*elem).anchor.next = newElement;
	    (*newElement).prev = &((*elem).anchor); 
	    (*elem).anchor.prev = newElement;
	    (*elem).num_members = (*elem).num_members +1;
	    return TRUE;
	}
   } else{
	My402ListElem *newElement = (My402ListElem *) malloc(sizeof(My402ListElem));
	if(newElement!=NULL) {
	    My402ListElem *lastElement = NULL;
	    lastElement = My402ListLast(elem);
	    (*newElement).obj = obj;
	    (*newElement).next = &((*elem).anchor);
	    (*newElement).prev = lastElement;
	    (*lastElement).next = newElement;
	    (*elem).anchor.prev = newElement;
	    (*elem).num_members = (*elem).num_members +1;
	    return TRUE;
	}
   }
   return FALSE;
 }

 int  My402ListPrepend(My402List* elem, void* obj) {
    if(elem!=NULL && (My402ListEmpty(elem))){
	My402ListElem *newElement = (My402ListElem *) malloc(sizeof(My402ListElem));
	if(newElement!=NULL) {
	    (*newElement).obj = obj;
	    (*newElement).next = &((*elem).anchor);
	    (*elem).anchor.next = newElement;
	    (*newElement).prev = &((*elem).anchor); 
	    (*elem).anchor.prev = newElement;
	    (*elem).num_members = (*elem).num_members +1;
	    return TRUE;
	}
   } else{
	My402ListElem *newElement = (My402ListElem *) malloc(sizeof(My402ListElem));
	if(newElement!=NULL) {
	    My402ListElem *firstElement = NULL;
	    firstElement = My402ListFirst(elem);
	    (*newElement).obj = obj;
	    (*newElement).next = firstElement;
	    (*newElement).prev = &((*elem).anchor);
	    (*firstElement).prev = newElement;
	    (*elem).anchor.next = newElement;
	    (*elem).num_members = (*elem).num_members +1;
	    return TRUE;
	}
   }
   return FALSE;
 }

void My402ListUnlink(My402List* elem1, My402ListElem* elem) {
    if(elem1!=NULL && elem!=NULL) {
    	
	    (*(*elem).prev).next = (*elem).next;
	    (*(*elem).next).prev = (*elem).prev;
	    (*elem).prev = NULL;
	    (*elem).next = NULL;
	    (*elem).obj = NULL;
	    free(elem);
	    (*elem1).num_members = (*elem1).num_members -1;
    }
}

void My402ListUnlinkAll(My402List* elem) {
    if(elem!=NULL) {
	My402ListElem *currentElement = NULL;
	My402ListElem *nextElement = NULL;
	currentElement = My402ListFirst(elem);
	while((*currentElement).next != &((*elem).anchor)){
	    nextElement = My402ListNext(elem, currentElement);
	    My402ListUnlink(elem, currentElement);
	    if(nextElement!= NULL)
	    	currentElement = nextElement;
	    else
		break;
	
	}
    	My402ListUnlink(elem, ((*elem).anchor).next);
	
    }
}

int  My402ListInsertAfter(My402List* elem1, void* obj, My402ListElem* elem) {
    if(elem==NULL){
	if(elem1!=NULL){
	    if(My402ListAppend(elem1,obj))
		return TRUE;
	}
    }
    if(elem!=NULL && elem1!=NULL) {
	My402ListElem *currentElement = NULL;
   	currentElement = My402ListFirst(elem1);
        while(currentElement!=NULL && currentElement != elem && (*currentElement).next != &((*elem1).anchor)) 
	    currentElement = My402ListNext(elem1, currentElement);
	if(currentElement == elem) {
   	    My402ListElem *newElement = (My402ListElem *) malloc(sizeof(My402ListElem));
	    if(newElement!=NULL) {	
	   	(*newElement).obj = obj;
	    	(*newElement).next = (*currentElement).next;
	    	(*(*currentElement).next).prev = newElement;
	    	(*newElement).prev = currentElement;
	    	(*currentElement).next = newElement;
	    	(*elem1).num_members = (*elem1).num_members +1;
	    	return TRUE;
	    }
	}
    }
    return FALSE;
 }
 
int  My402ListInsertBefore(My402List* elem1, void* obj, My402ListElem* elem) {
    if(elem==NULL){
	if(elem1!=NULL){
	    if(My402ListPrepend(elem1,obj))
		return TRUE;
	}
    }
    if(elem!=NULL && elem1!=NULL) {
	My402ListElem *currentElement = NULL;
   	currentElement = My402ListFirst(elem1);
        while(currentElement!=NULL && currentElement != elem && (*currentElement).next != &((*elem1).anchor)) 
	    currentElement = My402ListNext(elem1, currentElement);
	if(currentElement!=NULL && currentElement == elem) {
   	    My402ListElem *newElement = (My402ListElem *) malloc(sizeof(My402ListElem));
	    if(newElement!=NULL) {	
	    	(*newElement).obj = obj;
	    	(*newElement).next = currentElement;
	    	(*(*currentElement).prev).next = newElement;
	    	(*newElement).prev = (*currentElement).prev;
	    	(*currentElement).prev = newElement;
	    	(*elem1).num_members = (*elem1).num_members +1;
	    	return TRUE;
	    }
	}
    }
    return FALSE;
}

My402ListElem *My402ListFirst(My402List* elem) {
    
    if(elem!=NULL && ((*elem).anchor.next != &((*elem).anchor))){
	My402ListElem *firstElement = NULL;
	firstElement = (*elem).anchor.next;
	return firstElement;
    }
    return NULL;
}
My402ListElem *My402ListLast(My402List* elem) {
    
    if(elem!=NULL && ((*elem).anchor.next != &((*elem).anchor))){
	My402ListElem *lastElement = NULL;
	lastElement = (*elem).anchor.prev;
	return lastElement;
    }
    return NULL;
}

My402ListElem *My402ListNext(My402List* elem1, My402ListElem* elem) {
    if(elem==NULL)
	return NULL;
    My402ListElem *currentElement = NULL;
    currentElement = My402ListFirst(elem1);
    while(currentElement!=NULL && currentElement != elem && (*currentElement).next != &((*elem1).anchor)){
	currentElement = (*currentElement).next;
    }
    if(currentElement == elem){
	if((*currentElement).next == &((*elem1).anchor))
	    return NULL;
	else
	    return (*currentElement).next;
    }
    return NULL;
}

My402ListElem *My402ListPrev(My402List* elem1, My402ListElem* elem) {
    if(elem==NULL)
	return NULL;
    My402ListElem *currentElement = NULL;
    currentElement = My402ListFirst(elem1);
    while(currentElement!=NULL && currentElement != elem && (*currentElement).next != &((*elem1).anchor)){
	currentElement = My402ListNext(elem1, currentElement);
    }
    if(currentElement == elem){
	if((*currentElement).prev == &((*elem1).anchor))
	    return NULL;
	else
	    return (*currentElement).prev;
    }
    return NULL;
}

My402ListElem *My402ListFind(My402List* elem, void* obj) {
    My402ListElem *currentElement = NULL;
    currentElement = My402ListFirst(elem);
    while(currentElement != &((*elem).anchor)) {
	if((*currentElement).obj==obj)
	    return currentElement;
	currentElement = My402ListNext(elem, currentElement);
    }
    return NULL;

}

int My402ListInit(My402List* elem) {
    
    if(elem!=NULL) {
	(*elem).anchor.next = &((*elem).anchor);
	(*elem).anchor.prev = &((*elem).anchor);
	(*elem).anchor.obj = NULL;
	(*elem).num_members = 0;
	return TRUE;
    }
    return FALSE;
}
