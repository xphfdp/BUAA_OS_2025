#include <stdio.h>

int huiwenshu(int n) {
	int array[10] = {0};
	int i = 0;
	while(n > 0) {
		array[i] = n % 10;
		n = n / 10;
		i++;
	}
	for(int j =0;j < i;j++) {
		if(array[j] != array[i - j -1]) {
			return 0;
		}
	}
	return 1;
}
int main() {
	int n;
	scanf("%d", &n);
	
	if (huiwenshu(n) == 1) {
		printf("Y\n");
	} else {
		printf("N\n");
	}
	return 0;
}
