#pragma once

#define QUEUE_MAX 100

template <typename T>
class Queue
{
private:
	int f;
	int r;
	T Data[QUEUE_MAX];
public:
	Queue();
	~Queue();
	void Enqueue(T Message);
	T Dequeue();
	T PeekQ(int PeekPos);
	bool QIsFull();
	T FindMaxData();
};

template <typename T>
Queue<T>::Queue()
{
	f = 0;
	r = 0;
}
template <typename T>
Queue<T>::~Queue()
{
	
}

template <typename T>
void Queue<T>::Enqueue(T Message)
{
	Data[f] = Message;
	f++;
	if (f == QUEUE_MAX)
		f = 0;
}

template <typename T>
T Queue<T>::Dequeue()
{
	if (f == r)
		return -1;
	T iTemp = Data[r];
	r++;
	if (r == QUEUE_MAX)
		r = 0;
	return iTemp;
}

template <typename T>
T Queue<T>::PeekQ(int PeekPos)
{
	int Pos = PeekPos + r;
	if (Pos >= QUEUE_MAX)
		Pos -= QUEUE_MAX;
	else if (Pos < 0)
		return 0;

	return Data[Pos];
}

template <typename T>
bool Queue<T>::QIsFull()
{
	if (f + 1 == r || (f + 1 == QUEUE_MAX && r == 0))
		return true;
	return false;
}

template <typename T>
T Queue<T>::FindMaxData()
{
	T MaxData = 0;
	int iCnt = r;
	for (int i = 0; i < QUEUE_MAX; ++i)
	{
		if (iCnt == QUEUE_MAX)
			iCnt = 0;
		else if (iCnt == f)
			break;
		if (MaxData < Data[iCnt])
			MaxData = Data[iCnt];
		++iCnt;
	}

	return MaxData;
}