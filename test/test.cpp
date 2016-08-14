#include <stdio.h>
#include <assert.h>
#include <Windows.h>
#include <process.h>

class base{
public:
	base(){
		puts("base constructor");
	}

	virtual ~base(){
		puts("base destroy");
	}

	virtual void show(void){
		puts("base show");
	}
};

class child : public base{
public:
	child() :base(){
		puts("child constructor");
	}

	virtual ~child(){
		puts("child destroy");
	}

	virtual void show(){
		puts("child show");
	}
};

void shellSort(int * arr, int num){
	for (int gap = num / 2; gap > 0; gap /= 2){
		for (int d = gap; d < num; ++d){
			for (int k = d - gap; k >= 0 && arr[k] > arr[k + gap]; k -= gap){
				int temp = arr[k];
				arr[k] = arr[k + gap];
				arr[k + gap] = temp;
			}
		}
	}
}

int main(void){
	int arr[10] = { 29, 6, 12, 34, 65, 1, 0, 33, 78, 11 };
	shellSort(arr, 10);
	for (int i = 0; i < 10; ++i){
		printf("%d ", arr[i]);
	}
	return 0;
}