#include "List.h"

struct TestNode
{
	int value;
	ListHead link;
};

static void InitNode(TestNode* node, int value)
{
	node->value = value;
	List::Init(&node->link);
}

static bool CheckOrder(ListHead* head, const int* values, int count)
{
	int index = 0;
	ListHead* pos = NULL;

	LIST_FOR_EACH(pos, head)
	{
		if ( index >= count )
		{
			return false;
		}

		TestNode* node = LIST_ENTRY(pos, TestNode, link);
		if ( node->value != values[index] )
		{
			return false;
		}
		++index;
	}

	return index == count;
}

static bool TestAddDelete()
{
	LIST_HEAD(head);
	TestNode first;
	TestNode second;
	TestNode third;

	InitNode(&first, 1);
	InitNode(&second, 2);
	InitNode(&third, 3);

	if ( List::Empty(&head) == false || List::IsSingular(&head) )
	{
		return false;
	}

	List::Add(&second.link, &head);
	List::Add(&first.link, &head);
	List::AddTail(&third.link, &head);

	const int expected1[] = { 1, 2, 3 };
	if ( CheckOrder(&head, expected1, 3) == false )
	{
		return false;
	}

	if ( List::IsFirst(&first.link, &head) == false || List::IsLast(&third.link, &head) == false )
	{
		return false;
	}

	List::Delete(&second.link);
	if ( second.link.next != NULL || second.link.prev != NULL )
	{
		return false;
	}

	const int expected2[] = { 1, 3 };
	if ( CheckOrder(&head, expected2, 2) == false )
	{
		return false;
	}

	List::DeleteInit(&first.link);
	if ( first.link.next != &first.link || first.link.prev != &first.link )
	{
		return false;
	}

	if ( List::IsSingular(&head) == false )
	{
		return false;
	}

	return LIST_FIRST_ENTRY(&head, TestNode, link)->value == 3
		&& LIST_LAST_ENTRY(&head, TestNode, link)->value == 3;
}

static bool TestMoveReplace()
{
	LIST_HEAD(head1);
	LIST_HEAD(head2);
	TestNode first;
	TestNode second;
	TestNode third;
	TestNode replacement;

	InitNode(&first, 1);
	InitNode(&second, 2);
	InitNode(&third, 3);
	InitNode(&replacement, 4);

	List::AddTail(&first.link, &head1);
	List::AddTail(&second.link, &head1);
	List::AddTail(&third.link, &head2);

	List::Move(&third.link, &head1);
	const int expected1[] = { 3, 1, 2 };
	if ( CheckOrder(&head1, expected1, 3) == false || List::Empty(&head2) == false )
	{
		return false;
	}

	List::MoveTail(&third.link, &head1);
	const int expected2[] = { 1, 2, 3 };
	if ( CheckOrder(&head1, expected2, 3) == false )
	{
		return false;
	}

	List::Replace(&second.link, &replacement.link);
	const int expected3[] = { 1, 4, 3 };
	if ( CheckOrder(&head1, expected3, 3) == false )
	{
		return false;
	}

	List::ReplaceInit(&replacement.link, &second.link);
	const int expected4[] = { 1, 2, 3 };
	if ( CheckOrder(&head1, expected4, 3) == false )
	{
		return false;
	}

	return replacement.link.next == &replacement.link
		&& replacement.link.prev == &replacement.link;
}

static bool TestSpliceAndSafeIterate()
{
	LIST_HEAD(dst);
	LIST_HEAD(src);
	TestNode first;
	TestNode second;
	TestNode third;
	ListHead* pos = NULL;
	ListHead* temp = NULL;

	InitNode(&first, 1);
	InitNode(&second, 2);
	InitNode(&third, 3);

	List::AddTail(&first.link, &dst);
	List::AddTail(&second.link, &src);
	List::AddTail(&third.link, &src);
	List::SpliceTailInit(&src, &dst);

	const int expected1[] = { 1, 2, 3 };
	if ( CheckOrder(&dst, expected1, 3) == false || List::Empty(&src) == false )
	{
		return false;
	}

	LIST_FOR_EACH_SAFE(pos, temp, &dst)
	{
		TestNode* node = LIST_ENTRY(pos, TestNode, link);
		if ( node->value != 2 )
		{
			List::DeleteInit(pos);
		}
	}

	const int expected2[] = { 2 };
	return CheckOrder(&dst, expected2, 1) && List::IsSingular(&dst);
}

int main()
{
	if ( TestAddDelete() == false )
	{
		return 1;
	}

	if ( TestMoveReplace() == false )
	{
		return 2;
	}

	if ( TestSpliceAndSafeIterate() == false )
	{
		return 3;
	}

	return 0;
}
