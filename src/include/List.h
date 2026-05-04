#ifndef LIST_H
#define LIST_H

#include "Utility.h"

/*
 * 侵入式双向链表。
 * 设计保持 Linux 内核 list_head 的核心语义：
 * 1) 结点嵌入宿主对象，不额外分配内存；
 * 2) 头结点本身也是一个哨兵结点；
 * 3) 空链表满足 head->next == head 且 head->prev == head。
 */
struct ListHead
{
	ListHead* next;
	ListHead* prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) ListHead name = LIST_HEAD_INIT(name)

#define LIST_OFFSET_OF(type, member) \
	((unsigned long)(&(((type*)0)->member)))

#define LIST_ENTRY(ptr, type, member) \
	((type*)((unsigned long)(ptr) - LIST_OFFSET_OF(type, member)))

#define LIST_FIRST_ENTRY(head, type, member) \
	LIST_ENTRY((head)->next, type, member)

#define LIST_LAST_ENTRY(head, type, member) \
	LIST_ENTRY((head)->prev, type, member)

#define LIST_NEXT_ENTRY(pos, type, member) \
	LIST_ENTRY((pos)->member.next, type, member)

#define LIST_PREV_ENTRY(pos, type, member) \
	LIST_ENTRY((pos)->member.prev, type, member)

#define LIST_FOR_EACH(pos, head) \
	for ( (pos) = (head)->next; (pos) != (head); (pos) = (pos)->next )

#define LIST_FOR_EACH_PREV(pos, head) \
	for ( (pos) = (head)->prev; (pos) != (head); (pos) = (pos)->prev )

#define LIST_FOR_EACH_SAFE(pos, temp, head) \
	for ( (pos) = (head)->next, (temp) = (pos)->next; \
		  (pos) != (head); \
		  (pos) = (temp), (temp) = (pos)->next )

class List
{
public:
	static void Init(ListHead* head)
	{
		head->next = head;
		head->prev = head;
	}

	static bool Empty(const ListHead* head)
	{
		return head->next == head;
	}

	static bool IsSingular(const ListHead* head)
	{
		return head->next != head && head->next == head->prev;
	}

	static bool IsFirst(const ListHead* node, const ListHead* head)
	{
		return node->prev == head;
	}

	static bool IsLast(const ListHead* node, const ListHead* head)
	{
		return node->next == head;
	}

	static void Add(ListHead* node, ListHead* head)
	{
		AddBetween(node, head, head->next);
	}

	static void AddTail(ListHead* node, ListHead* head)
	{
		AddBetween(node, head->prev, head);
	}

	static void Delete(ListHead* node)
	{
		DeleteBetween(node->prev, node->next);
		node->next = NULL;
		node->prev = NULL;
	}

	static void DeleteInit(ListHead* node)
	{
		DeleteBetween(node->prev, node->next);
		Init(node);
	}

	static void Replace(ListHead* oldNode, ListHead* newNode)
	{
		newNode->next = oldNode->next;
		newNode->next->prev = newNode;
		newNode->prev = oldNode->prev;
		newNode->prev->next = newNode;
	}

	static void ReplaceInit(ListHead* oldNode, ListHead* newNode)
	{
		Replace(oldNode, newNode);
		Init(oldNode);
	}

	static void Move(ListHead* node, ListHead* head)
	{
		DeleteBetween(node->prev, node->next);
		AddBetween(node, head, head->next);
	}

	static void MoveTail(ListHead* node, ListHead* head)
	{
		DeleteBetween(node->prev, node->next);
		AddBetween(node, head->prev, head);
	}

	static void Splice(ListHead* list, ListHead* head)
	{
		if ( Empty(list) )
		{
			return;
		}
		SpliceBetween(list, head, head->next);
	}

	static void SpliceTail(ListHead* list, ListHead* head)
	{
		if ( Empty(list) )
		{
			return;
		}
		SpliceBetween(list, head->prev, head);
	}

	static void SpliceInit(ListHead* list, ListHead* head)
	{
		Splice(list, head);
		Init(list);
	}

	static void SpliceTailInit(ListHead* list, ListHead* head)
	{
		SpliceTail(list, head);
		Init(list);
	}

private:
	static void AddBetween(ListHead* node, ListHead* prev, ListHead* next)
	{
		next->prev = node;
		node->next = next;
		node->prev = prev;
		prev->next = node;
	}

	static void DeleteBetween(ListHead* prev, ListHead* next)
	{
		next->prev = prev;
		prev->next = next;
	}

	static void SpliceBetween(ListHead* list, ListHead* prev, ListHead* next)
	{
		ListHead* first = list->next;
		ListHead* last = list->prev;

		first->prev = prev;
		prev->next = first;

		last->next = next;
		next->prev = last;
	}
};

#endif
